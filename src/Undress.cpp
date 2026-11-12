#include "Undress.h"
#include "PenisMesh.h"
#include "Scheduler.h"
#include "compat/ScrapFunction.h"
#include "compat/Fallbacks.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <thread>
#include <format>
#include <string>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "Actions.h"
#include "ActorProperties.h"
#include "Bridge.h"
#include "EquipObjects.h"
#include "Erection.h"
#include "Physics.h"
#include "HUD.h"

namespace Undress
{
	namespace
	{
		struct State
		{
			std::uint64_t              handled = 0;  // bit (slot - 30): already taken off (or asked to)
			std::vector<std::uint32_t> items;        // what was taken off, to put back on
			bool                       leftoversLogged = false;  // what a full undress left on, logged once
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
		std::unordered_set<std::uint32_t>        g_blocked;  // OStim's NoUndressing (a thread builder's): not undressed by scenes

		bool IsPlayer(std::uint32_t a_id)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			return player && player->GetFormID() == a_id;
		}

		constexpr std::uint32_t PIPBOY = 0x00021B3B;  // the Pip-Boy (an armor) never comes off

		// OStim's FormUtil::canUndress: an item with a keyword whose editor ID
		// has "NoStrip" in it (OStimNoStrip, SexLabNoStrip...) stays on.
		bool NoStrip(const RE::TESObjectARMO* a_armor)
		{
			bool found = false;
			a_armor->ForEachKeyword([&](RE::BGSKeyword* a_keyword) {
				std::string id(a_keyword->formEditorID.c_str());
				std::ranges::transform(id, id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				found = id.find("nostrip") != std::string::npos;
				return found ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
			});
			return found;
		}

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

		// The redress scene for a_part that a_actor fits (their role in it), if any.
		std::optional<RedressAnim> FindRedressAnim(const RedressPart& a_part, const SceneRegistry::ActorCondition& a_actor)
		{
			SceneRegistry::ListFilter filter;
			filter.actors = { a_actor };
			for (const auto& summary : SceneRegistry::List(1, filter)) {
				const auto scene = SceneRegistry::Find(summary.id);
				if (!scene || !HasTag(*scene, "redress") || !HasTag(*scene, a_part.tag) || scene->speeds.empty() || scene->speeds[0].empty() || !scene->speeds[0][0]) {
					continue;
				}
				// Dressing is by body: the role's sex counts even without SetOnlyGayAnimsInGayScenes.
				if (const auto want = scene->actors[0].sex; want != SceneRegistry::Sex::kAny && a_actor.sex != SceneRegistry::Sex::kAny && want != a_actor.sex) {
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
			// Read-locked: another thread may be adding an item right now (an
			// equip object put on: AddItem, then EquipItem), which can move
			// the array; unlocked, a worn item could be missed.
			const RE::BSAutoReadLock lock{ list->rwLock };
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

		std::string SlotList(std::uint32_t a_mask)
		{
			std::string out;
			for (int slot = 30; slot <= 61; ++slot) {
				if (a_mask & (1u << (slot - 30))) {
					out += (out.empty() ? "" : ", ") + std::to_string(slot);
				}
			}
			return out;
		}

		// After a full undress: what's still on (armor in slots outside
		// the undress slots, or NoStrip),
		// once per actor and scene, so a report of "armor left on" can be
		// read in the log. Main thread.
		void LogLeftovers(RE::Actor* a_actor, const std::vector<std::uint32_t>& a_takingOff)
		{
			{
				std::scoped_lock lock(g_lock);
				auto& state = g_states[a_actor->GetFormID()];
				if (state.leftoversLogged) {
					return;
				}
				state.leftoversLogged = true;
			}
			for (const auto id : WornIn(a_actor, 0xFFFFFFFF)) {
				if (std::ranges::find(a_takingOff, id) != a_takingOff.end() || EquipObjects::IsObjectItem(id) || Erection::IsErectionItem(id) || PenisMesh::GivesPenis(a_actor, id)) {
					continue;
				}
				const auto item = RE::TESForm::GetFormByID(id);
				const auto armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
				const char* name = armor ? armor->GetFullName() : nullptr;
				REX::INFO("Undress: {:08X} keeps {:08X} \"{}\" on (slots {}; {})", a_actor->GetFormID(), id, name ? name : "",
					SlotList(armor ? armor->GetFilledSlots() : 0), armor && NoStrip(armor) ? "a NoStrip keyword" : "none of them checked in the undress slots");
			}
		}

		// The geometry a_actor's 3D draws now (third person), to tell whether
		// a strip changed it. Main thread.
		void Geometry(RE::NiAVObject* a_object, std::vector<RE::NiAVObject*>& a_out, int a_depth)
		{
			if (!a_object) {
				return;
			}
			if (a_object->IsGeometry()) {
				a_out.push_back(a_object);
				return;
			}
			if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
				for (const auto& child : node->children) {
					Geometry(child.get(), a_out, a_depth - 1);
				}
			}
		}

		std::vector<RE::NiAVObject*> GeometryOf(std::uint32_t a_id)
		{
			std::vector<RE::NiAVObject*> out;
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id)) {
				Geometry(actor->Get3D(false), out, 32);
			}
			std::ranges::sort(out);
			return out;
		}

		// Two seconds after a strip: whether the items are off, and whether
		// the body drawn changed. A 1.10.163 tester's female player stayed
		// dressed while the game had her clothes off (her 3D drew the same
		// 27 shapes all scene, and nothing was put back on). A player whose
		// items are off but whose 3D is unchanged is redrawn, as F4SE's
		// Actor.QueueUpdate does. Any thread.
		void CheckStrip(std::uint32_t a_id, std::vector<std::uint32_t> a_items, std::vector<RE::NiAVObject*> a_before)
		{
			Scheduler::AfterOnMain(std::chrono::seconds(2), [a_id, items = std::move(a_items), before = std::move(a_before)]() {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
				if (!actor || !actor->Get3D(false)) {
					return;
				}
				std::size_t on = 0;
				for (const auto worn : WornIn(actor, 0xFFFFFFFF)) {
					on += std::ranges::count(items, worn);
				}
				const auto after = GeometryOf(a_id);
				const bool same = after == before;
				REX::INFO("Undress check: {:08X}: {} of {} item(s) still on; drawn {} -> {} shape(s){}", a_id, on, items.size(), before.size(), after.size(),
					same ? ", unchanged" : "");
				// Only for clothes on the body (slot 33) and the head (30-32,
				// 46-49: hats, helmets, masks): redrawing the player for
				// gloves or a ring was a visible stutter in the next scene
				// (the owner's "Turn around" after a standing hold took off
				// his gloves, 5 November), but a helmet left drawn is plain
				// to see (the owner's, 7 November: "Rider Helmet Silver
				// Shroud" off by the game, the same 212 shapes drawn, still
				// on his head through a kiss). The tester's stuck clothes
				// were body clothes.
				std::uint32_t slots = 0;
				for (const auto item : items) {
					if (const auto form = RE::TESForm::GetFormByID(item)) {
						if (const auto armor = form->As<RE::TESObjectARMO>()) {
							slots |= armor->GetFilledSlots();
						}
					}
				}
				constexpr std::uint32_t SEEN = (1u << (33 - 30)) | (1u << 0) | (1u << 1) | (1u << 2) | (1u << (46 - 30)) | (1u << (47 - 30)) | (1u << (48 - 30)) |
				                               (1u << (49 - 30));
				const bool body = (slots & SEEN) != 0;
				if (on == 0 && same && IsPlayer(a_id) && !body) {
					REX::INFO("Undress check: nothing on the body or head came off (slots {:08X}): the shapes drawn staying the same is expected, no redraw", slots);
				} else if (on == 0 && same && IsPlayer(a_id)) {
					if (Compat::CanReset3D()) {
						REX::WARN("Undress check: the player's clothes are off but still drawn: redrawing them");
						actor->Reset3D(true, 0, true, 0xC);
						// The rebuild takes FSMP back to the mesh's own physics
						// file (the owner's A-Body swap went rigid for a moment,
						// then flaccid again, 1 November): Actor3D notices the
						// new 3D and puts the swap back, as for any rebuild.
					} else {
						REX::WARN("Undress check: the player's clothes are off but still drawn (no Actor::Reset3D on this game version to redraw them)");
					}
				}
			});
		}

		// Takes off what's in a_slots (those not taken off yet): finds the
		// items here, Papyrus unequips them. Any thread.
		void Strip(std::uint32_t a_id, const std::vector<int>& a_slots, bool a_byHand = false)
		{
			if (!a_byHand && (!g_config.enabled || (!g_config.player && IsPlayer(a_id)))) {
				return;
			}
			{
				std::scoped_lock lock(g_lock);
				if (!a_byHand && g_blocked.contains(a_id)) {
					return;
				}
			}
			std::uint32_t mask = 0;
			{
				std::scoped_lock lock(g_lock);
				auto& state = g_states[a_id];
				for (const auto slot : a_slots) {
					// As OStim's canUndress + overlaps: the slots asked for, and
					// below, items that also cover an undress slot (an outfit on
					// 33 and 50 comes off for an action asking for 50).
					if (slot < 30 || slot > 61) {
						continue;
					}
					const auto bit = std::uint64_t{ 1 } << (slot - 30);
					// By hand (the HUD's undress): every slot again, so what an
					// earlier strip missed or the game put back on comes off.
					if (a_byHand || !(state.handled & bit)) {
						state.handled |= bit;
						mask |= static_cast<std::uint32_t>(bit);
					}
				}
			}
			if (!mask) {
				return;
			}
			const bool full = a_slots == g_config.slots;
			std::uint32_t allowed = 0;
			for (const auto slot : g_config.slots) {
				if (slot >= 30 && slot <= 61) {
					allowed |= 1u << (slot - 30);
				}
			}
			if (a_byHand) {
				allowed = mask;  // by hand: what was asked for, undress slots or not
			}
			if (g_config.papyrusUndressing && !a_byHand) {
				// OStim's UsePapyrusUndressing: an add-on's FourUndress does it.
				if (const auto vm = FourStim::GetVM()) {
					REX::INFO("Undress: {:08X}, slots 0x{:08X}: FourUndress.UndressPartial (Papyrus undressing)", a_id, mask & allowed);
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(vm, "FourUndress"sv, "UndressPartial"sv, callback, static_cast<std::int32_t>(a_id), static_cast<std::int32_t>(mask & allowed));
				}
				return;
			}
			// OStim's SetRemoveWeaponsWithSlot: taking off that slot takes the
			// weapon too (weapons are off from the start with
			// SetRemoveWeaponsAtStart, the default).
			if (const int ws = g_config.removeWeaponsWithSlot; !g_config.removeWeaponsAtStart && ws >= 30 && ws <= 61 && (mask & allowed & (1u << (ws - 30)))) {
				if (const auto vm = FourStim::GetVM()) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(vm, "FourStimScene"sv, "HolsterByID"sv, callback, static_cast<std::int32_t>(a_id));
				}
			}
			F4SE::GetTaskInterface()->AddTask([a_id, mask, allowed, full]() {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
				if (!actor) {
					return;
				}
				auto items = WornIn(actor, mask);
				std::erase_if(items, [actor, allowed](std::uint32_t a_item) {
					if (EquipObjects::IsObjectItem(a_item) || Erection::IsErectionItem(a_item)) {
						return true;  // 4Stim's own strap-ons, erect bodysuits and such stay on
					}
					if (PenisMesh::GivesPenis(actor, a_item)) {
						REX::INFO("Undress: {:08X} keeps {:08X} on (it gives them a penis)", actor->GetFormID(), a_item);
						return true;  // a wearable penis (futa) stays on, as a body would
					}
					const auto armor = RE::TESForm::GetFormByID<RE::TESObjectARMO>(a_item);
					// Not on any undress slot: it stays (OStim's canUndress).
					if (armor && !(armor->GetFilledSlots() & allowed)) {
						return true;
					}
					return armor && NoStrip(armor);
				});
				std::string names;
				for (const auto item : items) {
					const auto armor = RE::TESForm::GetFormByID<RE::TESObjectARMO>(item);
					const char* name = armor ? armor->GetFullName() : nullptr;
					names += std::format("{}{:08X} \"{}\"", names.empty() ? "" : ", ", item, name ? name : "");
				}
				REX::INFO("Undress: {:08X}, slots 0x{:08X}: {} item(s) to take off{}{}", a_id, mask, items.size(), names.empty() ? "" : ": ", names);
				if (full) {
					LogLeftovers(actor, items);
				}
				if (items.empty()) {
					return;
				}
				NoteStripped(a_id, items);
				CheckStrip(a_id, items, GeometryOf(a_id));
				if (const auto vm = FourStim::GetVM()) {
					std::vector<std::int32_t> ids(items.begin(), items.end());
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(vm, "FourStimUndress"sv, "Strip"sv, callback, static_cast<std::int32_t>(a_id), ids, g_config.itemDelay);
				}
				HUD::OnFocusedSceneChanged();  // the HUD's "Undress / Dress" entries
			});
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	namespace
	{
		// FourUndress.UsePapyrusUndressing's answer.
		class UsePapyrusAnswer : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			void CallQueued() override {}
			void CallCanceled() override {}
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				const bool use = a_result.is<bool>() && RE::BSScript::get<bool>(a_result);
				if (use != g_config.papyrusUndressing) {
					REX::INFO("Undress: Papyrus undressing {} (FourUndress.UsePapyrusUndressing)", use ? "on: an add-on undresses and dresses actors" : "off");
				}
				g_config.papyrusUndressing = use;
			}
		};
	}

	void QueryPapyrusUndressing()
	{
		if (const auto vm = FourStim::GetVM()) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new UsePapyrusAnswer());
			Compat::DispatchStaticCall(vm, "FourUndress"sv, "UsePapyrusUndressing"sv, callback);
		}
	}

	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start)
	{
		// A new scene: steps left from the last one won't happen.
		std::erase_if(g_timed, [&](const Timed& t) { return std::ranges::find(a_ids, t.actor) != a_ids.end(); });
		if (!g_config.enabled) {
			return;
		}
		// As OStim's: undressing at the start happens as actors join,
		// whatever the first scene says (its noStrip is for its actions).
		if (a_start && g_config.atStart) {
			for (const auto id : a_ids) {
				Strip(id, g_config.slots);
			}
			return;
		}
		if (a_scene.noStrip) {
			return;
		}
		for (const auto& step : a_scene.undress) {
			if (step.actor < a_ids.size()) {
				g_timed.push_back({ a_ids[step.actor], step.slots, step.at });
			}
		}
		for (std::size_t role = 0; role < a_ids.size(); ++role) {
			// OStim's actor noStrip: no undressing by this role's actions.
			if (role < a_scene.actors.size() && a_scene.actors[role].noStrip) {
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

	void Enforce(RE::Actor* a_actor)
	{
		if (!a_actor || g_config.papyrusUndressing) {
			return;
		}
		const auto id = a_actor->GetFormID();
		std::vector<std::uint32_t> stripped;
		{
			std::scoped_lock lock(g_lock);
			const auto it = g_states.find(id);
			if (it == g_states.end() || it->second.items.empty()) {
				return;
			}
			stripped = it->second.items;
		}
		static std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> last;  // main thread
		const auto now = std::chrono::steady_clock::now();
		if (auto& when = last[id]; now - when < std::chrono::seconds(1)) {
			return;
		} else {
			when = now;
		}
		std::vector<std::int32_t> again;
		for (const auto worn : WornIn(a_actor, 0xFFFFFFFF)) {
			if (std::ranges::find(stripped, worn) != stripped.end()) {
				again.push_back(static_cast<std::int32_t>(worn));
			}
		}
		if (again.empty()) {
			return;
		}
		REX::INFO("Undress: {:08X} has {} item(s) back on mid-scene (the game put them back on): taking them off again", id, again.size());
		if (const auto vm = FourStim::GetVM()) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(vm, "FourStimUndress"sv, "Strip"sv, callback, static_cast<std::int32_t>(id), again, 0.0F);
		}
	}

	void StripAll(std::uint32_t a_id, bool a_byHand)
	{
		Strip(a_id, g_config.slots, a_byHand);
	}

	void StripMask(std::uint32_t a_id, std::uint32_t a_mask)
	{
		std::vector<int> slots;
		for (int slot = 30; slot <= 61; ++slot) {
			if (a_mask & (1u << (slot - 30))) {
				slots.push_back(slot);
			}
		}
		Strip(a_id, slots, true);
	}

	void RedressMask(std::uint32_t a_id, std::uint32_t a_mask)
	{
		if (g_config.papyrusUndressing) {
			if (const auto vm = FourStim::GetVM()) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(vm, "FourUndress"sv, "RedressPartial"sv, callback, static_cast<std::int32_t>(a_id), static_cast<std::int32_t>(a_mask));
			}
			return;
		}
		std::vector<std::int32_t> items;
		{
			std::scoped_lock lock(g_lock);
			const auto it = g_states.find(a_id);
			if (it == g_states.end()) {
				return;
			}
			auto& state = it->second;
			std::erase_if(state.items, [&](std::uint32_t a_item) {
				const auto form = RE::TESForm::GetFormByID(a_item);
				if (!form || !(form->GetFilledSlots() & a_mask)) {
					return false;
				}
				items.push_back(static_cast<std::int32_t>(a_item));
				return true;
			});
			state.handled &= ~static_cast<std::uint64_t>(a_mask);  // those slots can come off again
		}
		if (items.empty()) {
			return;
		}
		if (const auto vm = FourStim::GetVM()) {
			REX::INFO("Undress: dressing {:08X} again in slots {:08X} ({} item(s))", a_id, a_mask, items.size());
			const std::vector<std::int32_t> groups(items.size(), -1);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(vm, "FourStimUndress"sv, "Redress"sv, callback, static_cast<std::int32_t>(a_id), items, g_config.itemDelay, groups,
				std::vector<std::int32_t>{}, std::vector<float>{}, std::vector<float>{});
		}
	}

	std::vector<std::uint32_t> WornItems(RE::Actor* a_actor, std::uint32_t a_mask)
	{
		return a_actor ? WornIn(a_actor, a_mask) : std::vector<std::uint32_t>{};
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
		if (g_config.papyrusUndressing) {
			// OStim's UsePapyrusUndressing: the add-on's FourUndress dresses them.
			{
				std::scoped_lock lock(g_lock);
				g_states.erase(a_id);
			}
			if (const auto vm = (g_config.redress || a_force) ? FourStim::GetVM() : nullptr) {
				REX::INFO("Undress: {:08X}: FourUndress.Redress (Papyrus undressing)", a_id);
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(vm, "FourUndress"sv, "Redress"sv, callback, static_cast<std::int32_t>(a_id));
			}
			return;
		}
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
			const auto condition = ActorProperties::Condition(actor);
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
				if (const auto anim = FindRedressAnim(part, condition)) {
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
			Compat::DispatchStaticCall(vm, "FourStimUndress"sv, "Redress"sv, callback, static_cast<std::int32_t>(a_id), items, g_config.itemDelay, groups, idles,
				lengths, dressAts);
		}
		HUD::OnFocusedSceneChanged();
	}

	void Block(std::uint32_t a_id, bool a_blocked)
	{
		std::scoped_lock lock(g_lock);
		if (a_blocked) {
			g_blocked.insert(a_id);
		} else {
			g_blocked.erase(a_id);
		}
	}

	void Clear()
	{
		g_timed.clear();
		std::scoped_lock lock(g_lock);
		g_states.clear();
		g_blocked.clear();
	}
}
