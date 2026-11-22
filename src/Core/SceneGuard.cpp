#include "Core/Core.h"

namespace Core
{
	// The shake watch (bCollisionWatch): every frame, where each scene actor
	// is (the reference) and where they're drawn (their 3D root), against
	// their spot. Every 2 s (10 times a scene), a line with how far off they
	// are and how much they move frame to frame, and how often that movement
	// turns back on itself: a shake is a back-and-forth every frame (the
	// owner's wall scenes, 6 November, about 2 pixels each frame with the
	// camera still). A reference that turns back is being moved by
	// something fighting the lock (the animation's root motion, collision);
	// only the drawn root turning back is the animation itself.
	void WatchShaking(float a_seconds)
	{
		struct Track
		{
			RE::NiPoint3  lastLoc, lastRoot, lastLocStep, lastRootStep;
			bool          started = false;
			int           frames = 0, locTurns = 0, rootTurns = 0, lines = 0;
			float         locStepMax = 0.0F, rootStepMax = 0.0F, locOffMax = 0.0F, rootOffMax = 0.0F, time = 0.0F;
			// Turning: the reference's heading (data.angle.z) and the drawn
			// root's whole rotation, per frame. Added for the player's faint
			// vibration on 1.10.163 (20 November 2026), which showed no
			// movement at all in the steps above.
			float         lastHeading = 0.0F, lastHeadStep = 0.0F, headStepMax = 0.0F, rootTurnMax = 0.0F, headOffMax = 0.0F;
			RE::NiMatrix3 lastRot{};
			int           headTurns = 0;
			std::string   scene;
		};
		static std::unordered_map<std::uint32_t, Track> tracks;
		if (!g_settings.collisionWatch) {
			tracks.clear();
			return;
		}
		auto turned = [](const RE::NiPoint3& a, const RE::NiPoint3& b) {
			// Two frame steps pointing opposite ways, both over a hundredth of a unit.
			const float la = a.Length(), lb = b.Length();
			return la > 0.01F && lb > 0.01F && (a.x * b.x + a.y * b.y + a.z * b.z) < -0.5F * la * lb;
		};
		std::unordered_set<std::uint32_t> seen;
		for (const auto& active : g_activeScenes) {
			for (std::size_t role = 0; role < active.actors.size(); ++role) {
				const auto id = active.actors[role];
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				const auto root = actor ? actor->Get3D() : nullptr;
				if (!root) {
					continue;
				}
				seen.insert(id);
				auto& t = tracks[id];
				if (t.scene != active.sceneID) {
					t = Track{};
					t.scene = active.sceneID;
				}
				const auto spotFull = SpotOf(active, role);
				const auto spot = spotFull.position;
				const float heading = actor->data.angle.z;
				const auto& rot = root->world.rotate;
				auto wrap = [](float a) {
					while (a > PI_F) a -= 2.0F * PI_F;
					while (a < -PI_F) a += 2.0F * PI_F;
					return a;
				};
				const auto loc = actor->data.location;
				const auto drawn = root->world.translate;
				if (t.started) {
					const auto locStep = loc - t.lastLoc;
					const auto rootStep = drawn - t.lastRoot;
					t.locTurns += turned(locStep, t.lastLocStep) ? 1 : 0;
					t.rootTurns += turned(rootStep, t.lastRootStep) ? 1 : 0;
					t.locStepMax = std::max(t.locStepMax, locStep.Length());
					t.rootStepMax = std::max(t.rootStepMax, rootStep.Length());
					t.lastLocStep = locStep;
					t.lastRootStep = rootStep;
					const float headStep = wrap(heading - t.lastHeading);
					t.headTurns += (headStep * t.lastHeadStep < 0.0F && std::fabs(headStep) > 1e-5F) ? 1 : 0;
					t.headStepMax = std::max(t.headStepMax, std::fabs(headStep));
					t.lastHeadStep = headStep;
					// The angle between this frame's rotation and the last: trace(Rlast^T R).
					float trace = 0.0F;
					for (int r = 0; r < 3; ++r) {
						for (int c = 0; c < 3; ++c) {
							trace += t.lastRot.entry[r][c] * rot.entry[r][c];
						}
					}
					t.rootTurnMax = std::max(t.rootTurnMax, std::acos(std::clamp((trace - 1.0F) * 0.5F, -1.0F, 1.0F)));
				}
				t.lastHeading = heading;
				t.lastRot = rot;
				t.headOffMax = std::max(t.headOffMax, std::fabs(wrap(heading - spotFull.heading)));
				t.started = true;
				t.lastLoc = loc;
				t.lastRoot = drawn;
				t.locOffMax = std::max(t.locOffMax, loc.GetDistance(spot));
				t.rootOffMax = std::max(t.rootOffMax, drawn.GetDistance(spot));
				++t.frames;
				t.time += a_seconds;
				if (t.time >= 2.0F) {
					if (t.lines < 10) {
						++t.lines;
						REX::INFO("Shake watch: {:08X} in \"{}\", {} frames: reference up to {:.2f} off its spot, steps up to {:.2f}, {} turn(s) back; drawn root up to {:.2f} off, steps up to {:.2f}, {} turn(s) back; heading up to {:.3f} deg off, steps up to {:.4f} deg, {} reversal(s); drawn root turns up to {:.4f} deg a frame",
							id, active.sceneID, t.frames, t.locOffMax, t.locStepMax, t.locTurns, t.rootOffMax, t.rootStepMax, t.rootTurns, t.headOffMax * 180.0F / PI_F,
							t.headStepMax * 180.0F / PI_F, t.headTurns, t.rootTurnMax * 180.0F / PI_F);
					}
					const auto scene = t.scene;
					const int  lines = t.lines;
					t = Track{};
					t.scene = scene;
					t.lines = lines;
				}
			}
		}
		std::erase_if(tracks, [&](const auto& a_entry) { return !seen.contains(a_entry.first); });
	}

	void GuardScenes(float a_seconds)
	{
		static float clock = 0.0F;
		if (!g_guardScenes || (clock += a_seconds) < 0.25F) {
			return;
		}
		clock = 0.0F;
		const auto now = std::chrono::steady_clock::now();
		for (auto& active : g_activeScenes) {
			const auto scene = SceneRegistry::Find(active.sceneID);
			if (!scene) {
				continue;
			}
			const bool settled = active.remaining < 0.0F && !scene->IsTransition() && now - active.lastPlayed > std::chrono::milliseconds(1500);
			const RE::TESIdleForm* intruder = nullptr;
			std::uint32_t          intruded = 0;
			bool                   forceReplay = false;
			// For the "an NPC didn't follow the scene change" reports: a second
			// after each play, what each NPC is doing, once (a WARN when it
			// isn't on one of the scene's idles or is talking: a companion's
			// comment can hold its graph in a dialogue animation).
			const bool checkNow = !active.checkedAfterPlay && now - active.lastPlayed > std::chrono::seconds(1);
			if (checkNow) {
				active.checkedAfterPlay = true;
			}
			for (const auto id : active.actors) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				Undress::Enforce(actor);  // what came off stays off (the player too)
				if (actor) {
					LogVisibility(actor, active.sceneID);
				}
				if (IsPlayer(actor)) {
					ShowPlayerBody(actor, active.sceneID);
					// As OStim's lock: the player's weapon put away. Not in
					// combat only, as for NPCs below.
					if (auto& kicks = g_holsterKicks[id]; kicks.count < 3 && actor->GetWeaponMagicDrawn() && !actor->IsInCombat() && now - kicks.last > std::chrono::seconds(3)) {
						kicks.last = now;
						++kicks.count;
						if (SheatheNow(actor, "scene guard")) {
							forceReplay = true;
						}
					}
				}
				if (actor) {
					HoldGravity(actor);  // see "No gravity in scenes"
				}
				// Without Getbhk since 0.5.0 (it crashed the owner's game from
				// here, 1 November): see WatchCollisions. Only in the first
				// seconds after each scene plays (from 1.5 to 8 s), not all
				// scene long: the guard runs alongside the game's own actor
				// and physics jobs, and two crashes on 7 November were Havok's
				// thread check tripping in another NPC's movement
				// (forceBodyPairCollisions) well into a scene. What touches
				// the actors does so once they're placed.
				if (actor && settled && now - active.lastPlayed < std::chrono::seconds(8)) {
					WatchCollisions(actor, active.sceneID);
				}
				if (!actor || IsPlayer(actor)) {
					continue;
				}
				const auto process = actor->currentProcess;
				const auto data = process ? process->middleHigh : nullptr;
				if (!data) {
					continue;
				}
				data->packageIdleTimer = 30.0F;  // no package idle for a while; renewed every check
				if (checkNow) {
					const auto idle = data->currentIdle;
					const bool ours = idle && std::ranges::any_of(scene->speeds, [&](const auto& a_idles) { return std::ranges::find(a_idles, idle) != a_idles.end(); });
					const bool talking = actor->IsTalking();
					const auto what = std::format("idle {:08X}{}, {}talking, weapon {}", idle ? idle->GetFormID() : 0, ours ? " (the scene's)" : idle ? " (NOT the scene's)" : " (none)",
						talking ? "" : "not ", actor->GetWeaponMagicDrawn() ? "out" : "away");
					// No current idle is normal: the process doesn't keep one for
					// idles played this way (every check in the owner's log).
					// Talking alone isn't a problem: 4Stim's own moans count as
					// talking (every scene change in the owner's log, 10
					// November, with no chatter heard), so it's only noted.
					if (idle && !ours) {
						REX::WARN("Scene check: {:08X} in \"{}\" a second after it played: {}", id, active.sceneID, what);
					} else {
						REX::INFO("Scene check: {:08X} in \"{}\": {}", id, active.sceneID, what);
					}
				}
				// Scene lock watchdog: moved off the spot (or turned) anyway?
				if (g_lockScenes && now - active.lastRelock > std::chrono::seconds(1)) {
					const auto  role = static_cast<std::size_t>(std::ranges::find(active.actors, id) - active.actors.begin());
					const auto  spot = SpotOf(active, role);
					const float drift = actor->data.location.GetDistance(spot.position);
					float       turn = std::fmod(std::fabs(actor->data.angle.z - spot.heading), 2.0F * PI_F);
					turn = std::fmin(turn, 2.0F * PI_F - turn);
					turn = std::fmin(turn, 2.0F * PI_F - turn);
					if (drift > 4.0F || turn > 0.06F) {
						REX::INFO("Scene lock: {:08X} in \"{}\" was {:.1f} units / {:.1f} deg off its spot, locking it again", id, active.sceneID, drift,
							turn * 180.0F / PI_F);
						active.lastRelock = now;
						LockScene(active);
					}
				}
				// A weapon out with no fight going on (an NPC drawing on an
				// alarm or a threat that's gone): put away and the scene played
				// again. In real combat (an attack on the scene), it's left to
				// the game: a scene can be broken by an attack.
				// At most 3 tries a scene, 3 s apart: an actor whose weapon stays
				// "drawn" whatever is done (a companion's graph mid-scene, in the
				// owner's log) was put away and the scene replayed every second,
				// restarting its animation each time.
				if (auto& kicks = g_holsterKicks[id]; kicks.count < 3 && actor->GetWeaponMagicDrawn() && !actor->IsInCombat()) {
					if (now - kicks.last > std::chrono::seconds(3)) {
						kicks.last = now;
						if (++kicks.count == 3) {
							REX::WARN("Scene guard: {:08X} still has a weapon out after 3 tries; leaving it be for this scene", id);
						}
						REX::INFO("Scene guard: {:08X} in \"{}\" drew a weapon (not in combat): putting it away", id, active.sceneID);
						SheatheNow(actor, "scene guard");
						if (g_vm) {
							RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
							Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "PutWeaponAwayByID"sv, callback, static_cast<std::int32_t>(id));
						}
						forceReplay = true;
					}
					continue;
				}
				// The same furniture again after StopInteractingQuick took them
				// out of it: the process keeps a stale handle (the owner's Ana
				// and a garden idle marker, 4 November: "taken out" three times,
				// she never went back, and each replay restarted the scene).
				// Not replayed for it; the lock holds them on their spot.
				const auto inFurniture = FurnitureOf(actor);
				if (auto& kicks = g_guardKicks[id]; inFurniture && Compat::CanStopInteractingQuick() && inFurniture->GetFormID() == kicks.takenOutOf) {
					if (!kicks.staleLogged) {
						kicks.staleLogged = true;
						REX::INFO("Scene guard: {:08X} still shows {:08X}, which they were taken out of: a stale handle, left be", id, kicks.takenOutOf);
					}
				} else if (inFurniture && kicks.count < 3 && now - kicks.last > std::chrono::seconds(5)) {
					kicks.last = now;
					if (++kicks.count == 3) {
						REX::WARN("Scene guard: {:08X} keeps going back into furniture; leaving it be for this scene", id);
					}
					REX::INFO("Scene guard: {:08X} in \"{}\" went into furniture or an idle marker", id, active.sceneID);
					LeaveFurniture(actor, false);
					forceReplay = true;
					continue;
				}
				const auto current = data->currentIdle;
				if (!settled || !current || intruder) {
					continue;
				}
				const bool ours = std::ranges::any_of(scene->speeds, [&](const auto& a_idles) {
					return std::ranges::find(a_idles, current) != a_idles.end();
				});
				if (!ours) {
					intruder = current;
					intruded = id;
				}
			}
			if (forceReplay) {
				PlayOnActiveScene(active, active.sceneID, active.speed);
			} else if (intruder) {
				REX::INFO("Scene guard: {:08X} in \"{}\" was playing a game idle ({:08X} \"{}\"), playing the scene again", intruded, active.sceneID,
					intruder->GetFormID(), intruder->GetFormEditorID() ? intruder->GetFormEditorID() : "");
				PlayOnActiveScene(active, active.sceneID, active.speed);
			}
		}
	}

	// ---- Holding the player (bHoldPlayer) ----
	//
	// The lock holds scene actors on their spots with a translation; NPCs'
	// controllers collide with nothing (bNoActorCollision) and have no
	// gravity, so nothing pulls against it. The player's controller still
	// collided with the ground, and wherever her spot wasn't exactly on it
	// (a sleeping bag's spot 3 units above the bag, 20 November 2026; any
	// height change in the Align tab, a tester, 22 November) the game kept
	// settling her onto the ground while the translation held her up: her
	// position and root stayed put (the shake watch: 0.00) but her body
	// vibrated faintly. With no collision she instead sank slowly through
	// the ground, as nothing put her back (the lock's watchdog leaves the
	// player alone).
	//
	// So with bHoldPlayer the player collides with nothing too
	// (HoldNoCollision), and every frame her controller is put at her spot
	// (plus its own height over her feet, measured once a scene) with no
	// velocity and no fall time: the spot itself, at any height, with nothing
	// to settle onto. NPCs keep the translation and the watchdog.
	void HoldPlayer(float a_seconds)
	{
		struct Hold
		{
			std::string sceneID;
			std::size_t role = 0;
			float       capsuleZ = 0.0F;  // the controller's position over the feet (its capsule's middle)
			float       time = 0.0F, maxFix = 0.0F;
			int         fixes = 0, frames = 0;
		};
		static std::optional<Hold> s_hold;
		constexpr float            HAVOK_TO_GAME = 69.99125F;  // Fallout 4's Havok scale (1 / 0.0142875)

		const auto player = RE::PlayerCharacter::GetSingleton();
		const ActiveScene* active = nullptr;
		std::size_t        role = 0;
		if (g_lockScenes && g_settings.holdPlayer && g_settings.noActorCollision && player) {
			for (const auto& candidate : g_activeScenes) {
				for (std::size_t r = 0; r < candidate.actors.size(); ++r) {
					if (candidate.actors[r] == player->GetFormID()) {
						active = &candidate;
						role = r;
					}
				}
			}
		}
		const auto controller = active ? GetCharController(player) : nullptr;
		// Only once the lock holds her (gravity 0, HoldGravity), and not while
		// a transition walks the actors between spots.
		const auto scene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
		if (!controller || controller->gravity != 0.0F || !scene || scene->IsTransition()) {
			if (s_hold && !active) {
				REX::INFO("Player hold: done ({} correction(s) in the last stretch)", s_hold->fixes);
				s_hold.reset();
			}
			return;
		}
		const PhysicsLock physics(player);
		RE::hkVector4f    at{};
		controller->GetPositionImpl(at, false);
		const RE::NiPoint3 controllerAt{ at[0] * HAVOK_TO_GAME, at[1] * HAVOK_TO_GAME, at[2] * HAVOK_TO_GAME };
		if (!s_hold || s_hold->sceneID != active->sceneID || s_hold->role != role) {
			const float capsuleZ = s_hold ? s_hold->capsuleZ : controllerAt.z - player->data.location.z;
			s_hold = Hold{ active->sceneID, role, capsuleZ };
			REX::INFO("Player hold: \"{}\" role {}: held on her spot through her controller (capsule {:.1f} over her feet)", active->sceneID, role, capsuleZ);
		}
		auto&       hold = *s_hold;
		const auto  spot = SpotOf(*active, role).position;
		const RE::NiPoint3 wanted{ spot.x, spot.y, spot.z + hold.capsuleZ };
		const float off = wanted.GetDistance(controllerAt);
		if (off > 0.05F) {
			controller->SetPositionImpl(RE::hkVector4f{ wanted.x / HAVOK_TO_GAME, wanted.y / HAVOK_TO_GAME, wanted.z / HAVOK_TO_GAME, 0.0F }, false, false);
			++hold.fixes;
			hold.maxFix = std::max(hold.maxFix, off);
		}
		controller->SetLinearVelocityImpl(RE::hkVector4f{ 0.0F, 0.0F, 0.0F, 0.0F });
		controller->fallTime = 0.0F;
		++hold.frames;
		hold.time += a_seconds;
		if (hold.time >= 5.0F) {
			REX::INFO("Player hold: \"{}\": {} of {} frame(s) put back on the spot (up to {:.2f} off)", hold.sceneID, hold.fixes, hold.frames, hold.maxFix);
			hold.time = 0.0F;
			hold.fixes = 0;
			hold.frames = 0;
			hold.maxFix = 0.0F;
		}
	}
}
