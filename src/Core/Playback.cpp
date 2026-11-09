#include "Core/Core.h"

namespace Core
{
	void EndBrokenScenes(float a_seconds);

	// ---- Autoplay: transitions and sequences ----
	// A transition scene (one with a "destination") plays for its "length"
	// and then moves on to its destination; a sequence plays its scenes for
	// their durations, one after another. Time is counted in frames the game
	// isn't paused, so a pause menu doesn't skip ahead.
	//
	// The clock: a background thread queues one AutoplayTick task every
	// ~10 ms while anything is counting down. A task can't simply re-queue
	// itself: the game runs tasks added during its task pass in that same
	// pass, so that loops without the frame ever advancing (the game froze
	// for the whole transition, and the destination idle was lost).

	std::atomic<bool>                     g_autoplayTicking = false;   // the ticker thread is wanted
	std::atomic<bool>                     g_autoplayTickQueued = false;  // a tick task is waiting to run
	std::chrono::steady_clock::time_point g_autoplayLast;

	void AutoplayTick()
	{
		g_autoplayTickQueued = false;
		const auto now = std::chrono::steady_clock::now();
		float      elapsed = std::chrono::duration<float>(now - g_autoplayLast).count();
		g_autoplayLast = now;
		elapsed = std::min(elapsed, 0.25F);  // a hitch or a load screen doesn't skip a whole step
		if (g_slowMo) {
			elapsed *= 0.3F;  // the animations are in slow motion too
		}

		const auto ui = RE::UI::GetSingleton();
		const bool paused = ui && ui->menuMode > 0;
		static bool wasPaused = false;
		if (paused != wasPaused) {
			wasPaused = paused;
			REX::INFO("Scene clock: {} (menu mode {})", paused ? "paused" : "running", ui ? static_cast<int>(ui->menuMode) : -1);
		}
		if (!paused) {
			// AdvanceAutoplay and HandleClimax can't add or remove running
			// scenes, so the list is safe to walk while they run.
			for (auto& active : g_activeScenes) {
				if (active.remaining < 0.0F) {
					continue;
				}
				active.remaining -= elapsed;
				if (active.remaining <= 0.0F) {
					AdvanceAutoplay(active);
				}
			}
			// The animation files' annotation cues, timed here (AnimCues.h).
			for (const auto& due : AnimCues::Tick(elapsed, [](std::uint32_t a_id) { return PlaybackSpeed::RateOf(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_id)); })) {
				HandleTimedCue(due);
			}
			for (const auto id : Excitement::Tick(elapsed)) {
				if (const auto active = FindActiveScene(id)) {
					HandleClimax(*active, id);
				} else {
					Excitement::Climaxed(id, ActorProperties::FemalePostClimax(RE::TESForm::GetFormByID<RE::Actor>(id)));
				}
			}
			for (auto& active : g_activeScenes) {
				ResolveAwaitingClimaxes(active);
			}
			Sounds::Tick(elapsed);
			Peaks::Tick(elapsed);
			SexToys::Tick(elapsed);
			Expressions::Tick(elapsed);
			UpdateClimaxShake();
			KeepFadedIn();
			WatchShaking(elapsed);
			GuardScenes(elapsed);
			PruneScenePackage(elapsed);
			Undress::Tick(elapsed);
			EquipObjects::Tick(elapsed);
			for (auto& active : g_activeScenes) {
				SpeedControlTick(active, elapsed);
				AutoModeTick(active, elapsed);
			}
			EndBrokenScenes(elapsed);
			// Scenes ending after a climax, or NPC scenes whose time is up
			// (OStim's NPCSceneDuration). Ending goes through Papyrus, which
			// removes the scene later (TrackSceneStop).
			for (auto& active : g_activeScenes) {
				if (active.stopTimer >= 0.0F) {
					active.stopTimer -= elapsed;
					if (active.stopTimer < 0.0F) {
						active.stopTimer = -1.0F;
						if (active.endIn < 0.0F) {
							EndActiveScene(active, "its time is up (NPCSceneDuration)");
						}
						continue;
					}
				}
				if (active.endIn < 0.0F) {
					continue;
				}
				active.endIn -= elapsed;
				if (active.endIn <= 0.0F) {
					active.endIn = -1.0F;
					EndActiveScene(active, "after a climax");
				}
			}
		}
		Lights::Tick();  // lights follow their bones, paused or not
		PenisBend::Tick();
		Erection::Tick();
		Physics::Tick();
		EyeLook::Tick();
		{
			std::vector<std::uint32_t> everyone;
			for (const auto& active : g_activeScenes) {
				everyone.insert(everyone.end(), active.actors.begin(), active.actors.end());
			}
			Actor3D::Tick(everyone);  // a rebuilt actor gets the scene's changes back
		}
		ApiForms::Tick(elapsed);  // the excitement faction's rank
		const bool waiting = !g_activeScenes.empty();  // faces and voices run as long as any scene does
		if (!waiting && !g_climaxShake.active) {
			g_autoplayTicking = false;
		}
	}

	// As OStim's Thread::loop (Core/Thread.cpp): a scene ends when anyone in
	// it dies or is in combat, and a scene without the player ends when its
	// first actor isn't in the player's cell. Four times a second. Main thread.
	// Fallout4.esm's Health actor value (ActorValueInfo 000002D4), by form
	// ID rather than the game's actor value list (no new address needed).
	RE::ActorValueInfo* g_healthAV = nullptr;

	void EndBrokenScenes(float a_seconds)
	{
		static bool healthLooked = false;
		if (!healthLooked) {
			healthLooked = true;
			g_healthAV = RE::TESForm::GetFormByID<RE::ActorValueInfo>(0x000002D4);
			if (!g_healthAV) {
				REX::WARN("Scene end: Fallout4.esm's Health actor value (000002D4) not found: scenes don't end when someone is hurt");
			}
		}
		static float wait = 0.0F;
		if ((wait -= a_seconds) > 0.0F) {
			return;
		}
		wait = 0.25F;
		const auto player = RE::PlayerCharacter::GetSingleton();
		for (auto& active : g_activeScenes) {
			if (active.ending || active.actors.empty()) {
				continue;
			}
			std::string why;
			bool        withPlayer = false;
			active.health.resize(active.actors.size(), -1.0F);
			for (std::size_t role = 0; role < active.actors.size(); ++role) {
				const auto id = active.actors[role];
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				if (!actor) {
					continue;
				}
				withPlayer = withPlayer || actor == player;
				// 4Stim's (bEndOnDamage): hurt. Actors in a scene don't start
				// fights, so OStim's combat check alone let a scene go on
				// while its actors were shot (the owner's Legendary Raider).
				// Health only goes down from harm; it coming back up is regeneration.
				const float health = g_endOnDamage && g_healthAV ? actor->GetActorValue(*g_healthAV) : -1.0F;
				const float before = active.health[role];
				active.health[role] = health;
				if (actor->IsDead(false)) {
					why = std::format("{:08X} died", id);
				} else if (actor->IsInCombat()) {
					why = std::format("{:08X} is in combat", id);
				} else if (health >= 0.0F && before >= 0.0F && health < before - 0.5F) {
					why = std::format("{:08X} was hurt ({:.0f} health lost)", id, before - health);
				}
				if (!why.empty()) {
					break;
				}
			}
			if (why.empty() && !withPlayer && player) {
				const auto first = RE::TESForm::GetFormByID<RE::Actor>(active.actors.front());
				// OStim (Thread::loop) ends an NPC scene when its first actor
				// and the player aren't in the same cell. Outdoors that's
				// wrong for Fallout 4: exterior cells are 4096 units, so a
				// player standing beside an NPC scene in a settlement is often
				// one cell over, and the owner's NPC scenes ended within
				// seconds (5 November). Indoors 4Stim compares cells as OStim
				// does; outdoors, same worldspace and the scene's cell still
				// attached (in the loaded grid around the player) and its
				// actor's 3D still there.
				const auto sceneCell = first ? first->parentCell : nullptr;
				const auto playerCell = player->parentCell;
				if (first && sceneCell != playerCell) {
					const bool outdoors = sceneCell && playerCell && sceneCell->IsExterior() && playerCell->IsExterior();
					if (!outdoors) {
						why = "the player isn't in its cell";
					} else if (sceneCell->worldSpace != playerCell->worldSpace) {
						why = "the player isn't in its worldspace";
					} else if (!sceneCell->IsAttached() || !first->Get3D()) {
						why = "its cell isn't loaded around the player any more";
					}
				}
			}
			if (!why.empty()) {
				active.ending = true;
				active.interrupted = true;
				EndActiveScene(active, why);
			}
		}
	}

	// Main thread. Starts the ticker thread if it isn't running.
	void StartAutoplayTicks()
	{
		if (g_autoplayTicking.exchange(true)) {
			return;
		}
		g_autoplayLast = std::chrono::steady_clock::now();
		// Every 10 ms while scenes run (the scheduler's thread, no thread of its own).
		Scheduler::Every(std::chrono::milliseconds(10), []() {
			if (g_autoplayTicking && !g_autoplayTickQueued.exchange(true)) {
				F4SE::GetTaskInterface()->AddTask(AutoplayTick);
			}
			return g_autoplayTicking.load();
		});
	}

	// Plays a_sequence from its first entry on the running scene a_actorID
	// is in. Main thread.
	bool StartSequenceOnScene(std::uint32_t a_actorID, const std::shared_ptr<const SceneRegistry::Sequence>& a_sequence)
	{
		const auto active = FindActiveScene(a_actorID);
		if (!active) {
			REX::WARN("Sequence \"{}\": {:08X} isn't in a scene", a_sequence->id, a_actorID);
			return false;
		}
		if (active->actors.size() != a_sequence->actorCount) {
			REX::WARN("Sequence \"{}\" is for {} actor(s), the scene has {}", a_sequence->id, a_sequence->actorCount, active->actors.size());
			return false;
		}
		const auto& first = a_sequence->entries.front();
		if (!PlayOnActiveScene(*active, first.scene, first.speed)) {
			return false;
		}
		active->sequence = a_sequence;
		active->step = 0;
		REX::INFO("Sequence \"{}\": started on the running scene ({} scene(s))", a_sequence->id, a_sequence->entries.size());
		HUD::OnFocusedSceneChanged();  // no navigation during it, as OStim's scene menu
		ArmAutoplay(*active);
		return true;
	}

	// Queues a_sequence for the next scene these actors start (its first
	// scene), from any thread.
	void QueueSequence(std::shared_ptr<const SceneRegistry::Sequence> a_sequence, std::vector<std::uint32_t> a_actors)
	{
		std::scoped_lock lock(g_pendingSequenceLock);
		g_pendingSequence.Put(a_actors, PendingSequence{ std::move(a_sequence), a_actors });
	}

	bool IsPlayer(const RE::Actor* a_actor)
	{
		return a_actor && a_actor == RE::PlayerCharacter::GetSingleton();
	}

	// Teleports a_actor to a_spot (on the main thread). Natively, not through
	// Papyrus SetPosition: that left the player where they stood.
	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot)
	{
		if (!a_actor) {
			return false;
		}
		const auto id = a_actor->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, a_spot]() {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				return;
			}
			actor->SetPosition(a_spot.position, true);
			FaceHeading(actor, a_spot.heading);
			g_placedSpots[id] = { a_spot.position, a_spot.heading, std::chrono::steady_clock::now() };
			const auto& now = actor->data.location;
			REX::INFO("Furniture: {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg; now at ({:.1f}, {:.1f}, {:.1f})",
				id, a_spot.position.x, a_spot.position.y, a_spot.position.z, a_spot.heading * 180.0F / PI_F, now.x, now.y, now.z);
		});
		return true;
	}

	std::int64_t NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void ReleaseFailedStart(std::vector<std::uint32_t> a_ids, std::string_view a_why)
	{
		std::erase(a_ids, 0u);
		if (a_ids.empty()) {
			return;
		}
		REX::WARN("Start failed ({}): letting {} actor(s) go again", a_why, a_ids.size());
		DropStartingThreadOf(a_ids, a_why);
		g_failedStartAt = NowMs();
		g_focusNextScene = false;
		Scheduler::AfterOnMain(std::chrono::milliseconds(1500), [ids = std::move(a_ids)]() {  // Papyrus's start returns first
			// Started after all (another try): leave it to the scene's own end.
			if (std::ranges::any_of(ids, [](std::uint32_t a_id) { return FindActiveScene(a_id) != nullptr; }) || !g_vm) {
				return;
			}
			std::vector<std::int32_t> args;
			for (const auto id : ids) {
				args.push_back(static_cast<std::int32_t>(id));
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "ReleaseFailedStart"sv, callback, args);
		});
	}

	void StartScene(std::monostate, RE::Actor* a_actor, RE::Actor*, std::string a_sceneID)
	{
		if (!a_actor) {
			REX::WARN("StartScene called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene) {
			REX::WARN("StartScene: no scene \"{}\" (check the Scenes folder and this log's load summary)", a_sceneID);
			ReleaseFailedStart({ a_actor->GetFormID() }, "no such scene");
			return;
		}
		const int  speed = StartSpeed(*scene);  // OStim: a new thread starts at the scene's defaultSpeed
		const auto idle = scene->speeds[speed][0];
		const auto sceneID = scene->id;

		REX::INFO("StartScene: {:08X} <- scene \"{}\" speed {} (idle {:08X})", a_actor->GetFormID(), scene->id, speed + 1, idle->GetFormID());
		PlaceOnPendingFurniture({ a_actor });

		// Papyrus calls arrive on a script thread; run engine animation code on the main thread.
		F4SE::GetTaskInterface()->AddTask([a_actor, idle, sceneID, speed]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StartScene: actor has no AI process");
				ReleaseFailedStart({ a_actor->GetFormID() }, "no AI process");
				return;
			}
			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StartScene: PlayIdle returned {}", ok);
			if (ok) {
				CuePlayed(a_actor->GetFormID(), idle, sceneID);
			}
			if (!ok) {
				ReleaseFailedStart({ a_actor->GetFormID() }, "the game refused the animation");
			}
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor);
			if (ok) {
				TrackSceneStart({ a_actor->GetFormID() }, sceneID, speed);
				if (focus) {
					PlayerScene focused{ a_actor->GetFormID(), 0, sceneID, speed };
					if (const auto active = FindActiveScene(a_actor->GetFormID())) {
						focused.furniture = active->furnitureType;
					}
					SetPlayerScene(std::move(focused));
				}
			}
		});
	}

	void StopScene(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("StopScene called with a null actor");
			return;
		}

		REX::INFO("StopScene: {:08X}", a_actor->GetFormID());

		// FO4.esm's IdleStop (000E9855), played the same way as a pose.
		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopScene: could not find IdleStop in Fallout4.esm");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor, idle]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StopScene: actor has no AI process");
				return;
			}

			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StopScene: PlayIdle(IdleStop) returned {}", ok);
			StopAgainIfJustPlayed({ a_actor->GetFormID() }, idle);
			ClearFocusIfIn(a_actor->GetFormID());
			TrackSceneStop(a_actor->GetFormID());
		});
	}

	// ---- Spike 4: lock / unlock an actor in place ----
	// Uses CallActorMethod to invoke the engine's own Papyrus-native
	// Actor.SetRestrained / Actor.EnableAI, confirmed from Actor.psc:
	//   bool Function SetRestrained(bool abRestrained = true) native
	//   Function EnableAI(bool abEnable = true, bool abPauseVoice = false) native

	// Blocks the Activate/Talk prompt and stops dialogue (including an
	// automatic proximity greet, which needs no button press and can fire
	// the instant an actor is moved near the player -- confirmed: a full
	// dialogue menu opened on its own within ~1s of MoveActorTo, well
	// before LockActor used to apply this same block ~3s later). Call
	// this BEFORE MoveActorTo brings the actor near the
	// player, not as part of locking them in place afterward.
	void SuppressInteraction(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("SuppressInteraction called with a null actor");
			return;
		}

		const auto blockOk = CallActorMethod(a_actor, "ObjectReference"sv, "BlockActivation"sv, true, true);
		const auto dialogueOk = CallActorMethod(a_actor, "Actor"sv, "AllowPCDialogue"sv, false);
		// Before anything moves them: where to put them back afterwards.
		{
			const auto  id = a_actor->GetFormID();
			const auto  pos = a_actor->data.location;
			const float heading = a_actor->data.angle.z;
			F4SE::GetTaskInterface()->AddTask([id, pos, heading]() {
				g_positionBefore[id] = { pos, heading, std::chrono::steady_clock::now() };
			});
		}
		REX::INFO("SuppressInteraction: {:08X} BlockActivation dispatch={}, AllowPCDialogue dispatch={}",
			a_actor->GetFormID(), blockOk, dialogueOk);
		// Out of any furniture or idle marker before they're placed (else
		// they keep its pose and props under the scene), and nothing for
		// their AI to do until the scene ends (ReleaseFromScene's
		// EvaluatePackage hands them back their routine).
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() {
			g_guardKicks.erase(id);  // a new scene: the guard starts over
			g_holsterKicks.erase(id);
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor || IsPlayer(actor)) {
				return;
			}
			if (LeaveFurniture(actor, true)) {
				REX::INFO("SuppressInteraction: {:08X} was in furniture or an idle marker, taken out", id);
			}
			// OStim's scene package (ScenePackage.h): their AI picks nothing
			// else until the scene ends, so a settler's job, a sandbox or a
			// work spot can't pull them out of it. EvaluatePackage, as
			// OStim's Thread::AddActor, for them to switch to it now.
			ScenePackage::Add(id);
			CallActorMethod(actor, "Actor"sv, "EvaluatePackage"sv, false);
		});
	}

	void RestoreInteraction(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("RestoreInteraction called with a null actor");
			return;
		}

		const auto blockOk = CallActorMethod(a_actor, "ObjectReference"sv, "BlockActivation"sv, false, false);
		const auto dialogueOk = CallActorMethod(a_actor, "Actor"sv, "AllowPCDialogue"sv, true);
		REX::INFO("RestoreInteraction: {:08X} BlockActivation dispatch={}, AllowPCDialogue dispatch={}",
			a_actor->GetFormID(), blockOk, dialogueOk);
	}

	void LockActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("LockActor called with a null actor");
			return;
		}

		REX::INFO("LockActor: {:08X}", a_actor->GetFormID());

		const auto restrainedOk = CallActorMethod(a_actor, "Actor"sv, "SetRestrained"sv, true);
		const auto aiOk = CallActorMethod(a_actor, "Actor"sv, "EnableAI"sv, false, false);
		REX::INFO("LockActor: SetRestrained dispatch={}, EnableAI dispatch={}", restrainedOk, aiOk);
	}

	void UnlockActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("UnlockActor called with a null actor");
			return;
		}

		REX::INFO("UnlockActor: {:08X}", a_actor->GetFormID());

		const auto restrainedOk = CallActorMethod(a_actor, "Actor"sv, "SetRestrained"sv, false);
		const auto aiOk = CallActorMethod(a_actor, "Actor"sv, "EnableAI"sv, true, false);
		REX::INFO("UnlockActor: SetRestrained dispatch={}, EnableAI dispatch={}", restrainedOk, aiOk);
	}

	// ---- Spike 4 continued: position one actor relative to another ----
	// Uses `data.angle.z` (radians) for the anchor's current heading -- the
	// one field confirmed directly from Actor.h's own inline GetHeading():
	// `return data.angle.z;`. Position itself is no longer read in native
	// code at all; MoveTo computes it from the anchor's own location.

	// Places a_target at a_distance units from a_anchor, offset by
	// a_angleOffsetDeg degrees from the anchor's own facing, then turns
	// a_target to face back toward the anchor.
	//
	// Goes through ObjectReference's own native MoveTo/SetAngle (dispatched
	// via CallActorMethod, same as LockActor/UnlockActor) rather than the
	// raw C++ SetPosition/SetHeading virtuals. Those only update partial
	// actor state and caused real desync bugs (mesh stretching, then a
	// facegen glitch after a QueueUpdate workaround); MoveTo/SetAngle are
	// the same native functions the engine itself uses for every teleport,
	// so they should keep position, 3D, and physics in sync properly.
	//
	// MoveTo's offsets are relative to the ANCHOR's own local facing
	// (X = right, Y = forward), so we don't need the anchor's world
	// heading to compute the offset -- only to compute the final facing
	// angle afterward.
	void MoveActorTo(std::monostate, RE::Actor* a_target, RE::Actor* a_anchor, float a_distance, float a_angleOffsetDeg)
	{
		if (!a_target || !a_anchor) {
			REX::WARN("MoveActorTo called with a null actor");
			return;
		}

		// NOTE: switched from MoveTo(ObjectReference, ...) to SetPosition
		// (plain floats) -- MoveTo's object argument silently no-op'd even
		// with timing ruled out as the cause, while SetAngle (plain floats,
		// no object arg) worked correctly. Computing world-space XYZ
		// ourselves sidesteps whatever that argument-packing issue was.
		// data.location is UNCONFIRMED (inferred alongside the confirmed
		// data.angle.z); if this fails to compile, that's the first thing
		// to check -- search TESObjectREFR.h for GetPositionX/Y/Z or the
		// real member name.
		const auto anchorPos = a_anchor->data.location;
		const float anchorHeadingRad = a_anchor->data.angle.z;
		const float anchorHeadingDeg = anchorHeadingRad * 180.0f / PI_F;

		const float angleRad = anchorHeadingRad + a_angleOffsetDeg * PI_F / 180.0f;
		const float targetX = anchorPos.x + a_distance * std::sin(angleRad);
		const float targetY = anchorPos.y + a_distance * std::cos(angleRad);
		const float targetZ = anchorPos.z;
		const float targetHeadingDeg = anchorHeadingDeg + a_angleOffsetDeg + 180.0f;  // face back toward anchor

		REX::INFO("MoveActorTo: {:08X} relative to {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
			a_target->GetFormID(), a_anchor->GetFormID(), targetX, targetY, targetZ, targetHeadingDeg);

		const auto moveOk = CallActorMethod(a_target, "ObjectReference"sv, "SetPosition"sv, targetX, targetY, targetZ);
		const auto angleOk = CallActorMethod(a_target, "ObjectReference"sv, "SetAngle"sv, 0.0f, 0.0f, targetHeadingDeg);
		REX::INFO("MoveActorTo: SetPosition dispatch={}, SetAngle dispatch={}", moveOk, angleOk);
	}

	// ---- Scene collision ----
	// Paired animations put both actors' references on one spot, so their
	// collision capsules overlap for the whole scene and shove each other
	// apart (confirmed: equal-and-opposite 40-70 unit displacement before
	// the idles even started). Bodies sharing a nonzero Havok system group
	// skip collision with EACH OTHER but still collide with the world, so
	// they keep standing on the ground. (The kNoCollision flag would also
	// drop ground collision, so it's deliberately not used.)

	// g_savedFilters (each actor's filter before the scene) is with the
	// collision watch above.

	RE::bhkCharacterController* GetCharController(RE::Actor* a_actor)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		const auto middleHigh = process ? process->middleHigh : nullptr;
		return middleHigh ? middleHigh->charController.get() : nullptr;
	}

	bool ApplyFilter(RE::Actor* a_actor, std::uint32_t a_filter)
	{
		const PhysicsLock physics(a_actor);
		const auto        controller = GetCharController(a_actor);
		if (!controller) {
			return false;
		}
		RE::CFilter filter{};
		filter.filter = a_filter;
		return controller->SetCollisionFilterInfo(filter);
	}

	void IgnorePairCollision(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("IgnorePairCollision called with a null actor");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor0, a_actor1]() {
			const PhysicsLock physics(a_actor0);  // both are in one cell for a scene
			const auto        c0 = GetCharController(a_actor0);
			const auto c1 = GetCharController(a_actor1);
			const auto b0 = c0 ? c0->GetBodyImpl() : nullptr;
			const auto b1 = c1 ? c1->GetBodyImpl() : nullptr;
			if (!b0 || !b1) {
				REX::WARN("IgnorePairCollision: missing character controller or body");
				return;
			}

			const auto f0 = b0->m_collisionFilterInfo;
			const auto f1 = b1->m_collisionFilterInfo;
			g_savedFilters.try_emplace(a_actor0->GetFormID(), f0);
			g_savedFilters.try_emplace(a_actor1->GetFormID(), f1);

			RE::CFilter cf0{}, cf1{};
			cf0.filter = f0;
			cf1.filter = f1;
			// Share actor 0's group if it has one; otherwise a fixed group
			// unlikely to clash with anything the game assigns.
			std::uint32_t group = cf0.GetSystemGroup();
			if (group == 0) {
				group = 0xF5A1;
			}
			cf0.SetSystemGroup(group);
			cf1.SetSystemGroup(group);

			const auto ok0 = c0->SetCollisionFilterInfo(cf0);
			const auto ok1 = c1->SetCollisionFilterInfo(cf1);
			REX::INFO("IgnorePairCollision: {:08X} filter {:08X} -> {:08X} ({}), {:08X} filter {:08X} -> {:08X} ({})",
				a_actor0->GetFormID(), f0, cf0.filter, ok0, a_actor1->GetFormID(), f1, cf1.filter, ok1);
		});
	}

	void RestoreCollisionNow(RE::Actor* a_actor);

	void RestoreCollision(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("RestoreCollision called with a null actor");
			return;
		}

		// After the actors have stepped apart (MoveApart): collision back
		// while they're still inside each other leaves them stuck together.
		Scheduler::AfterOnMain(std::chrono::milliseconds(700), [a_actor]() { RestoreCollisionNow(a_actor); });
	}

	void RestoreCollisionNow(RE::Actor* a_actor)
	{
		RestoreTouched(a_actor->GetFormID());
		RestoreGravity(a_actor);
		{
			const auto it = g_savedFilters.find(a_actor->GetFormID());
			if (it == g_savedFilters.end()) {
				REX::INFO("RestoreCollision: {:08X} has no saved filter", a_actor->GetFormID());
				return;
			}
			const auto ok = ApplyFilter(a_actor, it->second);
			REX::INFO("RestoreCollision: {:08X} filter -> {:08X} ({})", a_actor->GetFormID(), it->second, ok);
			g_savedFilters.erase(it);
		}
	}

	// ---- Two-actor scenes ----

	// Puts both actors on the same spot, a_distance units in front of
	// a_anchor, with the same heading: a_anchor's heading plus
	// a_headingOffsetDeg (180 = facing the anchor, 0 = facing where the
	// anchor faces; use distance 0 / offset 0 to build the scene on the
	// anchor's own spot, e.g. when the anchor is a participant). Paired animations
	// are authored around one shared origin, with the offset between the
	// actors baked into the animation itself, so both must start from the
	// same point and facing.
	void PlacePair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, RE::Actor* a_anchor, float a_distance, float a_headingOffsetDeg)
	{
		if (!a_actor0 || !a_actor1 || !a_anchor) {
			REX::WARN("PlacePair called with a null actor");
			return;
		}
		if (PlaceOnPendingFurniture({ a_actor0, a_actor1 })) {
			return;
		}

		const auto  anchorPos = a_anchor->data.location;
		const float anchorHeadingRad = a_anchor->data.angle.z;
		const float x = anchorPos.x + a_distance * std::sin(anchorHeadingRad);
		const float y = anchorPos.y + a_distance * std::cos(anchorHeadingRad);
		const float z = anchorPos.z;
		float headingDeg = std::fmod(anchorHeadingRad * 180.0f / PI_F + a_headingOffsetDeg, 360.0f);
		if (headingDeg < 0.0f) {
			headingDeg += 360.0f;
		}

		REX::INFO("PlacePair: {:08X} + {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
			a_actor0->GetFormID(), a_actor1->GetFormID(), x, y, z, headingDeg);

		// Natively, like furniture placement: Papyrus SetPosition on the
		// player is a teleport the game may fade the screen for, and with
		// the script engine busy that fade can take very long to clear.
		const Furniture::Spot spot{ RE::NiPoint3{ x, y, z }, headingDeg * PI_F / 180.0F };
		for (auto* actor : { a_actor0, a_actor1 }) {
			PlaceActorAt(actor, spot);
		}
	}

	// Starts roles 0 and 1 of a registered two-actor scene in the same task,
	// i.e. on the same game frame, so the two actors begin in step.
	void PlaySceneIdles(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, std::string a_sceneID)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("PlaySceneIdles called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() < 2) {
			REX::WARN("PlaySceneIdles: no two-actor scene \"{}\"", a_sceneID);
			ReleaseFailedStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, "no such scene");
			return;
		}
		const int  speed = StartSpeed(*scene);  // OStim: a new thread starts at the scene's defaultSpeed
		const auto idle0 = scene->speeds[speed][0];
		const auto idle1 = scene->speeds[speed][1];
		const auto sceneID = scene->id;

		REX::INFO("PlaySceneIdles: scene \"{}\": {:08X} <- {:08X}, {:08X} <- {:08X}",
			scene->id, a_actor0->GetFormID(), idle0->GetFormID(), a_actor1->GetFormID(), idle1->GetFormID());

		F4SE::GetTaskInterface()->AddTask([a_actor0, idle0, a_actor1, idle1, sceneID, speed]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			if (!p0 || !p1) {
				REX::WARN("PlaySceneIdles: an actor has no AI process");
				ReleaseFailedStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, "no AI process");
				return;
			}
			for (auto* actor : { a_actor0, a_actor1 }) {
				const auto& pos = actor->data.location;
				REX::INFO("PlaySceneIdles: {:08X} actually at ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
					actor->GetFormID(), pos.x, pos.y, pos.z, actor->data.angle.z * 180.0f / PI_F);
			}
			const auto ok0 = p0->PlayIdle(*a_actor0, idle0, nullptr);
			const auto ok1 = p1->PlayIdle(*a_actor1, idle1, nullptr);
			REX::INFO("PlaySceneIdles: PlayIdle returned {} / {}", ok0, ok1);
			if (ok0) {
				CuePlayed(a_actor0->GetFormID(), idle0, sceneID);
			}
			if (ok1) {
				CuePlayed(a_actor1->GetFormID(), idle1, sceneID);
			}
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor0) || IsPlayer(a_actor1);
			if (!ok0 && !ok1) {
				ReleaseFailedStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, "the game refused both animations");
				return;
			}
			TrackSceneStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, sceneID, speed);
			if (focus) {
				PlayerScene focused{ a_actor0->GetFormID(), a_actor1->GetFormID(), sceneID, speed };
				if (const auto active = FindActiveScene(a_actor0->GetFormID())) {
					focused.furniture = active->furnitureType;
				}
				SetPlayerScene(std::move(focused));
			}
		});
	}

	// Queues sequence a_sequenceID for the next scene started with these
	// actors (akActor1 None for a solo sequence) and returns the scene to
	// start, its first one; "" if there's no such sequence or the actor
	// count doesn't match. FourStimScene.BeginPairSequence wraps this.
	std::string QueueSequenceNative(std::monostate, std::string a_sequenceID, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequenceID);
		if (!sequence || !a_actor0) {
			REX::WARN("QueueSequence: no sequence \"{}\" (or no actor)", a_sequenceID);
			return {};
		}
		std::vector<std::uint32_t> actors{ a_actor0->GetFormID() };
		if (a_actor1) {
			actors.push_back(a_actor1->GetFormID());
		}
		if (actors.size() != sequence->actorCount) {
			REX::WARN("QueueSequence: \"{}\" is for {} actor(s), {} given", sequence->id, sequence->actorCount, actors.size());
			return {};
		}
		QueueSequence(sequence, std::move(actors));
		return sequence->entries.front().scene;
	}

	// Plays sequence a_sequenceID on the scene a_actor is already in.
	// Returns false if there's no such sequence; the rest is checked on the
	// next frame (see the log).
	bool StartSequenceOnSceneNative(std::monostate, RE::Actor* a_actor, std::string a_sequenceID)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequenceID);
		if (!sequence || !a_actor) {
			REX::WARN("StartSequenceOnScene: no sequence \"{}\" (or no actor)", a_sequenceID);
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID(), sequence]() { StartSequenceOnScene(id, sequence); });
		return true;
	}

	// Stops the sequence the scene a_actor is in is playing (the scene stays
	// where it is).
	void StopSequenceNative(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() {
			const auto active = FindActiveScene(id);
			if (active && active->sequence) {
				REX::INFO("Sequence \"{}\": stopped", active->sequence->id);
				active->sequence.reset();
				ArmAutoplay(*active);  // a transition still finishes
			}
		});
	}

	// Number of actors (roles) in a registered scene, or 0 if it doesn't exist.
	std::int32_t GetSceneActorCount(std::monostate, std::string a_sceneID)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene ? static_cast<std::int32_t>(scene->actors.size()) : 0;
	}
}
