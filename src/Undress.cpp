#include "Undress.h"

#include <algorithm>
#include <array>
#include <optional>
#include <mutex>
#include <unordered_map>

#include "Actions.h"
#include "Bridge.h"
#include "HUD.h"

namespace Undress
{
	namespace
	{
		struct State
		{
			std::uint64_t              handled = 0;  // bit (slot - 30): already taken off (or asked to)
			std::vector<std::uint32_t> items;        // what was taken off, to put back on
		};

		// A scene's animated undressing step, waiting for its time.
		struct Timed
		{
			std::uint32_t    actor = 0;
			std::vector<int> slots;
			float            remaining = 0.0F;
		};

		Config                                   g_config;
		std::vector<Timed>                       g_timed;  // main thread
		std::mutex                               g_lock;
		std::unordered_map<std::uint32_t, State> g_states;

		bool Allowed(int a_slot)
		{
			return std::ranges::find(g_config.slots, a_slot) != g_config.slots.end();
		}

		bool IsPlayer(std::uint32_t a_id)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			return player && player->GetFormID() == a_id;
		}

		constexpr std::uint32_t PIPBOY = 0x00021B3B;  // the Pip-Boy (an armor) never comes off

		// Redress animations, OStim's way: a body part at a time (torso, feet,
		// hands, head), each a one-actor scene tagged "redress" and the part,
		// for the actor's sex; the part's items go on at the scene's dressAt.
		struct RedressPart
		{
			const char*      tag;
			std::uint32_t    mask;     // bit (slot - 30)
			float            dressAt;  // when the scene doesn't say (OStim's)
		};
		constexpr std::uint32_t Bits(std::initializer_list<int> a_slots)
		{
			std::uint32_t mask = 0;
			for (const auto slot : a_slots) {
				mask |= 1u << (slot - 30);
			}
			return mask;
		}
		constexpr std::array REDRESS_PARTS{
			RedressPart{ "torso", Bits({ 33, 36, 37, 38, 41, 42, 43, 50 }), 1.5F },
			RedressPart{ "feet", Bits({ 39, 40, 44, 45 }), 2.9F },
			RedressPart{ "hands", Bits({ 34, 35 }), 1.6F },
			RedressPart{ "head", Bits({ 46, 47 }), 1.9F },
		};

		struct RedressAnim
		{
			std::int32_t idle = 0;
			float        length = 0.0F;
			float        dressAt = 0.0F;
		};

		bool HasTag(const SceneRegistry::Scene& a_scene, std::string_view a_tag)
		{
			return std::ranges::any_of(a_scene.tags, [&](const std::string& t) { return _stricmp(t.c_str(), std::string(a_tag).c_str()) == 0; });
		}

		// The redress scene for a_part and an actor of sex a_sex, if any.
		std::optional<RedressAnim> FindRedressAnim(const RedressPart& a_part, SceneRegistry::Sex a_sex)
		{
			SceneRegistry::ListFilter filter;
			filter.sexes = { a_sex };
			for (const auto& summary : SceneRegistry::List(1, filter)) {
				const auto scene = SceneRegistry::Find(summary.id);
				if (!scene || !HasTag(*scene, "redress") || !HasTag(*scene, a_part.tag) || scene->speeds.empty() || scene->speeds[0].empty() || !scene->speeds[0][0]) {
					continue;
				}
				const float length = scene->length > 0.0F ? scene->length : 3.0F;
				const float dressAt = scene->dressAt >= 0.0F ? scene->dressAt : std::min(a_part.dressAt, length);
				return RedressAnim{ static_cast<std::int32_t>(scene->speeds[0][0]->GetFormID()), length, std::min(dressAt, length) };
			}
			return std::nullopt;
		}

		// What a_actor wears with any of a_mask's slots (bit 0 = slot 30).
		// Main thread.
		std::vector<std::uint32_t> WornIn(RE::Actor* a_actor, std::uint32_t a_mask)
		{
			std::vector<std::uint32_t> items;
			const auto list = a_actor->inventoryList;
			if (!list) {
				return items;
			}
			for (auto& item : list->data) {
				const auto object = item.object;
				if (!object || object->GetFormType() != RE::ENUM_FORM_ID::kARMO || object->GetFormID() == PIPBOY) {
					continue;
				}
				bool equipped = false;
				for (auto stack = item.stackData.get(); stack && !equipped; stack = stack->nextStack.get()) {
					equipped = stack->IsEquipped();
				}
				if (equipped && (object->GetFilledSlots() & a_mask) && std::ranges::find(items, object->GetFormID()) == items.end()) {
					items.push_back(object->GetFormID());
				}
			}
			return items;
		}

		// Takes off what's in a_slots (those not taken off yet): finds the
		// items here, Papyrus unequips them. Any thread.
		void Strip(std::uint32_t a_id, const std::vector<int>& a_slots, bool a_byHand = false)
		{
			if (!a_byHand && (!g_config.enabled || (!g_config.player && IsPlayer(a_id)))) {
				return;
			}
			std::uint32_t mask = 0;
			{
				std::scoped_lock lock(g_lock);
				auto& state = g_states[a_id];
				for (const auto slot : a_slots) {
					if (slot < 30 || slot > 61 || !Allowed(slot)) {
						continue;
					}
					const auto bit = std::uint64_t{ 1 } << (slot - 30);
					if (!(state.handled & bit)) {
						state.handled |= bit;
						mask |= static_cast<std::uint32_t>(bit);
					}
				}
			}
			if (!mask) {
				return;
			}
			F4SE::GetTaskInterface()->AddTask([a_id, mask]() {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
				if (!actor) {
					return;
				}
				const auto items = WornIn(actor, mask);
				REX::INFO("Undress: {:08X}, slots 0x{:08X}: {} item(s) to take off", a_id, mask, items.size());
				if (items.empty()) {
					return;
				}
				NoteStripped(a_id, items);
				if (const auto vm = FourStim::GetVM()) {
					std::vector<std::int32_t> ids(items.begin(), items.end());
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					vm->DispatchStaticCall("FourStimUndress"sv, "Strip"sv, callback, static_cast<std::int32_t>(a_id), ids, g_config.itemDelay);
				}
				HUD::OnFocusedSceneChanged();  // the HUD's "Undress / Dress" entries
			});
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start)
	{
		// A new scene: steps left from the last one won't happen.
		std::erase_if(g_timed, [&](const Timed& t) { return std::ranges::find(a_ids, t.actor) != a_ids.end(); });
		if (!g_config.enabled || a_scene.noStrip) {
			return;
		}
		for (const auto& step : a_scene.undress) {
			if (step.actor < a_ids.size()) {
				g_timed.push_back({ a_ids[step.actor], step.slots, step.at });
			}
		}
		for (std::size_t role = 0; role < a_ids.size(); ++role) {
			if (a_start && g_config.atStart) {
				Strip(a_ids[role], g_config.slots);
				continue;
			}
			std::vector<int> slots;
			bool             full = false;
			for (const auto& action : a_scene.actions) {
				auto take = [&](std::size_t a_role, const Actions::Side& a_side) {
					if (a_role != role) {
						return;
					}
					if (a_side.fullStrip && g_config.fullMidScene) {
						full = true;
					} else if (g_config.partial) {
						slots.insert(slots.end(), a_side.undressSlots.begin(), a_side.undressSlots.end());
					}
				};
				take(action.actor, action.type->actor);
				take(action.target, action.type->target);
				take(action.performer, action.type->performer);
			}
			if (full) {
				Strip(a_ids[role], g_config.slots);
			} else if (!slots.empty()) {
				Strip(a_ids[role], slots);
			}
		}
	}

	void Tick(float a_seconds)
	{
		if (g_timed.empty()) {
			return;
		}
		std::vector<Timed> due;
		for (auto& t : g_timed) {
			t.remaining -= a_seconds;
			if (t.remaining <= 0.0F) {
				due.push_back(t);
			}
		}
		std::erase_if(g_timed, [](const Timed& t) { return t.remaining <= 0.0F; });
		for (const auto& t : due) {
			REX::INFO("Undress: {:08X}, animated step ({} slot(s))", t.actor, t.slots.size());
			Strip(t.actor, t.slots);
		}
	}

	void StripAll(std::uint32_t a_id, bool a_byHand)
	{
		Strip(a_id, g_config.slots, a_byHand);
	}

	bool IsStripped(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto it = g_states.find(a_id);
		return it != g_states.end() && !it->second.items.empty();
	}

	std::vector<std::uint32_t> Stripped(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto it = g_states.find(a_id);
		return it != g_states.end() ? it->second.items : std::vector<std::uint32_t>{};
	}

	void NoteStripped(std::uint32_t a_id, const std::vector<std::uint32_t>& a_items)
	{
		std::scoped_lock lock(g_lock);
		auto& items = g_states[a_id].items;
		for (const auto item : a_items) {
			if (std::ranges::find(items, item) == items.end()) {
				items.push_back(item);
			}
		}
	}

	void Redress(std::uint32_t a_id, bool a_force, bool a_afterScene)
	{
		std::vector<std::int32_t> items;
		{
			std::scoped_lock lock(g_lock);
			const auto it = g_states.find(a_id);
			if (it == g_states.end()) {
				return;
			}
			for (const auto item : it->second.items) {
				items.push_back(static_cast<std::int32_t>(item));
			}
			g_states.erase(it);
		}
		if (items.empty() || (!g_config.redress && !a_force)) {
			return;
		}
		// After a scene, NPCs dress a body part at a time with the redress
		// animations (if any are installed), like OStim.
		std::vector<std::int32_t> groups(items.size(), -1);
		std::vector<std::int32_t> idles;
		std::vector<float>        lengths, dressAts;
		const auto                actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
		if (a_afterScene && g_config.animateRedress && actor && !IsPlayer(a_id)) {
			const auto sex = SceneRegistry::SexOf(actor);
			for (const auto& part : REDRESS_PARTS) {
				std::vector<std::size_t> mine;
				for (std::size_t i = 0; i < items.size(); ++i) {
					const auto item = RE::TESForm::GetFormByID(static_cast<std::uint32_t>(items[i]));
					if (groups[i] < 0 && item && (item->GetFilledSlots() & part.mask)) {
						mine.push_back(i);
					}
				}
				if (mine.empty()) {
					continue;
				}
				if (const auto anim = FindRedressAnim(part, sex)) {
					for (const auto i : mine) {
						groups[i] = static_cast<std::int32_t>(idles.size());
					}
					idles.push_back(anim->idle);
					lengths.push_back(anim->length);
					dressAts.push_back(anim->dressAt);
				}
			}
		}
		if (const auto vm = FourStim::GetVM()) {
			REX::INFO("Undress: dressing {:08X} again ({} item(s), {} redress animation(s))", a_id, items.size(), idles.size());
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("FourStimUndress"sv, "Redress"sv, callback, static_cast<std::int32_t>(a_id), items, g_config.itemDelay, groups, idles,
				lengths, dressAts);
		}
		HUD::OnFocusedSceneChanged();
	}

	void Clear()
	{
		g_timed.clear();
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
