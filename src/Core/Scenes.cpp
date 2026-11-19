#include "Core/Core.h"

namespace Core
{
	// Turns a_actor to a_heading (radians). Like OStim: Actor::SetHeading
	// only works on the player; NPCs get their reference angle set.
	void FaceHeading(RE::Actor* a_actor, float a_heading)
	{
		if (!a_actor) {
			return;
		}
		if (a_actor == RE::PlayerCharacter::GetSingleton()) {
			a_actor->SetHeading(a_heading);
		} else {
			a_actor->SetAngleOnReference(RE::NiPoint3{ 0.0F, 0.0F, a_heading });
		}
	}

	// A drawn weapon put away, as OStim's lock does for the player
	// (sheatheWeaponry: "rae WeaponSheathe"), here for every scene actor: the
	// game's own sheathe (what Skyrim's Actor.SheatheWeapon calls; Fallout
	// 4's Papyrus has no SheatheWeapon). Taking the weapon off isn't enough:
	// an NPC who drew stays in the drawn state with bare fists, and that
	// state's arm and body pose shows over the scene's animations (the
	// owner's log: both scenes where an actor "barely responded" and was
	// turned 90 degrees in a chair had her weapon out the whole time, put
	// away by unequipping three times to no effect). Main thread.
	bool SheatheNow(RE::Actor* a_actor, std::string_view a_why)
	{
		if (!a_actor || !a_actor->GetWeaponMagicDrawn()) {
			return false;
		}
		a_actor->DrawWeaponMagicHands(false);
		REX::INFO("Weapons: {:08X} puts their weapon away ({})", a_actor->GetFormID(), a_why);
		return true;
	}

	std::mutex  g_playerSceneLock;
	PlayerScene g_playerScene;

	PlayerScene GetPlayerScene()
	{
		std::scoped_lock lock(g_playerSceneLock);
		return g_playerScene;
	}

	void SetPlayerScene(PlayerScene a_scene)
	{
		{
			std::scoped_lock lock(g_playerSceneLock);
			g_playerScene = std::move(a_scene);
		}
		HUD::OnFocusedSceneChanged();
	}

	// Clears the focused scene if a_actorID is in it.
	void ClearFocusIfIn(std::uint32_t a_actorID)
	{
		const auto focused = GetPlayerScene();
		const auto ids = focused.ActorIDs();
		if (focused.Active() && std::ranges::find(ids, a_actorID) != ids.end()) {
			SetPlayerScene({});
		}
	}

	// ---- Roles ----
	// Who may take each role of a scene (docs/ACTORS.md), as OStim: the
	// actor's type, what they have for the role's requirements (their actor
	// properties), and, with SetOnlyGayAnimsInGayScenes, the role's sex.

	std::vector<SceneRegistry::ActorCondition> ConditionsOf(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<SceneRegistry::ActorCondition> conditions;
		for (const auto id : a_ids) {
			conditions.push_back(ActorProperties::Cached(id));
		}
		return conditions;
	}

	// The filter for the picker's lists: these actors, roles open (a new
	// scene, the player's place kept if it's chosen) or already given (a
	// running one).
	SceneRegistry::ListFilter SexFilter(const std::vector<std::uint32_t>& a_ids, bool a_fixedOrder)
	{
		SceneRegistry::ListFilter filter;
		filter.fixedOrder = a_fixedOrder;
		filter.actors = ConditionsOf(a_ids);
		if (!a_fixedOrder) {
			filter.pinned = PlayerPin(a_ids);
		}
		return filter;
	}

	// Whether the actors of a running scene (role order) may play a_scene.
	bool SexesFit(const SceneRegistry::Scene& a_scene, const std::vector<std::uint32_t>& a_ids)
	{
		return SceneRegistry::Fits(a_scene.actors, ConditionsOf(a_ids));
	}

	// FurnitureFits, plus the furniture's own rules for floor scenes played
	// on it (a mattress only takes lying scenes).
	bool SceneFitsFurniture(const SceneRegistry::Scene& a_scene, const std::string& a_here)
	{
		if (!FurnitureFits(a_scene.furniture, a_here)) {
			return false;
		}
		return !a_scene.furniture.empty() || a_here.empty() || Furniture::AllowsFloorScene(a_here, a_scene);
	}

	bool FurnitureFits(const std::string& a_sceneFurniture, const std::string& a_here)
	{
		if (a_sceneFurniture.empty()) {
			// Off furniture; or on furniture whose type chain reaches "none"
			// (beds, like OStim): scenes without furniture play there too.
			return a_here.empty() || Furniture::IsA(a_here, "none");
		}
		return !a_here.empty() && Furniture::IsA(a_here, a_sceneFurniture);
	}

	std::vector<Furniture::Found> PickerFurniture()
	{
		std::scoped_lock lock(g_pickerFurnitureLock);
		return g_pickerFurniture;
	}

	// The nearest found piece a scene for a_type can be played on.
	const Furniture::Found* FurnitureFor(const std::vector<Furniture::Found>& a_found, const std::string& a_type)
	{
		for (const auto& f : a_found) {  // nearest first
			if (Furniture::IsA(f.type, a_type)) {
				return &f;
			}
		}
		return nullptr;
	}

	// Scenes a running scene's actors (in their roles) can move to on
	// furniture of type a_furniture ("" = off furniture).
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForRunning(const std::vector<std::uint32_t>& a_ids, const std::string& a_furniture)
	{
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> out;
		for (const auto& summary : SceneRegistry::List(a_ids.size(), SexFilter(a_ids, true))) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && SceneFitsFurniture(*scene, a_furniture)) {
				out.push_back(scene);
			}
		}
		return out;
	}

	// The scene a scene starts with on a_here ("" = off furniture), as
	// OStim's thread starters and changeFurniture pick it: a random one of
	// a_scenes (no transitions, nothing marked noRandomSelection), tagged
	// "intro" (SetUseIntroScenes) or else "idle" when a_tagged; off furniture
	// with someone standing, on a bed with no one standing. Departure from
	// OStim, which gives up when nothing qualifies: 4Stim falls back step by
	// step (intro -> idle -> any scene, then without the standing rule),
	// since most converted packs have no intro scenes yet. a_why: which step.
	std::shared_ptr<const SceneRegistry::Scene> PickStartScene(std::vector<std::shared_ptr<const SceneRegistry::Scene>> a_scenes, const std::string& a_here,
		bool a_tagged, std::string& a_why)
	{
		std::erase_if(a_scenes, [](const auto& a_scene) { return a_scene->IsTransition() || a_scene->noRandomSelection; });
		const bool onBed = !a_here.empty() && Furniture::IsA(a_here, "bed");
		auto standingRule = [&](const SceneRegistry::Scene& a_scene) {
			const bool standing = std::ranges::any_of(a_scene.actors, [](const SceneRegistry::SceneActor& a) { return a.HasTag("standing"); });
			return a_here.empty() ? standing : onBed ? !standing : true;
		};
		auto tagged = [](const SceneRegistry::Scene& a_scene, const std::string& a_tag) {
			return std::ranges::any_of(a_scene.tags, [&](const std::string& t) { return _stricmp(t.c_str(), a_tag.c_str()) == 0; });
		};
		std::vector<std::string> tags;
		if (a_tagged) {
			if (g_settings.useIntroScenes) {
				tags.emplace_back("intro");
			}
			tags.emplace_back("idle");
		}
		tags.emplace_back("");  // any scene
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
		for (const bool rule : { true, false }) {
			for (const auto& tag : tags) {
				pool.clear();
				std::ranges::copy_if(a_scenes, std::back_inserter(pool), [&](const auto& a_scene) {
					return (tag.empty() || tagged(*a_scene, tag)) && (!rule || standingRule(*a_scene));
				});
				if (!pool.empty()) {
					a_why = std::format("{}{}: {} {} scene(s)", tag.empty() ? std::string("any") : tag, rule ? "" : ", without the standing rule", pool.size(),
						a_here.empty() ? "floor" : a_here);
					static std::mt19937 rng{ std::random_device{}() };
					return pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)];
				}
			}
		}
		return nullptr;
	}

	// OStim's disableUse: no one else can use the furniture a scene is on.
	void BlockFurniture(std::uint32_t a_ref)
	{
		if (!a_ref || !g_vm) {
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimFurniture"sv, "Block"sv, callback, static_cast<std::int32_t>(a_ref));
	}

	// OStim's enableUse, for furniture 4Stim blocked.
	void UnblockFurniture(std::uint32_t a_ref)
	{
		{
			std::scoped_lock lock(g_blockedFurnitureLock);
			if (!a_ref || g_blockedFurniture.erase(a_ref) == 0) {
				return;
			}
		}
		if (g_vm) {
			REX::INFO("Furniture: {:08X} free to use again", a_ref);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimFurniture"sv, "Unblock"sv, callback, static_cast<std::int32_t>(a_ref));
		}
	}

	bool FurnitureBlocked(std::uint32_t a_ref)
	{
		std::scoped_lock lock(g_blockedFurnitureLock);
		return a_ref && g_blockedFurniture.contains(a_ref);
	}

	// Notes the clutter around a_spot for the scene a_actors are about to
	// start (main thread, before they're moved there).
	void NoteClutterFor(std::vector<std::uint32_t> a_actors, const RE::NiPoint3& a_spot)
	{
		if (!g_settings.resetClutter) {
			return;
		}
		std::ranges::sort(a_actors);
		{
			std::scoped_lock lock(g_pendingClutterLock);
			if (const auto noted = g_pendingClutter.Peek(a_actors); noted && noted->fromSave) {
				return;  // a scene from a save: its clutter was noted before the save
			}
		}
		auto items = Furniture::SnapshotClutter(a_spot, g_settings.resetClutterRadius);
		std::scoped_lock lock(g_pendingClutterLock);
		g_pendingClutter.Put(a_actors, PendingClutter{ a_actors, std::move(items), false });
	}

	std::optional<PendingFurniture> PendingFurnitureFor(const std::vector<std::uint32_t>& a_actors, bool a_take)
	{
		std::scoped_lock lock(g_pendingFurnitureLock);
		if (a_take) {
			return g_pendingFurniture.Take(a_actors);
		}
		const auto found = g_pendingFurniture.Peek(a_actors);
		return found ? std::optional<PendingFurniture>(*found) : std::nullopt;
	}

	// Furniture for the scene a_actors are about to start.
	void SetPendingFurniture(PendingFurniture a_pending)
	{
		std::scoped_lock lock(g_pendingFurnitureLock);
		const auto actors = a_pending.actors;
		g_pendingFurniture.Put(actors, std::move(a_pending));
	}

	void ClearPendingFurniture(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_pendingFurnitureLock);
		g_pendingFurniture.Erase(a_actors);
	}

	// As OStim's ThreadActor positionBefore for the player: where they stood
	// before a scene first moved them onto furniture, so the end can put them
	// back (SetResetPosition). Kept until then; a later move doesn't replace
	// it. Main thread.
	void NotePlayerSpotBefore(RE::Actor* a_actor)
	{
		if (a_actor && IsPlayer(a_actor) && !g_positionBefore.contains(a_actor->GetFormID())) {
			g_positionBefore[a_actor->GetFormID()] = { a_actor->data.location, a_actor->data.angle.z, std::chrono::steady_clock::now() };
			REX::INFO("Furniture: the player's spot before the scene noted, to put them back at its end");
		}
	}

	// Moves a_actors onto the pending furniture's spot, if the next scene for
	// them has one. False if it doesn't.
	bool PlaceOnPendingFurniture(const std::vector<RE::Actor*>& a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		const auto pending = PendingFurnitureFor(ids, false);
		if (!pending) {
			return false;
		}
		const auto ref = pending->ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(pending->ref) : nullptr;
		if (!ref && !pending->fixedSpot) {
			REX::WARN("Furniture: {:08X} is gone, the scene is played where the actors are", pending->ref);
			return false;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto near = player ? player->GetPosition() : ref ? ref->GetPosition() : pending->fixedSpot->position;
		const Furniture::Found piece{ pending->type, {}, pending->ref, 0.0F, pending->fixedSpot };
		auto       spot = Furniture::SpotOf(piece, near, pending->floorScene);
		if (const auto& o = pending->offset; o[0] != 0.0F || o[1] != 0.0F || o[2] != 0.0F || o[3] != 0.0F) {
			// In the spot's frame: x to its right, y forward.
			const float c = std::cos(spot.heading), s = std::sin(spot.heading);
			spot.position.x += o[0] * c + o[1] * s;
			spot.position.y += -o[0] * s + o[1] * c;
			spot.position.z += o[2];
			spot.heading += o[3] * PI_F / 180.0F;
			REX::INFO("Furniture: scene offset ({}, {}, {}, {} deg)", o[0], o[1], o[2], o[3]);
		}
		// On a bed, the scene can later move between bed scenes and floor
		// scenes: keep both spots.
		g_furnitureSpots.Erase(ids);
		if (ref && Furniture::IsEdgeAnchored(pending->type)) {
			const auto other = Furniture::SpotFor(ref, pending->type, near, !pending->floorScene);
			g_furnitureSpots.Put(ids, FurnitureSpots{ ActorSlots<FurnitureSpots>::Key(ids), pending->floorScene ? other : spot, pending->floorScene ? spot : other });
		}
		// The clutter there, before anyone is moved onto it (tasks run in order).
		F4SE::GetTaskInterface()->AddTask([ids, at = spot.position]() { NoteClutterFor(ids, at); });
		for (const auto actor : a_actors) {
			NotePlayerSpotBefore(actor);
			PlaceActorAt(actor, spot);
		}
		return true;
	}

	// OStim's Thread::SetSpeed: a speed played outside a transition sets the
	// relative speed (speed / number of speeds, OStim's formula as it is).
	void NoteSpeed(ActiveScene& a_active, const std::string& a_sceneID, int a_speed)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (scene && !scene->IsTransition()) {
			a_active.relativeSpeed = scene->speeds.empty() ? -1.0F : static_cast<float>(a_speed) / static_cast<float>(scene->speeds.size());
		}
	}

	// OStim's Thread::ChangeNode: the speed a scene moved to starts at. One
	// speed: it; no relative speed yet: the scene's defaultSpeed; else the
	// relative speed over its speeds, rounded.
	int MappedSpeed(const ActiveScene& a_active, const SceneRegistry::Scene& a_to)
	{
		const int count = static_cast<int>(a_to.speeds.size());
		if (count <= 1) {
			return 0;
		}
		if (a_active.relativeSpeed < 0.0F) {
			return std::clamp(a_to.defaultSpeed, 0, count - 1);
		}
		return std::clamp(static_cast<int>(a_active.relativeSpeed * static_cast<float>(count - 1) + 0.5F), 0, count - 1);
	}

	// The speed a scene starts at: its defaultSpeed (OStim: a new thread's
	// relative speed is -1, so ChangeNode plays defaultSpeed).
	int StartSpeed(const SceneRegistry::Scene& a_scene)
	{
		return std::clamp(a_scene.defaultSpeed, 0, std::max(static_cast<int>(a_scene.speeds.size()) - 1, 0));
	}

	// ---- No gravity in scenes ----
	// Not in OStim (Skyrim's lock holds actors in the air without help).
	// The lock's translation puts the actor back on their spot every frame,
	// but the player isn't restrained (SetRestrained also freezes the look
	// input the free camera needs), so their character controller still
	// falls under gravity between frames wherever the spot is above the
	// floor: a chair scene's spot, with its alignment, holds them some units
	// up (the owner's log, 5 November: both held at 1509.1 over a floor at
	// about 1504, the player standing on nothing). Fall, put back, fall: the
	// player's whole body bouncing about six times a second, in chair scenes
	// only and whichever role he had, while the restrained NPC stayed still.
	// So scene actors' controllers have no gravity while the lock holds
	// them (the guard keeps it at 0), and get their own back with their
	// collision (RestoreCollisionNow).
	std::unordered_map<std::uint32_t, float> g_savedGravity;  // actor -> their controller's gravity before the scene (main thread)

	void HoldGravity(RE::Actor* a_actor)
	{
		const auto controller = g_lockScenes ? GetCharController(a_actor) : nullptr;
		if (!controller || controller->gravity == 0.0F) {
			return;
		}
		const PhysicsLock physics(a_actor);
		const auto        id = a_actor->GetFormID();
		if (g_savedGravity.try_emplace(id, controller->gravity).second) {
			REX::INFO("Scene lock: {:08X}'s controller gravity {:.2f} -> 0 for the scene", id, controller->gravity);
		}
		controller->gravity = 0.0F;
		controller->fallTime = 0.0F;
	}

	void RestoreGravity(RE::Actor* a_actor)
	{
		const auto it = g_savedGravity.find(a_actor->GetFormID());
		if (it == g_savedGravity.end()) {
			return;
		}
		const PhysicsLock physics(a_actor);
		if (const auto controller = GetCharController(a_actor)) {
			controller->gravity = it->second;
			REX::INFO("Scene lock: {:08X}'s controller gravity back to {:.2f}", a_actor->GetFormID(), it->second);
		}
		g_savedGravity.erase(it);
	}

	// The actor's collision back (RestoreCollisionNow): what the watch
	// changed for them, back as it was. Main thread.
	void RestoreTouched(std::uint32_t a_actor)
	{
		g_touchLogged.erase(a_actor);
		g_nearbyBodies.erase(a_actor);
		const auto it = g_touched.find(a_actor);
		if (it == g_touched.end()) {
			return;
		}
		const PhysicsLock physics(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_actor));
		std::size_t       restored = 0;
		for (const auto& touched : it->second) {
			RE::CFilter filter{};
			filter.filter = touched.filter;
			if (touched.object && touched.object->SetCollisionFilterInfo(filter)) {
				++restored;
			}
		}
		REX::INFO("Collision watch: {} of {} thing(s) {:08X} touched collide with them again", restored, it->second.size(), a_actor);
		g_touched.erase(it);
	}

	// A scene leaves its furniture (it ends, or moves to other furniture):
	// the furniture free to use again and, with SetResetClutter, the loose
	// items around it back where they lay (as OStim's changeFurniture and
	// thread end). Main thread.
	void FreeFurniture(ActiveScene& a_scene)
	{
		RestoreFurnitureCollision(a_scene.furnitureRef);
		UnblockFurniture(a_scene.furnitureRef);
		if (!a_scene.clutter.empty()) {
			if (g_settings.resetClutter) {
				Furniture::RestoreClutter(a_scene.clutter);
			}
			a_scene.clutter.clear();
		}
	}

	std::vector<Furniture::Found> FurnitureNear(const RE::NiPoint3& a_center, RE::TESObjectREFR* a_from)
	{
		auto found = Furniture::FindNear(a_center, g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.logFurniture, a_from);
		std::erase_if(found, [](const Furniture::Found& f) {
			return f.ref && std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a_scene) { return a_scene.furnitureRef == f.ref; });
		});
		for (const auto& f : found) {
			REX::INFO("Furniture: nearest {}: {:08X}, {:.0f} away", f.type, f.ref, f.distance);
		}
		return found;
	}

	// The scales furniture a_ref of type a_type gives a scene on it.
	void SetFurnitureScales(ActiveScene& a_scene, std::uint32_t a_ref, const std::string& a_type)
	{
		a_scene.furnitureScaleMult = a_type.empty() ? 1.0F : Furniture::MultiplyScale(a_type);
		const auto ref = a_ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_ref) : nullptr;
		a_scene.furnitureRefScale = ref && ref->refScale > 0 ? ref->refScale / 100.0F : 1.0F;
	}

	ActiveScene* FindActiveScene(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		return it != g_activeScenes.end() ? &*it : nullptr;
	}
	// How long the end event waits for Papyrus's release (EndPairScene /
	// EndGroupScene take 1.5 s) before it's sent anyway.
	constexpr auto RELEASE_WAIT = std::chrono::seconds(4);

	// Sends the end event for the ended threads with any of a_actors (their
	// release done, or a_late: it never came), and forgets their actors'
	// metadata (OStim's ends with the thread). Main thread.
	void SendEndedFor(const std::vector<std::uint32_t>& a_actors, bool a_late)
	{
		std::vector<EndedThread> due;
		std::erase_if(g_endedThreads, [&](EndedThread& a_ended) {
			const bool mine = std::ranges::any_of(a_ended.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
			if (mine) {
				due.push_back(std::move(a_ended));
			}
			return mine;
		});
		for (auto& ended : due) {
			{
				std::scoped_lock lock(g_threadsLock);
				if (const auto it = g_threads.find(ended.threadID); it != g_threads.end() && it->second.status == ThreadRecord::Status::kEnded) {
					g_threads.erase(it);
				}
			}
			if (a_late) {
				REX::INFO("Thread {}: end event sent without hearing the actors were released", ended.threadID);
			}
			// Actors already in a new scene keep that scene's metadata.
			std::vector<std::uint32_t> free;
			std::ranges::copy_if(ended.actors, std::back_inserter(free), [](std::uint32_t id) { return !FindActiveScene(id); });
			ForgetActorMeta(free);
			SceneEvents::SceneEnded(ended.actors, ended.sceneID, ended.threadID, ended.metadata);
		}
	}

	void TrackSceneStart(std::vector<std::uint32_t> a_actors, const std::string& a_sceneID, int a_speed)
	{
		PenisBend::NoteRestPose(a_actors);  // before the scene's animation poses them
		for (const auto id : a_actors) {
			g_visibilityLast.erase(id);  // the visibility check logs them afresh
			g_touchLogged.erase(id);     // and the collision watch
			g_nearbyBodies.erase(id);
			if (!IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id))) {
				ScenePackage::Add(id);  // scenes resumed from a save, too (SuppressInteraction adds the rest earlier)
			}
		}
		// An actor can only be in one scene: a new scene replaces an old one
		// (which leaves its furniture first).
		for (auto& old : g_activeScenes) {
			if (std::ranges::any_of(old.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); })) {
				FreeFurniture(old);
			}
		}
		std::erase_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			const bool replaced = std::ranges::any_of(a_scene.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
			if (replaced) {
				std::vector<std::uint32_t> left;
				std::ranges::copy_if(a_scene.actors, std::back_inserter(left), [&](std::uint32_t id) { return std::ranges::find(a_actors, id) == a_actors.end(); });
				Excitement::Leave(left);
				AnimCues::Stop(left);
				Expressions::Leave(left);
				Sounds::Leave(left);
				Peaks::Leave(left);
				SexToys::Leave(left);
				PlaybackSpeed::Set(left, 1.0F);
				Lights::Leave(left);
				ApiForms::SceneEnded(left);
				EquipObjects::Leave(left);
				PenisBend::Leave(left);
				PenisBend::RestoreRestPose(left);
				Erection::Leave(left);
				EyeLook::Leave(left);
				UnlockActors(left);
				RestoreScales(a_scene);
				for (const auto id : left) {
					Undress::Redress(id, false, true);
				}
				// Its thread is over (replaced, not ended: no end event, as before).
				std::scoped_lock lock(g_threadsLock);
				g_threads.erase(a_scene.threadID);
			}
			return replaced;
		});
		ActiveScene scene{ a_actors, a_sceneID };
		scene.speed = a_speed;
		NoteSpeed(scene, a_sceneID, a_speed);
		const bool hasPlayer = std::ranges::any_of(a_actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		// Its thread: the one a mod asked for with these actors (thread
		// builder), else a new one (the picker, a save's scene...).
		std::optional<ThreadRecord> start;  // a copy of what was asked for
		{
			const auto sorted = ActorSlots<int>::Key(a_actors);
			std::scoped_lock lock(g_threadsLock);
			PruneStartingLocked();
			auto it = std::ranges::find_if(g_threads, [&](const auto& a_entry) {
				return a_entry.second.status == ThreadRecord::Status::kStarting && a_entry.second.actors == sorted;
			});
			if (it == g_threads.end()) {
				ThreadRecord record;
				record.id = hasPlayer ? 0 : g_nextThreadID++;
				record.actors = sorted;
				it = g_threads.insert_or_assign(record.id, std::move(record)).first;
			} else {
				start = it->second;
			}
			it->second.status = ThreadRecord::Status::kRunning;
			it->second.roleActors = a_actors;
			scene.threadID = it->first;
			if (start) {
				// What was asked for while it was starting is done below; not twice.
				it->second.stopWhenStarted = false;
				it->second.stallWhenStarted.reset();
				it->second.autoWhenStarted.reset();
				it->second.speedWhenStarted.reset();
				it->second.sceneWhenStarted.clear();
			}
		}
		if (start) {
			scene.noAutoMode = start->noAutoMode;
			scene.noPlayerControl = start->noPlayerControl;
			scene.noUndress = start->noUndress;
			scene.noPostDialogue = start->noPostDialogue;
			scene.endAfterSequence = start->endAfterSequence;
			for (const auto id : a_actors) {
				Undress::Block(id, start->noUndress);
			}
		}
		if (const auto furniture = PendingFurnitureFor(a_actors, true)) {
			scene.furnitureRef = furniture->ref;
			scene.furnitureType = furniture->type;
			scene.furnitureFixedSpot = furniture->fixedSpot;
			SetFurnitureScales(scene, furniture->ref, furniture->type);
			{
				std::scoped_lock lock(g_pendingClutterLock);
				if (auto clutter = g_pendingClutter.Take(a_actors)) {
					scene.clutter = std::move(clutter->items);
				}
			}
			BlockFurniture(furniture->ref);
			IgnoreFurnitureCollision(furniture->ref, a_actors, furniture->type);
			scene.furnitureSpots = g_furnitureSpots.Take(a_actors);
			REX::INFO("Scene \"{}\": on furniture {:08X} ({})", a_sceneID, furniture->ref, furniture->type);
		}
		{
			std::scoped_lock lock(g_pendingSequenceLock);
			if (const auto pending = g_pendingSequence.Take(a_actors)) {
				const auto& first = pending->sequence->entries.front();
				if (_stricmp(first.scene.c_str(), a_sceneID.c_str()) == 0) {
					scene.sequence = pending->sequence;
					REX::INFO("Sequence \"{}\": started ({} scene(s))", scene.sequence->id, scene.sequence->entries.size());
				}
			}
		}
		// The spot it's played on: where role 0 was placed (everyone was
		// placed on the same spot, or the player's / first actor's own).
		if (const auto first = RE::TESForm::GetFormByID<RE::Actor>(a_actors.front())) {
			scene.center = first->data.location;
			scene.heading = first->data.angle.z;
		}
		// Better: the spot they were just placed on, if they were (the
		// player isn't moved: then the player's own spot and facing).
		const auto now = std::chrono::steady_clock::now();
		bool       fromPlacement = false;
		for (const auto id : a_actors) {
			const auto it = g_placedSpots.find(id);
			if (it != g_placedSpots.end() && now - it->second.when < std::chrono::seconds(15)) {
				if (!fromPlacement) {
					scene.center = it->second.position;
					scene.heading = it->second.heading;
					fromPlacement = true;
				}
			}
			if (it != g_placedSpots.end()) {
				g_placedSpots.erase(it);
			}
		}
		// The player is moved only onto furniture (the picker's, placed
		// above): otherwise the scene is built on their own spot. Not after
		// a placement: the game had pushed the player up out of the chair's
		// collision by then (9 units in the owner's log, 5 November, before
		// the chair was out of the way), and his spot held both actors in the
		// air over the chair for the whole scene, the player bouncing.
		for (const auto id : a_actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (IsPlayer(actor) && !fromPlacement) {
				scene.center = actor->data.location;  // the player is never moved: the scene is built on their spot
				scene.heading = actor->data.angle.z;
			}
		}
		for (const auto id : a_actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			scene.baseScale.push_back(actor && actor->refScale ? actor->refScale / 100.0F : 1.0F);
			scene.appliedScale.push_back(0.0F);
		}
		// LockScene (below) reads heels and builds the alignment key.
		g_activeScenes.push_back(std::move(scene));
		LockScene(g_activeScenes.back());
		SceneEvents::SceneStarted(a_actors, a_sceneID, g_activeScenes.back().threadID, g_activeScenes.back().speed);
		Erection::SceneStarted(a_actors);  // after the physics swaps (SceneEvents): a swapped body's erection is its swap
		WatchAnimEvents(a_actors, a_sceneID);
		auto& added = g_activeScenes.back();
		RefreshExcitement(added);
		if (const auto first = SceneRegistry::Find(added.sceneID)) {
			ApiForms::SceneStarted(added.actors, *first, hasPlayer, added.furnitureType);  // OStim's API factions (4Stim.esl)
		}
		if (const auto first = SceneRegistry::Find(added.sceneID)) {
			Undress::SceneEntered(added.actors, *first, true);
			EquipObjects::SceneEntered(added.actors, *first, true);
		}
		if (added.sequence && added.sequence->entries.front().speed != 0) {
			PlayOnActiveScene(added, added.sequence->entries.front().scene, added.sequence->entries.front().speed);
		}
		ArmAutoplay(added);
		const bool withPlayer = hasPlayer;
		if (!withPlayer && g_settings.npcSceneDuration > 0.0F) {
			added.stopTimer = g_settings.npcSceneDuration;
			StartAutoplayTicks();
		}
		if (start && start->duration > 0.0F) {
			added.stopTimer = start->duration;  // OStim's SetDuration, the player's scene too
			StartAutoplayTicks();
		}
		if (start && start->undressAtStart) {
			for (const auto id : added.actors) {
				Undress::StripAll(id, true);
			}
		}
		// OStim's Thread::evaluateAutoMode: NPC scenes always (4Stim:
		// bAutoModeNPC), the player's with SetAIControl, else solo ones with
		// SetForceAIForMasturbation and the rest with
		// SetForceAIInConsensualScenes. NoAutoMode keeps it off (StartAutoMode).
		if (!withPlayer ? g_autoConfig.npc :
		    g_autoConfig.player || (added.actors.size() == 1 ? g_autoConfig.playerSolo : g_autoConfig.playerPartner)) {
			StartAutoMode(added);
		}
		// Started again from a save: back to where it was.
		std::optional<PendingResume> resume;
		{
			std::scoped_lock lock(g_pendingResumeLock);
			resume = g_pendingResume.Take(added.actors);
		}
		if (resume) {
			for (const auto& [id, value] : resume->excitement) {
				Excitement::Set(id, value);
			}
			if (resume->autoMode != added.autoMode.on) {
				resume->autoMode ? StartAutoMode(added) : StopAutoMode(added);
			}
			if (resume->speed > 0) {
				PlayOnActiveScene(added, added.sceneID, resume->speed);
			}
			REX::INFO("Save data: \"{}\" running again (speed {}, auto mode {})", added.sceneID, resume->speed + 1, resume->autoMode);
		}
		// What a mod asked of the thread while it was starting.
		if (start) {
			if (start->stallWhenStarted) {
				added.stallClimax = *start->stallWhenStarted;
				for (const auto id : added.actors) {
					Excitement::SetThreadStalled(id, added.stallClimax);
				}
			}
			if (start->autoWhenStarted && *start->autoWhenStarted != added.autoMode.on) {
				*start->autoWhenStarted ? StartAutoMode(added) : StopAutoMode(added);
			}
			if (!start->sceneWhenStarted.empty() || start->speedWhenStarted) {
				const auto to = SceneRegistry::Find(start->sceneWhenStarted.empty() ? added.sceneID : start->sceneWhenStarted);
				if (to && to->actors.size() == added.actors.size()) {
					const int speed = std::clamp(start->speedWhenStarted.value_or(added.speed), 0, std::max(static_cast<int>(to->speeds.size()) - 1, 0));
					if (to->id != added.sceneID) {
						added.sequence.reset();
					}
					if (PlayOnActiveScene(added, to->id, speed)) {
						ArmAutoplay(added);
					}
				}
			}
			if (start->stopWhenStarted) {
				EndActiveScene(added, "asked by another mod while it was starting (FourThread.Stop)");
			}
		}
	}

	void TrackSceneStop(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		if (it == g_activeScenes.end()) {
			return;
		}
		auto ended = std::move(*it);
		g_activeScenes.erase(it);
		if (g_activeScenes.empty()) {
			SetCameraFadeOff(false);
		}
		for (const auto id : ended.actors) {
			Undress::Block(id, false);
		}
		FreeFurniture(ended);
		UnlockActors(ended.actors);
		MoveApart(ended);
		RestoreScales(ended);
		Excitement::Leave(ended.actors);
		AnimCues::Stop(ended.actors);
		Expressions::Leave(ended.actors);
		Sounds::Leave(ended.actors, !ended.noPostDialogue);  // OStim's post-scene dialogue, unless NoPostDialogue
		Peaks::Leave(ended.actors);
		SexToys::Leave(ended.actors);
		PlaybackSpeed::Set(ended.actors, 1.0F);
		Lights::Leave(ended.actors);
		EquipObjects::Leave(ended.actors);
		PenisBend::Leave(ended.actors);
		PenisBend::RestoreRestPose(ended.actors);
		Erection::Leave(ended.actors);  // before dressing: a bodysuit comes off first
		Actor3D::Leave(ended.actors);
		EyeLook::Leave(ended.actors);
		for (const auto id : ended.actors) {
			Undress::Redress(id, false, true);
		}
		SceneEvents::SceneStopped(ended.actors);
		ApiForms::SceneEnded(ended.actors);  // OStim's Core::free: out of the scene's factions
		// The thread ends; its end event waits until Papyrus has let its
		// actors go (SceneReleased, FourStimScene's EndPairScene /
		// EndGroupScene), or RELEASE_WAIT if that never comes. Until then
		// FourThread.GetMetadata and the actors' metadata still answer, as
		// OStim's do inside its end event.
		EndedThread done{ ended.threadID, ended.sceneID, ended.actors, {} };
		{
			std::scoped_lock lock(g_threadsLock);
			if (const auto record = g_threads.find(ended.threadID); record != g_threads.end() && record->second.status == ThreadRecord::Status::kRunning) {
				done.metadata = record->second.metadata;
				record->second.status = ThreadRecord::Status::kEnded;
			}
		}
		g_endedThreads.push_back(std::move(done));
		Scheduler::AfterOnMain(RELEASE_WAIT, [actors = ended.actors]() { SendEndedFor(actors, true); });
	}

	// As OStim's clearNodeQueue: a sequence interrupted (navigation, another
	// mod moving the scene) stops, and with EndAfterSequence, the scene ends
	// too. True if the scene is ending. Auto mode's routes just stop.
	bool InterruptSequence(ActiveScene& a_active, std::string_view a_why)
	{
		if (!a_active.sequence) {
			return false;
		}
		const auto sequence = std::exchange(a_active.sequence, nullptr);
		REX::INFO("Sequence \"{}\": stopped, {}", sequence->id, a_why);
		HUD::OnFocusedSceneChanged();
		if (a_active.endAfterSequence && !sequence->route) {
			EndActiveScene(a_active, "its sequence was interrupted (EndAfterSequence)");
			a_active.remaining = -1.0F;
			return true;
		}
		return false;
	}

	// A scene moved on by someone else (navigation, a speed key). Leaving
	// the scene a sequence is on ends the sequence: the player took over.
	void TrackSceneChange(std::uint32_t a_actorID, const std::string& a_sceneID, int a_speed)
	{
		const auto scene = FindActiveScene(a_actorID);
		if (!scene) {
			return;
		}
		const bool moved = _stricmp(scene->sceneID.c_str(), a_sceneID.c_str()) != 0;
		NoteSpeed(*scene, a_sceneID, a_speed);
		if (moved) {
			scene->queuedScene.clear();
			WatchAnimEvents(scene->actors, a_sceneID);
		}
		scene->sceneID = a_sceneID;
		scene->speed = a_speed;
		scene->lastPlayed = std::chrono::steady_clock::now();
		scene->checkedAfterPlay = false;
		RefreshExcitement(*scene);
		if (const auto entered = moved ? SceneRegistry::Find(a_sceneID) : nullptr) {
			Undress::SceneEntered(scene->actors, *entered, false);
			EquipObjects::SceneEntered(scene->actors, *entered, false);
			ApiForms::SceneChanged(scene->actors, *entered, scene->threadID == 0);
			for (const auto id : scene->actors) {
				PenisBend::Offset(id, 0);  // OStim's changeNode: the annotation's bend ends with the scene
				PenisBend::Aim(id, false);  // until the new animation's 4StimPenisAim says so
			}
			LockScene(*scene);  // this scene's alignment
		}
		if (moved) {
			if (InterruptSequence(*scene, std::format("the scene was moved to \"{}\"", a_sceneID))) {
				return;
			}
			ArmAutoplay(*scene);
			if (scene->autoMode.on) {
				// Picked by the player: auto mode stays a while before moving on.
				scene->autoMode.cooldown = std::max(scene->autoMode.cooldown, g_autoConfig.sceneMin);
			}
		}
	}
}
