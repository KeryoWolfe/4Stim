#include "Core/Core.h"

namespace Core
{
	void RestartHeels(std::uint32_t a_id, const char* a_why)
	{
		if (!g_vm) {
			return;
		}
		REX::INFO("Heels: {:08X} back on ({})", a_id, a_why);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "SetHeels"sv, callback, static_cast<std::int32_t>(a_id), true);
	}

	// Whether the High Heels System's plugin is there (its Papyrus API turns
	// an actor's height off and on).
	bool HHSInstalled()
	{
		static const bool installed = std::filesystem::exists("Data/F4SE/Plugins/HHS.dll");
		return installed;
	}

	// Turns role a_role's heel height off (or back on) through HHS. Main thread.
	void SetHeelsOff(ActiveScene& a_active, std::size_t a_role, bool a_off)
	{
		a_active.heelsOff.resize(a_active.actors.size(), false);
		if (!g_vm || a_role >= a_active.actors.size() || a_active.heelsOff[a_role] == a_off) {
			return;
		}
		a_active.heelsOff[a_role] = a_off;
		REX::INFO("Heels: {:08X} {} (High Heels System)", a_active.actors[a_role], a_off ? "off: feet not on the ground" : "back on");
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "SetHeels"sv, callback, static_cast<std::int32_t>(a_active.actors[a_role]), !a_off);
	}

	ActorSpot SpotOf(const ActiveScene& a_active, std::size_t a_role)
	{
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
		const auto id = settled ? settled->id : a_active.sceneID;
		const auto o = Alignment::Get(a_active.alignKey, id, a_role);
		// As OStim's alignActor: the role's offset from the scene file plus
		// the alignment.
		SceneRegistry::Position base;
		if (settled && a_role < settled->actors.size()) {
			base = settled->actors[a_role].offset;
			// OStim's scaleOffsetWithFurniture: on furniture, the offset grows
			// and shrinks with it.
			if (settled->scaleOffsetWithFurniture && !a_active.furnitureType.empty()) {
				base.x *= a_active.furnitureRefScale;
				base.y *= a_active.furnitureRefScale;
				base.z *= a_active.furnitureRefScale;
			}
		}
		const float x = o.x + base.x, y = o.y + base.y, z = o.z + base.z, r = o.rot + base.r;
		// On a bed: its own scenes at the edge spot, floor scenes along the middle.
		auto  center = a_active.center;
		float heading = a_active.heading;
		if (a_active.furnitureSpots && settled) {
			const auto& chosen = settled->furniture.empty() ? a_active.furnitureSpots->floor : a_active.furnitureSpots->furniture;
			center = chosen.position;
			heading = chosen.heading;
		}
		const float c = std::cos(heading), s = std::sin(heading);
		ActorSpot spot;
		// Facing heading h, forward is (sin h, cos h) and right is (cos h, -sin h).
		spot.position = { center.x + x * c + y * s, center.y - x * s + y * c, center.z + z };
		spot.heading = heading + r * PI_F / 180.0F;
		// As OStim: heels come off an actor whose feet aren't on the ground
		// (lying, kneeling...). With the High Heels System, its own height is
		// turned off (LockScene, HHS's StopHeight_HHS, as OStim takes the
		// NiOverride offset off); without it, their spot is lowered by it.
		if (!HHSInstalled() && a_role < a_active.heels.size() && a_active.heels[a_role] > 0.0F && settled && a_role < settled->actors.size() &&
			!settled->actors[a_role].feetOnGround) {
			spot.position.z -= a_active.heels[a_role];
		}
		spot.scale = o.scale;
		return spot;
	}

	// Reads each actor's heels and builds the alignment key for the scene
	// playing, as OStim's rebuildAlignmentKey: sex, NPC height, and heel
	// height unless the heels are off (feet not on the ground).
	void RefreshActors(ActiveScene& a_active)
	{
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
		a_active.heels.resize(a_active.actors.size(), 0.0F);
		std::vector<Alignment::ActorInfo> info;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role]);
			// As OStim's getActorConditions, read fresh: what they are and
			// have now (a penis put on or taken off mid-scene), for the
			// navigation and the picker, at every scene change.
			if (actor) {
				ActorProperties::Remember(actor);
			}
			a_active.heels[role] = Scaling::HeelHeight(actor);
			const bool feetOnGround = settled && role < settled->actors.size() && settled->actors[role].feetOnGround;
			info.push_back({ SceneRegistry::SexOf(actor), Scaling::NPCHeight(actor), feetOnGround ? a_active.heels[role] : 0.0F });
		}
		a_active.alignKey = Alignment::KeyFor(info);
	}

	void LockScene(ActiveScene& a_active)
	{
		if (!g_vm) {
			return;
		}
		RefreshActors(a_active);
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto id = a_active.actors[role];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			const auto spot = SpotOf(a_active, role);
			// HHS on only while they wear heels with their feet on the ground.
			// HHS ignores shoes coming off while its height is off, and turning
			// it back on restores the old height: so with the shoes off it
			// stays off (else they float), and comes back on with the shoes.
			if (HHSInstalled() && role < a_active.heels.size()) {
				a_active.hadHeels.resize(a_active.actors.size(), false);
				if (a_active.heels[role] > 0.0F) {
					a_active.hadHeels[role] = true;
				}
				if (a_active.hadHeels[role]) {
					const bool feet = settled && role < settled->actors.size() && settled->actors[role].feetOnGround;
					SetHeelsOff(a_active, role, a_active.heels[role] <= 0.0F || !feet);
				}
			}
			// The penis bend (OStim's setSoSBend): the alignment's, else the scene's.
			{
				const auto found = settled ? Alignment::Find(a_active.alignKey, settled->id, role) : std::nullopt;
				PenisBend::Set(id, found ? found->bend : settled && role < settled->actors.size() ? settled->actors[role].penisBend : 0.0F, found.has_value());
			}
			// OStim's look directions (its eyeballModifierOverride), for this scene.
			if (settled && role < settled->actors.size()) {
				EyeLook::Set(id, settled->actors[role].lookUp, settled->actors[role].lookLeft);
			} else {
				EyeLook::Set(id, 0.0F, 0.0F);
			}
			// The actor's scale, only when it changes. As OStim's
			// ThreadActor::scale: the role's scale times the alignment's,
			// over their own NPC height, and with heels on, times
			// scaleHeight / (scaleHeight + heels); with SetScaling=1 their
			// own scale times the alignment's.
			if (role < a_active.baseScale.size()) {
				// OStim's setScaleMult(alignment.scale * furnitureScaleMult).
				const float mult = spot.scale * a_active.furnitureScaleMult;
				float       wanted = a_active.baseScale[role] * mult;
				if (!Scaling::Settings().disabled && settled && role < settled->actors.size()) {
					const auto& part = settled->actors[role];
					wanted = part.scale * mult / Scaling::NPCHeight(actor);
					if (role < a_active.heels.size() && a_active.heels[role] > 0.0F && part.feetOnGround) {
						wanted *= part.scaleHeight / (part.scaleHeight + a_active.heels[role]);
					}
				}
				const float current = a_active.appliedScale[role] > 0.0F ? a_active.appliedScale[role] : a_active.baseScale[role];
				if (std::fabs(wanted - current) > 0.001F) {
					SetActorScale(actor, wanted);
					a_active.appliedScale[role] = wanted;
					// With what the height is made of, to check NPC height in game
					// (parity audit 2 §21: OStim scales by the base's height;
					// whether Fallout 4 also uses heightMax or the race's height
					// is unverified): the base's height and height max, and the
					// 3D's scale before this change.
					const auto npc = actor->GetNPC();
					const auto root = actor->Get3D();
					REX::INFO("Scaling: {:08X} (role {}) scaled to {:.3f} (heels {:.1f}, feet on the ground {}; NPC height {:.3f}, height max {:.3f}, drawn at scale {:.3f} before)", id, role, wanted,
						role < a_active.heels.size() ? a_active.heels[role] : 0.0F, settled && role < settled->actors.size() && settled->actors[role].feetOnGround,
						npc ? npc->height : 0.0F, npc ? npc->heightMax : 0.0F, root ? root->world.scale : 0.0F);
				}
			}
			if (!g_lockScenes) {
				continue;
			}
			FaceHeading(actor, spot.heading);  // face the right way first; the translation then holds it
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			if (!g_settings.translationLock) {
				// bTranslationLock=0 (finding a shake): put on the spot once, with
				// no translation holding them there every frame; the scene
				// guard's watchdog puts them back if they drift.
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "Unlock"sv, callback, static_cast<std::int32_t>(id));
				actor->SetPosition(spot.position, true);
				continue;
			}
			Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "LockInPlace"sv, callback, static_cast<std::int32_t>(id), spot.position.x, spot.position.y,
				spot.position.z, spot.heading * 180.0F / PI_F);
		}
	}

	// ObjectReference.SetScale. Called directly (roadmap 2b.6: at once, where
	// the Papyrus call ran a frame or more later, after whatever came next
	// had already read the old scale), or through Papyrus as before with
	// bDirectNatives=0. Main thread.
	void SetActorScale(RE::Actor* a_actor, float a_scale)
	{
		if (g_settings.directNatives) {
			a_actor->SetScale(a_scale);
		} else {
			CallActorMethod(a_actor, "ObjectReference"sv, "SetScale"sv, a_scale);
		}
	}

	// Actors leaving a scene: their own scale back (and their heels).
	void RestoreScales(const ActiveScene& a_active)
	{
		for (std::size_t role = 0; role < a_active.actors.size() && role < a_active.heelsOff.size(); ++role) {
			if (a_active.heelsOff[role]) {
				// Back on now if they wear heels; else once they're dressed
				// again (HeelsChanged, from the redress).
				const auto id = a_active.actors[role];
				if (Scaling::HeelHeight(RE::TESForm::GetFormByID<RE::Actor>(id)) > 0.0F) {
					RestartHeels(id, "scene over");
				} else {
					std::scoped_lock lock(g_heelsWaitingLock);
					g_heelsWaiting.insert(id);
					REX::INFO("Heels: {:08X} stay off until their shoes are back on", id);
				}
			}
		}
		for (std::size_t role = 0; role < a_active.actors.size() && role < a_active.appliedScale.size(); ++role) {
			if (a_active.appliedScale[role] > 0.0F) {
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role])) {
					SetActorScale(actor, a_active.baseScale[role]);
				}
			}
		}
	}

	// ---- Changing furniture mid-scene (OStim's changeFurniture) ----
	// The scene leaves its furniture (free again, clutter put back) and goes
	// on a_piece with a_sceneID, or a random intro / idle scene that fits
	// there (OStim's: on a bed one with no one standing). Main thread.
	bool MoveSceneToFurniture(ActiveScene& a_active, const Furniture::Found& a_piece, const std::string& a_sceneID)
	{
		if (a_piece.ref && a_piece.ref == a_active.furnitureRef) {
			return false;  // as OStim: already on it
		}
		const auto ref = a_piece.ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_piece.ref) : nullptr;
		if (!ref && !a_piece.spot) {
			REX::WARN("Furniture change: {:08X} is gone", a_piece.ref);
			return false;
		}
		std::shared_ptr<const SceneRegistry::Scene> scene;
		std::string                                 why = "asked for";
		if (!a_sceneID.empty()) {
			scene = SceneRegistry::Find(a_sceneID);
			if (scene && (scene->actors.size() != a_active.actors.size() || !SexesFit(*scene, a_active.actors) || !SceneFitsFurniture(*scene, a_piece.type))) {
				REX::WARN("Furniture change: \"{}\" doesn't fit these actors on {}, picking one", a_sceneID, a_piece.type);
				scene.reset();
			}
		}
		if (!scene) {
			scene = PickStartScene(ScenesForRunning(a_active.actors, a_piece.type), a_piece.type, true, why);
		}
		if (!scene) {
			REX::WARN("Furniture change: no scene fits these {} actor(s) on {}", a_active.actors.size(), a_piece.type);
			Notify(TR("$4stim_no_scene_there", "No scene fits them there"));
			return false;
		}
		REX::INFO("Furniture change: \"{}\" from {} ({:08X}) to {} ({:08X}) with \"{}\" ({})", a_active.sceneID,
			a_active.furnitureType.empty() ? std::string("the floor") : a_active.furnitureType, a_active.furnitureRef, a_piece.type, a_piece.ref, scene->id, why);
		FreeFurniture(a_active);

		// The new spot, as for a scene started there (PlaceOnPendingFurniture).
		const auto  player = RE::PlayerCharacter::GetSingleton();
		const auto  near = player ? player->GetPosition() : a_active.center;
		const bool  floorScene = scene->furniture.empty();
		auto        spot = Furniture::SpotOf(a_piece, near, floorScene);
		const auto& o = scene->furnitureOffset;
		{
			const float c = std::cos(spot.heading), s = std::sin(spot.heading);
			spot.position.x += o[0] * c + o[1] * s;
			spot.position.y += -o[0] * s + o[1] * c;
			spot.position.z += o[2];
			spot.heading += o[3] * PI_F / 180.0F;
		}
		a_active.furnitureSpots.reset();
		if (ref && Furniture::IsEdgeAnchored(a_piece.type)) {
			const auto other = Furniture::SpotFor(ref, a_piece.type, near, !floorScene);
			auto       sorted = a_active.actors;
			std::ranges::sort(sorted);
			a_active.furnitureSpots = FurnitureSpots{ sorted, floorScene ? other : spot, floorScene ? spot : other };
		}
		if (g_settings.resetClutter) {
			a_active.clutter = Furniture::SnapshotClutter(spot.position, g_settings.resetClutterRadius);  // before anyone is moved there
		}
		a_active.furnitureRef = a_piece.ref;
		a_active.furnitureType = a_piece.type;
		a_active.furnitureFixedSpot = a_piece.spot;
		SetFurnitureScales(a_active, a_piece.ref, a_piece.type);
		a_active.center = spot.position;
		a_active.heading = spot.heading;
		BlockFurniture(a_piece.ref);
		IgnoreFurnitureCollision(a_piece.ref, a_active.actors, a_piece.type);

		auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_active.actors.front()) {
			focused.furniture = a_piece.type;
			SetPlayerScene(focused);
		}
		// A sequence or a picked transition destination doesn't carry over.
		if (InterruptSequence(a_active, "the scene changed furniture")) {
			return false;  // the scene is ending (EndAfterSequence)
		}
		a_active.queuedScene.clear();
		PlayOnActiveScene(a_active, scene->id, -1);
		// Everyone over there now (the scene lock follows; without it, they stay).
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role])) {
				const auto at = SpotOf(a_active, role);
				NotePlayerSpotBefore(actor);
				actor->SetPosition(at.position, true);
				FaceHeading(actor, at.heading);
			}
		}
		LockScene(a_active);
		// As OStim (endSceneMode / startSceneMode): the free camera starts
		// again where the player now is.
		if (std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); })) {
			RecenterFreeCamera();
		}
		ArmAutoplay(a_active);
		if (a_active.autoMode.on) {
			a_active.autoMode.cooldown = std::max(a_active.autoMode.cooldown, g_autoConfig.sceneMin);
		}
		SceneEvents::FurnitureChanged(a_active.actors, a_piece.ref, a_piece.type, a_active.sceneID, a_active.threadID);
		ApiForms::FurnitureChanged(a_active.actors, {}, a_piece.type);
		return true;
	}

	// A furniture change asked for (the HUD, the picker, Papyrus): done on
	// the main thread, behind a fade for a scene with the player
	// (SetUseFades, as OStim's changeFurniture).
	struct FurnitureChange
	{
		std::uint32_t    role0 = 0;  // the scene's
		Furniture::Found piece;
		std::string      sceneID;
	};
	std::mutex                     g_furnitureChangeLock;
	std::optional<FurnitureChange> g_furnitureChange;

	void ApplyFurnitureChange()
	{
		std::optional<FurnitureChange> change;
		{
			std::scoped_lock lock(g_furnitureChangeLock);
			change = std::exchange(g_furnitureChange, std::nullopt);
		}
		if (!change) {
			return;
		}
		if (const auto active = FindActiveScene(change->role0)) {
			MoveSceneToFurniture(*active, change->piece, change->sceneID);
		}
	}

	// Main thread.
	void RequestFurnitureChange(std::uint32_t a_role0, Furniture::Found a_piece, std::string a_sceneID)
	{
		const auto active = FindActiveScene(a_role0);
		if (!active) {
			return;
		}
		const bool withPlayer = std::ranges::any_of(active->actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		{
			std::scoped_lock lock(g_furnitureChangeLock);
			g_furnitureChange = FurnitureChange{ active->actors.front(), std::move(a_piece), std::move(a_sceneID) };
		}
		if (withPlayer && g_settings.useFades && g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "ChangeFurnitureFaded"sv, callback);  // FadeToBlack, ApplyFurnitureChange, FadeFromBlack
		} else {
			ApplyFurnitureChange();
		}
	}

	void UnlockActors(const std::vector<std::uint32_t>& a_ids)
	{
		ScenePackage::Remove(a_ids);  // before ReleaseFromScene's EvaluatePackage hands them back their routine
		if (!g_vm) {
			return;
		}
		for (const auto id : a_ids) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "Unlock"sv, callback, static_cast<std::int32_t>(id));
			// Back to their routine right away: end the do-nothing package the
			// scene gave them (LeaveFurniture) and the scene guard's hold on
			// their idles (packageIdleTimer, renewed to 30 s every check),
			// which left NPCs standing still for a long while after a scene.
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (actor && !IsPlayer(actor)) {
				actor->EndInterruptPackage(false);
				const auto process = actor->currentProcess;
				if (const auto data = process ? process->middleHigh : nullptr) {
					data->packageIdleTimer = 0.0F;
				}
			}
		}
	}

	// A scene ended. As OStim does with "Reset position" (on by default):
	// each NPC goes back to where they stood before the scene, facing the way
	// they faced (FourStimScene.ResetPosition: a translation there at a huge
	// speed, OStim's setPosition). The player goes back only if the scene
	// moved them onto furniture (NotePlayerSpotBefore; natively, as they were
	// placed); otherwise the scene was built on their own spot. That also takes them out of each other
	// before their collision with each other comes back (RestoreCollision
	// waits for this). NPCs with nowhere recorded (a scene started again
	// from a save) step 70 units off the spot instead, in the first clear
	// direction, so no one is left stuck inside someone else.
	void MoveApart(const ActiveScene& a_ended)
	{
		if (!g_vm) {
			return;
		}
		std::vector<std::uint32_t> unplaced;
		for (const auto id : a_ended.actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			const auto before = g_positionBefore.find(id);
			if (actor && IsPlayer(actor)) {
				if (before != g_positionBefore.end()) {
					if (g_resetPosition && !(a_ended.interrupted && g_stayWhenInterrupted)) {
						REX::INFO("Scene end: the player back to where they stood before the furniture scene");
						PlaceActorAt(actor, Furniture::Spot{ before->second.position, before->second.heading });
					}
					g_positionBefore.erase(before);
				}
				continue;
			}
			if (!actor) {
				continue;
			}
			if (before != g_positionBefore.end()) {
				if (g_resetPosition && !(a_ended.interrupted && g_stayWhenInterrupted)) {
					const auto& b = before->second;
					REX::INFO("Scene end: {:08X} back to where they stood before the scene", id);
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "ResetPosition"sv, callback, static_cast<std::int32_t>(id), b.position.x, b.position.y,
						b.position.z, b.heading * 180.0F / PI_F);
				} else {
					unplaced.push_back(id);
				}
				g_positionBefore.erase(before);
			} else {
				unplaced.push_back(id);
			}
		}
		if (a_ended.actors.size() < 2 || unplaced.empty()) {
			return;
		}
		std::size_t stay = 0;
		for (std::size_t role = 0; role < a_ended.actors.size(); ++role) {
			if (IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a_ended.actors[role]))) {
				stay = role;
			}
		}
		constexpr float DISTANCE = 70.0F;
		const std::array<float, 4> directions{ PI_F, -PI_F * 0.5F, PI_F * 0.5F, 0.0F };  // behind, left, right, ahead
		std::size_t                next = 0;
		for (std::size_t role = 0; role < a_ended.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_ended.actors[role]);
			if (role == stay || !actor || IsPlayer(actor) || std::ranges::find(unplaced, actor->GetFormID()) == unplaced.end()) {
				continue;
			}
			bool moved = false;
			for (std::size_t tries = 0; tries < directions.size() && !moved; ++tries) {
				const float        angle = a_ended.heading + directions[(next + tries) % directions.size()];
				const RE::NiPoint3 to{ a_ended.center.x + DISTANCE * std::sin(angle), a_ended.center.y + DISTANCE * std::cos(angle), a_ended.center.z };
				const RE::NiPoint3 up{ 0.0F, 0.0F, 40.0F };
				if (Furniture::ClearFraction(actor, a_ended.center + up, to + up) < 0.99F) {
					continue;
				}
				REX::INFO("Scene end: {:08X} steps off the spot, {:.0f} units away", actor->GetFormID(), DISTANCE);
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "MoveApart"sv, callback, static_cast<std::int32_t>(actor->GetFormID()), to.x, to.y, to.z);
				next = (next + tries + 1) % directions.size();
				moved = true;
			}
			if (!moved) {
				REX::INFO("Scene end: {:08X} has nowhere clear to step to; left on the spot", actor->GetFormID());
			}
		}
	}

	// The furniture or idle marker a_actor is using, if any (nullptr if none
	// or if the handle no longer resolves).
	RE::NiPointer<RE::TESObjectREFR> FurnitureOf(RE::Actor* a_actor)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		const auto data = process ? process->middleHigh : nullptr;
		if (!data) {
			return nullptr;
		}
		if (auto ref = data->occupiedFurniture.get()) {
			return ref;
		}
		return data->currentFurniture.get();
	}

	// An NPC using furniture or an idle marker (a workbench, a broom, a
	// chair...) stays in it under any idle we play, props and all: get them
	// out on the spot, and give their AI a do-nothing package so it doesn't
	// walk them back. Main thread. True if they were in something.
	bool LeaveFurniture(RE::Actor* a_actor, bool a_doNothing)
	{
		const auto furniture = FurnitureOf(a_actor);
		if (furniture) {
			const auto base = furniture->GetObjectReference();
			REX::INFO("Furniture use: {:08X} is using {:08X} (base {:08X} \"{}\"), taking them out", a_actor->GetFormID(), furniture->GetFormID(),
				base ? base->GetFormID() : 0, base && base->GetFormEditorID() ? base->GetFormEditorID() : "");
			g_guardKicks[a_actor->GetFormID()].takenOutOf = furniture->GetFormID();
			if (Compat::CanStopInteractingQuick()) {
				a_actor->StopInteractingQuick(true, false, true);
			} else if (g_vm) {
				// 1.10.163 (no StopInteractingQuick there, docs/OLDGEN.md): a
				// tester's NPC stayed in a mirror's idle marker under every
				// scene. Moved onto their own spot through Papyrus instead,
				// which takes an actor out of furniture.
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "LeaveFurnitureByID"sv, callback, static_cast<std::int32_t>(a_actor->GetFormID()));
			}
		}
		if (a_doNothing || furniture) {
			a_actor->InitiateDoNothingPackage();
		}
		return furniture != nullptr;
	}


	// Once a second: anyone still on the scene package who isn't in a scene
	// any more (ScenePackage::Prune) gets their own packages back now.
	void PruneScenePackage(float a_seconds)
	{
		static float clock = 0.0F;
		if ((clock += a_seconds) < 1.0F) {
			return;
		}
		clock = 0.0F;
		std::vector<std::uint32_t> inScenes;
		for (const auto& active : g_activeScenes) {
			inScenes.insert(inScenes.end(), active.actors.begin(), active.actors.end());
		}
		for (const auto id : ScenePackage::Prune(inScenes)) {
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
				CallActorMethod(actor, "Actor"sv, "EvaluatePackage"sv, false);
			}
		}
	}
}
