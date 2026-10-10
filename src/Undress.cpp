#include "Undress.h"

#include <algorithm>
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

		Config                                   g_config;
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

		// "Plugin.esp|0xID" -> the idle's form ID (0 if none or not found).
		std::uint32_t IdleFormID(const std::string& a_spec)
		{
			const auto bar = a_spec.find('|');
			const auto handler = RE::TESDataHandler::GetSingleton();
			if (a_spec.empty() || bar == std::string::npos || !handler) {
				return 0;
			}
			try {
				const auto id = static_cast<std::uint32_t>(std::stoul(a_spec.substr(bar + 1), nullptr, 16));
				const auto idle = handler->LookupForm<RE::TESIdleForm>(id & 0x00FFFFFF, a_spec.substr(0, bar));
				if (!idle) {
					REX::WARN("Undress: no idle {} (sRedressIdle)", a_spec);
				}
				return idle ? idle->GetFormID() : 0;
			} catch (const std::exception&) {
				REX::WARN("Undress: sRedressIdle \"{}\" isn't Plugin.esp|0xID", a_spec);
				return 0;
			}
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
		if (!g_config.enabled || a_scene.noStrip) {
			return;
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
		if (const auto vm = FourStim::GetVM()) {
			const auto idle = a_afterScene ? static_cast<std::int32_t>(IdleFormID(g_config.redressIdle)) : 0;
			REX::INFO("Undress: dressing {:08X} again ({} item(s){})", a_id, items.size(), idle ? ", redress idle first" : "");
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("FourStimUndress"sv, "Redress"sv, callback, static_cast<std::int32_t>(a_id), items, g_config.itemDelay, idle,
				g_config.redressIdleLength);
		}
		HUD::OnFocusedSceneChanged();
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
