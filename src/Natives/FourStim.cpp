#include "Core/Core.h"

namespace Core
{
	// ---- Actions (docs/ACTIONS.md) ----

	bool SceneHasAction(std::monostate, std::string a_sceneID, std::string a_type)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && scene->HasAction(a_type);
	}

	bool SceneHasActionTag(std::monostate, std::string a_sceneID, std::string a_tag)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && scene->HasActionTag(a_tag);
	}

	std::int32_t GetSceneActionCount(std::monostate, std::string a_sceneID)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene ? static_cast<std::int32_t>(scene->actions.size()) : 0;
	}

	// The first action of a_type (id, alias, or "" for any) with these roles
	// (-1: any); its index, or -1.
	std::int32_t FindSceneAction(std::monostate, std::string a_sceneID, std::string a_type, std::int32_t a_actor, std::int32_t a_target)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene) {
			return -1;
		}
		// By the alias-resolved id, as OStim's findAction: unknown types too.
		const auto id = a_type.empty() ? std::string{} : Actions::ResolveAlias(a_type);
		for (std::size_t i = 0; i < scene->actions.size(); ++i) {
			const auto& action = scene->actions[i];
			if ((id.empty() || action.type->id == id) &&
				(a_actor < 0 || action.actor == static_cast<std::size_t>(a_actor)) &&
				(a_target < 0 || action.target == static_cast<std::size_t>(a_target))) {
				return static_cast<std::int32_t>(i);
			}
		}
		return -1;
	}
		const SceneRegistry::SceneAction* SceneActionAt(const SceneRegistry::Scene* a_scene, std::int32_t a_index)
		{
			return a_scene && a_index >= 0 && static_cast<std::size_t>(a_index) < a_scene->actions.size() ? &a_scene->actions[static_cast<std::size_t>(a_index)] : nullptr;
		}

	std::string GetSceneActionType(std::monostate, std::string a_sceneID, std::int32_t a_index)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		const auto action = SceneActionAt(scene.get(), a_index);
		return action ? action->type->id : std::string{};
	}

	// a_which: 0 actor, 1 target, 2 performer. The role index, or -1.
	std::int32_t GetSceneActionRole(std::monostate, std::string a_sceneID, std::int32_t a_index, std::int32_t a_which)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		const auto action = SceneActionAt(scene.get(), a_index);
		if (!action) {
			return -1;
		}
		switch (a_which) {
		case 0:
			return static_cast<std::int32_t>(action->actor);
		case 1:
			return static_cast<std::int32_t>(action->target);
		case 2:
			return static_cast<std::int32_t>(action->performer);
		default:
			return -1;
		}
	}

	std::vector<std::string> GetActionTypes(std::monostate)
	{
		return Actions::List();
	}

	// Events (docs/EVENTS.md), OStim's OThread.CallEvent: fires a_event in
	// the scene a_inScene is in; roles -1: a_inScene's own (actor and
	// performer) or none (target). False if there's no such event. The event
	// fires on the main thread right after.
	bool CallEvent(std::monostate, RE::Actor* a_inScene, std::string a_event, std::int32_t a_actor, std::int32_t a_target, std::int32_t a_performer)
	{
		if (!a_inScene) {
			return false;
		}
		auto lower = a_event;
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const auto events = Events::List();
		if (std::ranges::find(events, lower) == events.end()) {
			REX::WARN("CallEvent: no event \"{}\"", a_event);
			return false;
		}
		const auto id = a_inScene->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, lower, a_actor, a_target, a_performer]() {
			const auto active = FindActiveScene(id);
			if (!active) {
				REX::WARN("CallEvent \"{}\": {:08X} isn't in a scene", lower, id);
				return;
			}
			const int self = static_cast<int>(std::ranges::find(active->actors, id) - active->actors.begin());
			FireEvent(*active, lower, a_actor < 0 ? self : a_actor, a_target, a_performer < 0 ? (a_actor < 0 ? self : a_actor) : a_performer);
		});
		return true;
	}

	std::vector<std::string> GetEvents(std::monostate)
	{
		return Events::List();
	}

	std::string GetActionName(std::monostate, std::string a_type)
	{
		const auto type = Actions::Find(a_type);
		return type ? type->name : std::string{};
	}

	std::vector<std::string> GetActionTags(std::monostate, std::string a_type)
	{
		const auto type = Actions::Find(a_type);
		return type ? type->tags : std::vector<std::string>{};
	}

	bool ActionHasTag(std::monostate, std::string a_type, std::string a_tag)
	{
		const auto type = Actions::Find(a_type);
		return type && type->HasTag(a_tag);
	}

	// ---- Excitement (Excitement.h) ----

	float GetExcitement(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::Get(a_actor->GetFormID()) : -1.0F;
	}

	void SetExcitement(std::monostate, RE::Actor* a_actor, float a_value)
	{
		if (a_actor) {
			Excitement::Set(a_actor->GetFormID(), a_value);
		}
	}

	void AddExcitement(std::monostate, RE::Actor* a_actor, float a_value, bool a_useMultiplier)
	{
		if (a_actor) {
			Excitement::Add(a_actor->GetFormID(), a_value, a_useMultiplier);
		}
	}

	std::int32_t GetTimesClimaxed(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::TimesClimaxed(a_actor->GetFormID()) : 0;
	}

	// OStim's OActor.Climax(act, ignoreStall) (ThreadActor::orgasm): while
	// stalled the climax waits for the stall to end, and ignoring the stall
	// doesn't lift it. Was: the stall dropped for good, and the climax lost
	// if a stall came back before the next tick.
	void Climax(std::monostate, RE::Actor* a_actor, bool a_ignoreStall)
	{
		if (a_actor) {
			Excitement::Orgasm(a_actor->GetFormID(), a_ignoreStall);
		}
	}

	void StallClimax(std::monostate, RE::Actor* a_actor, bool a_stall)
	{
		if (a_actor) {
			Excitement::SetStalled(a_actor->GetFormID(), a_stall);
		}
	}

	// OActor.IsClimaxStalled(act, checkThread): the actor's own stall, or
	// with a_checkThread their thread's too.
	bool IsClimaxStalled(std::monostate, RE::Actor* a_actor, bool a_checkThread)
	{
		return a_actor && Excitement::IsStalled(a_actor->GetFormID(), a_checkThread);
	}

	float GetExcitementMultiplier(std::monostate, RE::Actor* a_actor)
	{
		// As OStim's: -1 for an actor not in a scene.
		return a_actor && Excitement::Get(a_actor->GetFormID()) >= 0.0F ? Excitement::Multiplier(a_actor->GetFormID()) : -1.0F;
	}

	void SetExcitementMultiplier(std::monostate, RE::Actor* a_actor, float a_multiplier)
	{
		if (a_actor) {
			Excitement::SetMultiplier(a_actor->GetFormID(), a_multiplier);
		}
	}

	float GetTimeUntilClimax(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::TimeUntilClimax(a_actor->GetFormID()) : -1.0F;
	}

	// ---- Undressing (Undress.h) ----

	// FourStimUndress.Strip reports what it took off.
	void NoteStripped(std::monostate, RE::Actor* a_actor, std::vector<RE::TESForm*> a_items)
	{
		if (!a_actor) {
			return;
		}
		std::vector<std::uint32_t> ids;
		for (const auto item : a_items) {
			if (item) {
				ids.push_back(item->GetFormID());
			}
		}
		Undress::NoteStripped(a_actor->GetFormID(), ids);
	}

	void UndressActor(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			Undress::StripAll(a_actor->GetFormID(), true);
		}
	}

	void RedressActor(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			Undress::Redress(a_actor->GetFormID(), true);
		}
	}

	// ---- Auto mode and scene details ----

	// Whether a_actor is in a running scene (one actor, one scene).
	bool IsInScene(std::monostate, RE::Actor* a_actor)
	{
		return a_actor && FindActiveScene(a_actor->GetFormID()) != nullptr;
	}

	// FourStimScene's EndPairScene / EndGroupScene, once they've let the
	// actors go: the end event of the scene they were in goes out now
	// (SendEndedFor).
	void SceneReleased(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			if (actor) {
				ids.push_back(actor->GetFormID());
			}
		}
		F4SE::GetTaskInterface()->AddTask([ids = std::move(ids)]() { SendEndedFor(ids, false); });
	}

	void SetAutoMode(std::monostate, RE::Actor* a_actor, bool a_on)
	{
		if (!a_actor) {
			return;
		}
		const auto id = a_actor->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, a_on]() {
			if (const auto active = FindActiveScene(id)) {
				a_on ? StartAutoMode(*active) : StopAutoMode(*active);
			}
		});
	}

	bool IsAutoMode(std::monostate, RE::Actor* a_actor)
	{
		// Read from a Papyrus thread: a plain copy of the flag is fine.
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		return active && active->autoMode.on;
	}

	// Plays the a_event auto transition ("climax", "pullout"...) of the
	// scene a_actor is in: a_role's (or the first role that has one, -1).
	bool AutoTransition(std::monostate, RE::Actor* a_actor, std::string a_event, std::int32_t a_role)
	{
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		const auto scene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
		if (!scene) {
			return false;
		}
		std::string dest;
		for (std::size_t role = 0; role < scene->actors.size() && dest.empty(); ++role) {
			if (a_role < 0 || static_cast<std::size_t>(a_role) == role) {
				dest = scene->actors[role].AutoTransition(a_event);
			}
		}
		if (dest.empty()) {
			dest = scene->AutoTransition(a_event);  // OStim's scene-wide ones
		}
		if (dest.empty()) {
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID(), dest]() {
			if (const auto active = FindActiveScene(id); active && PlayOnActiveScene(*active, dest, -1)) {
				ArmAutoplay(*active);
			}
		});
		return true;
	}

	bool SceneActorHasTag(std::monostate, std::string a_sceneID, std::int32_t a_role, std::string a_tag)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && a_role >= 0 && static_cast<std::size_t>(a_role) < scene->actors.size() && scene->actors[a_role].HasTag(a_tag);
	}

	std::vector<std::string> GetSceneActorTags(std::monostate, std::string a_sceneID, std::int32_t a_role)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && a_role >= 0 && static_cast<std::size_t>(a_role) < scene->actors.size() ? scene->actors[a_role].tags : std::vector<std::string>{};
	}

	// Re-reads all scene files (handy while authoring). Returns the count loaded.
	std::int32_t ReloadScenes(std::monostate)
	{
		Translations::Load();  // first: scenes, furniture... are translated as they load
		const auto count = SceneRegistry::Reload();
		Alignment::Reload();
		Expressions::Reload();
		Sounds::Reload();
		Events::Reload();
		Lights::Reload();
		Scaling::Reload();
		ActorProperties::Reload();
		EquipObjects::Reload();
		Options::Reload();
		HUD::LoadConfig();  // theme and Utility entries too, for HUD authors
		return count;
	}

	// Equip objects (OStim's OActor functions), for actors in a scene.
	bool IsTongue(std::string a_type)
	{
		std::ranges::transform(a_type, a_type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_type == "tongue";
	}

	bool EquipObject(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		if (!a_actor) {
			return false;
		}
		const bool tongue = IsTongue(a_type);
		const bool ok = EquipObjects::Equip(a_actor->GetFormID(), std::move(a_type));
		if (ok && tongue) {
			Expressions::Hold(a_actor->GetFormID(), "tongue");  // its mouth open, or it can't be seen (Expressions::Hold)
		}
		return ok;
	}

	void UnequipObject(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		if (a_actor) {
			const bool tongue = IsTongue(a_type);
			EquipObjects::Unequip(a_actor->GetFormID(), std::move(a_type));
			if (tongue) {
				Expressions::Hold(a_actor->GetFormID(), "");
			}
		}
	}

	bool IsObjectEquipped(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		return a_actor && EquipObjects::IsEquipped(a_actor->GetFormID(), std::move(a_type));
	}

	bool SetObjectVariant(std::monostate, RE::Actor* a_actor, std::string a_type, std::string a_variant, float a_duration)
	{
		return a_actor && EquipObjects::SetVariant(a_actor->GetFormID(), std::move(a_type), std::move(a_variant), a_duration);
	}

	void UnsetObjectVariant(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		if (a_actor) {
			EquipObjects::UnsetVariant(a_actor->GetFormID(), std::move(a_type));
		}
	}

	std::vector<std::string> GetEquipObjectTypes(std::monostate)
	{
		return EquipObjects::Types();
	}

	// FourStimScene's release, an NPC out of a scene: EvaluatePackage on the
	// main thread, through iEvaluatePackage's switch (Papyrus's as before
	// with 0; directly with 1 or 2, logged: the test in roadmap 2b.6).
	void EvaluatePackageOf(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		const auto id = a_actor->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id]() {
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
				EvaluateActorPackage(actor, "scene end");
			}
		});
	}

	// 4Stim's undressing took clothes off or put them on (Papyrus, after the
	// items): heels are measured again and HHS turned off or on to match.
	void HeelsChanged(std::monostate, std::int32_t a_actorID)
	{
		const auto id = static_cast<std::uint32_t>(a_actorID);
		F4SE::GetTaskInterface()->AddTask([id]() {
			if (const auto active = FindActiveScene(id)) {
				LockScene(*active);  // measures heels again, rescales, HHS on / off
				return;
			}
			bool waiting;
			{
				std::scoped_lock lock(g_heelsWaitingLock);
				waiting = g_heelsWaiting.contains(id);
			}
			if (waiting && Scaling::HeelHeight(RE::TESForm::GetFormByID<RE::Actor>(id)) > 0.0F) {
				{
					std::scoped_lock lock(g_heelsWaitingLock);
					g_heelsWaiting.erase(id);
				}
				RestartHeels(id, "dressed again");
			}
		});
	}

	// Actor properties (docs/ACTORS.md): OStim's OActor.HasSchlong (4Stim:
	// HasPenis) and the actor type.
	bool HasPenis(std::monostate, RE::Actor* a_actor)
	{
		return ActorProperties::HasPenis(a_actor);
	}

	std::string GetActorType(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? ActorProperties::Type(a_actor) : std::string{};
	}

	// ---- Furniture (OStim's OFurniture and OThread furniture functions) ----

	// FourStimFurniture.Block blocked it (it wasn't already): 4Stim unblocks it.
	void NoteFurnitureBlocked(std::monostate, std::int32_t a_refID)
	{
		std::scoped_lock lock(g_blockedFurnitureLock);
		g_blockedFurniture.insert(static_cast<std::uint32_t>(a_refID));
	}

	// After the fade (FourStimMenu.ChangeFurnitureFaded).
	void ApplyFurnitureChangeNative(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() { ApplyFurnitureChange(); });
	}

	// OStim's OThread.ChangeFurniture: a_actor's scene onto a_furniture, with
	// a_sceneID or ("") a random intro / idle scene that fits there.
	void ChangeFurnitureNative(std::monostate, RE::Actor* a_actor, RE::TESObjectREFR* a_furniture, std::string a_sceneID)
	{
		if (!a_actor || !a_furniture) {
			return;
		}
		const auto actorID = a_actor->GetFormID();
		const auto refID = a_furniture->GetFormID();
		F4SE::GetTaskInterface()->AddTask([actorID, refID, sceneID = std::move(a_sceneID)]() {
			const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(refID);
			const auto type = ref ? Furniture::Classify(ref) : std::string{};
			if (type.empty()) {
				REX::WARN("ChangeFurniture: {:08X} isn't furniture of any type", refID);
				return;
			}
			RequestFurnitureChange(actorID, Furniture::Found{ type, Furniture::Name(type), refID, 0.0F, std::nullopt }, sceneID);
		});
	}

	// ---- Muting and choosing equip objects (OStim's OActor.Mute; the HUD's options) ----

	// Mute / Unmute apply on the main thread; until they have, IsMuted
	// answers what was asked, so a script reads back its own call (OStim's
	// are synchronous; parity audit F21).
	std::mutex                               g_muteLock;
	std::unordered_map<std::uint32_t, bool> g_muteAsked;

	void AskMute(std::uint32_t a_id, bool a_muted)
	{
		{
			std::scoped_lock lock(g_muteLock);
			g_muteAsked[a_id] = a_muted;
		}
		F4SE::GetTaskInterface()->AddTask([a_id, a_muted]() {
			Sounds::SetMuted(a_id, a_muted);
			std::scoped_lock lock(g_muteLock);
			if (const auto it = g_muteAsked.find(a_id); it != g_muteAsked.end() && it->second == a_muted) {
				g_muteAsked.erase(it);
			}
		});
	}

	void Mute(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			AskMute(a_actor->GetFormID(), true);
		}
	}

	void Unmute(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			AskMute(a_actor->GetFormID(), false);
		}
	}

	void NoteSceneWeapon(std::uint32_t a_actor, std::uint32_t a_weapon)
	{
		std::scoped_lock lock(g_sceneWeaponsLock);
		if (a_weapon) {
			g_sceneWeapons[a_actor] = a_weapon;
		} else {
			g_sceneWeapons.erase(a_actor);
		}
	}

	std::uint32_t SceneWeaponOf(std::uint32_t a_actor)
	{
		std::scoped_lock lock(g_sceneWeaponsLock);
		const auto it = g_sceneWeapons.find(a_actor);
		return it != g_sceneWeapons.end() ? it->second : 0;
	}

	// FourStim.SheatheForScene: HolsterForScene puts a drawn weapon away
	// before taking it off (see SheatheNow).
	bool RemoveWeaponsAtStart(std::monostate)
	{
		return Undress::Settings().removeWeaponsAtStart;
	}

	bool PlayerAIDrivenInScenes(std::monostate)
	{
		return g_settings.playerAIDriven;
	}

	void SheatheForScene(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() { SheatheNow(RE::TESForm::GetFormByID<RE::Actor>(id), "scene start"); });
		}
	}

	void RememberSceneWeapon(std::monostate, RE::Actor* a_actor, RE::TESForm* a_weapon)
	{
		if (a_actor && a_weapon) {
			NoteSceneWeapon(a_actor->GetFormID(), a_weapon->GetFormID());
		}
	}

	// The weapon to put back on, by form ID (0: nothing was taken off), and
	// forgotten. A form ID, not the Form: a Form returned from here arrived
	// in Papyrus as "Mismatched types assigning to variable" (a 1.10.163
	// tester's Papyrus log, every scene end), as Actors passed from native
	// code do (FourStimPhysics), so weapons were never put back.
	std::int32_t TakeSceneWeaponID(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return 0;
		}
		const auto id = SceneWeaponOf(a_actor->GetFormID());
		NoteSceneWeapon(a_actor->GetFormID(), 0);
		return static_cast<std::int32_t>(id);
	}

	// OStim's OData.GetVoiceSetPairs / GetVoiceSetName / SetVoiceSet: voice
	// set choices per actor base form ID (docs/SOUND.md).
	std::vector<std::string> GetVoiceSetPairs(std::monostate)
	{
		return Sounds::VoiceSetPairs();
	}

	std::string GetVoiceSetName(std::monostate, std::int32_t a_formID)
	{
		return Sounds::VoiceSetName(static_cast<std::uint32_t>(a_formID));
	}

	void SetVoiceSet(std::monostate, std::int32_t a_formID, std::string a_voice)
	{
		const auto base = static_cast<std::uint32_t>(a_formID);
		Sounds::SetVoiceSet(base, a_voice);
		F4SE::GetTaskInterface()->AddTask([base]() { Sounds::ApplyVoiceSet(base); });
	}

	// OStim's OData action and event stimulation: per actor base, for a
	// side of an action or event (aiRole 1 actor, 2 target, 4 performer),
	// in place of its own (docs/EXCITEMENT.md). Saved with the game.
	std::vector<std::string> GetActions(std::monostate)
	{
		auto list = Actions::List();
		std::ranges::sort(list);
		return list;
	}

	// The side's own values: the action's, or the event's (its supertypes').
	std::pair<float, float> DefaultStim(StimOverrides::Kind a_kind, std::int32_t a_role, const std::string& a_id)
	{
		const int index = a_role == StimOverrides::kActor ? 0 : a_role == StimOverrides::kTarget ? 1 : a_role == StimOverrides::kPerformer ? 2 : -1;
		if (index < 0) {
			return { 0.0F, 0.0F };
		}
		if (a_kind == StimOverrides::Kind::kEvent) {
			return Events::DefaultStimulation(a_id, index);
		}
		const auto type = Actions::Find(a_id);
		if (!type) {
			return { 0.0F, 0.0F };
		}
		const auto& side = index == 0 ? type->actor : index == 1 ? type->target : type->performer;
		return { side.stimulation, side.maxStimulation };
	}

	// 4Stim's: the same for an actor (their base), for the HUD's options.
	void SetActorVoiceSet(std::monostate, RE::Actor* a_actor, std::string a_voice)
	{
		const auto npc = a_actor ? a_actor->GetNPC() : nullptr;
		if (npc) {
			SetVoiceSet({}, static_cast<std::int32_t>(npc->GetFormID()), std::move(a_voice));
		}
	}

	std::string GetActorVoiceSetName(std::monostate, RE::Actor* a_actor)
	{
		const auto npc = a_actor ? a_actor->GetNPC() : nullptr;
		return Sounds::VoiceSetName(npc ? npc->GetFormID() : 0);
	}

	bool IsMuted(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		{
			std::scoped_lock lock(g_muteLock);
			if (const auto it = g_muteAsked.find(a_actor->GetFormID()); it != g_muteAsked.end()) {
				return it->second;  // asked, not applied yet
			}
		}
		return Sounds::IsMuted(a_actor->GetFormID());  // read from a Papyrus thread, as IsInScene
	}

	// Which object of a type a_actor wears (its file name), and choosing it.
	bool SetEquipObjectID(std::monostate, RE::Actor* a_actor, std::string a_type, std::string a_id)
	{
		return a_actor && EquipObjects::SetChosen(a_actor->GetFormID(), std::move(a_type), std::move(a_id));
	}

	std::string GetEquipObjectID(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		return a_actor ? EquipObjects::Chosen(a_actor->GetFormID(), std::move(a_type)) : std::string{};
	}

	// OStim's OData.GetEquipObjectPairs / GetEquipObjectName / SetEquipObjectID,
	// by form ID (an actor base, a race, 0x0 men, 0x1 women; the player's
	// base is 0x7). SetEquipObjectID is 4Stim's by actor, so OStim's is
	// SetFormEquipObjectID here.
	std::vector<std::string> GetEquipObjectPairs(std::monostate, std::int32_t a_formID, std::string a_type)
	{
		return EquipObjects::PairsFor(static_cast<std::uint32_t>(a_formID), std::move(a_type));
	}

	std::string GetEquipObjectName(std::monostate, std::int32_t a_formID, std::string a_type)
	{
		return EquipObjects::NameFor(static_cast<std::uint32_t>(a_formID), std::move(a_type));
	}

	void SetFormEquipObjectID(std::monostate, std::int32_t a_formID, std::string a_type, std::string a_id)
	{
		EquipObjects::SetFor(static_cast<std::uint32_t>(a_formID), std::move(a_type), std::move(a_id));
	}
}
