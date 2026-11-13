#include "Core/Core.h"

namespace Core
{
	// ---- Speed and navigation (the player's current scene) ----
	// Both replay idles on the actors already in place: same spot, same
	// collision setup, both roles started on the same frame. Navigation keeps
	// the roles (destination scenes always have the same number of actors)
	// and keeps the current speed where the destination has that many.

	namespace
	{
		struct FrameTiming
		{
			std::string                           line;
			std::chrono::steady_clock::time_point last;
			std::vector<double>                   frames;
		};

		// One frame later (F4SE's tasks run once a frame): the gap since the
		// last, three times, then the log line.
		void TimeNextFrame(std::shared_ptr<FrameTiming> a_timing)
		{
			F4SE::GetTaskInterface()->AddTask([a_timing]() {
				const auto now = std::chrono::steady_clock::now();
				a_timing->frames.push_back(std::chrono::duration<double, std::milli>(now - a_timing->last).count());
				a_timing->last = now;
				if (a_timing->frames.size() < 3) {
					TimeNextFrame(a_timing);
					return;
				}
				REX::INFO("{}; next frames {:.1f} / {:.1f} / {:.1f} ms", a_timing->line, a_timing->frames[0], a_timing->frames[1], a_timing->frames[2]);
			});
		}
	}

	// Plays speed a_speed of a_scene on the player's scene actors, on the main
	// thread, and records the new state.
	void PlayPlayerScene(const SceneRegistry::Scene& a_scene, int a_speed, const PlayerScene& a_current)
	{
		const auto  idles = a_scene.speeds[a_speed];
		const int   count = static_cast<int>(a_scene.speeds.size());
		PlayerScene next = a_current;
		next.sceneID = a_scene.id;
		next.speed = a_speed;
		// Timing (the short freeze when picking a transition, docs/TODO.md):
		// from the pick to the task, each PlayIdle, and the next frames after.
		const auto picked = std::chrono::steady_clock::now();
		const bool transition = a_scene.IsTransition();
		F4SE::GetTaskInterface()->AddTask([idles, next, count, previous = a_current, picked, transition]() {
			using ms = std::chrono::duration<double, std::milli>;
			const auto started = std::chrono::steady_clock::now();
			std::string playTimes;
			// The scene may have ended or moved on since this was queued (an
			// End right after a speed key, say): then don't bring it back.
			const auto now = GetPlayerScene();
			if (now.ActorIDs() != previous.ActorIDs() || now.sceneID != previous.sceneID || now.speed != previous.speed) {
				REX::INFO("Player scene: change to \"{}\" speed {} dropped, the scene changed first", next.sceneID, next.speed + 1);
				return;
			}
			const auto  ids = next.ActorIDs();
			std::string results;
			for (std::size_t role = 0; role < idles.size() && role < ids.size(); ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				const auto before = std::chrono::steady_clock::now();
				const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
				playTimes += std::format("{}{:.1f}", playTimes.empty() ? "" : " / ", ms(std::chrono::steady_clock::now() - before).count());
				if (ok) {
					CuePlayed(ids[role], idles[role], next.sceneID);
				}
				results += std::format("{}{:08X} {}", results.empty() ? "" : ", ", ids[role], ok ? "ok" : process ? "refused" : "no AI process");
				if (!ok && process) {
					// Refused (a tester on 1.10.163: the player stood still while
					// their partner moved on): once more on the next frame.
					F4SE::GetTaskInterface()->AddTask([id = ids[role], idle = idles[role], sceneID = next.sceneID]() {
						const auto again = RE::TESForm::GetFormByID<RE::Actor>(id);
						const auto process = again ? again->currentProcess : nullptr;
						const bool ok = process && GetPlayerScene().sceneID == sceneID && process->PlayIdle(*again, idle, nullptr);
						if (ok) {
							CuePlayed(id, idle, sceneID);
						}
						REX::INFO("Player scene: {:08X} PlayIdle again: {}", id, ok ? "ok" : "refused again");
					});
				}
			}
			SetPlayerScene(next);
			REX::INFO("Player scene: \"{}\" speed {}: PlayIdle {}", next.sceneID, next.speed + 1, results);
			TrackSceneChange(next.role0, next.sceneID, next.speed);
			if (next.sceneID != previous.sceneID) {
				SceneEvents::SceneChanged(next.ActorIDs(), previous.sceneID, next.sceneID, ThreadIDOf(next.role0), next.speed);
			} else if (next.speed != previous.speed) {
				SceneEvents::SpeedChanged(next.ActorIDs(), next.sceneID, next.speed, count, ThreadIDOf(next.role0));
			}
			const auto done = std::chrono::steady_clock::now();
			// The next three frames: a freeze while the game loads the new
			// animations shows as one long gap here, after PlayIdle returned.
			auto timing = std::make_shared<FrameTiming>();
			timing->line = std::format("Player scene timing: \"{}\"{}: {:.1f} ms from the pick to the task, PlayIdle {} ms, task {:.1f} ms",
				next.sceneID, transition ? " (transition)" : "", ms(started - picked).count(), playTimes, ms(done - started).count());
			timing->last = done;
			TimeNextFrame(timing);
		});
	}

	// Changes the player's scene speed by a_delta. Returns the new speed
	// (1-based), or 0 if the player isn't in a scene.
	// OStim's NoPlayerControl (a thread builder's): the player's own speed,
	// navigation and auto mode keys leave this scene alone.
	bool NoPlayerControl(std::uint32_t a_actorID)
	{
		const auto active = a_actorID ? FindActiveScene(a_actorID) : nullptr;
		return active && active->noPlayerControl;
	}

	std::int32_t ChangeSceneSpeedImpl(std::int32_t a_delta)
	{
		const auto current = GetPlayerScene();
		const auto scene = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		if (!scene || NoPlayerControl(current.role0)) {
			return 0;
		}
		const int count = static_cast<int>(scene->speeds.size());
		const int speed = std::clamp(current.speed + a_delta, 0, count - 1);
		if (speed != current.speed) {
			PlayPlayerScene(*scene, speed, current);
		}
		return speed + 1;
	}

	std::int32_t ChangeSceneSpeed(std::monostate, std::int32_t a_delta)
	{
		return ChangeSceneSpeedImpl(a_delta);
	}

	// Moves the player's scene to a_destination (normally one of the current
	// scene's navigation links). Returns false if that isn't possible.
	// a_search: picked in the search picker, which goes anywhere, as OStim's
	// search menu (warpTo), sequence or not.
	bool NavigateSceneImpl(const std::string& a_destination, bool a_search)
	{
		const auto current = GetPlayerScene();
		const auto from = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		const auto to = SceneRegistry::Find(a_destination);
		if (NoPlayerControl(current.role0)) {
			REX::INFO("NavigateScene: this scene was started with NoPlayerControl");
			return false;
		}
		if (!a_search && FourStim::FocusedInSequence()) {
			REX::INFO("NavigateScene: not during a sequence (as OStim's scene menu)");
			return false;
		}
		if (!from || !to || to->actors.size() != from->actors.size()) {
			REX::WARN("NavigateScene: can't go to \"{}\"", a_destination);
			return false;
		}
		if (!SexesFit(*to, current.ActorIDs())) {
			REX::WARN("NavigateScene: \"{}\" is for other sexes than this scene's actors (bMatchSex)", a_destination);
			return false;
		}
		if (!SceneFitsFurniture(*to, current.furniture)) {
			REX::WARN("NavigateScene: \"{}\" is for {} furniture, this scene is on {}", a_destination,
				to->furniture.empty() ? "no" : to->furniture, current.furniture.empty() ? "none" : current.furniture);
			return false;
		}
		if (from->IsTransition()) {
			// Let the transition finish, then go there instead of its
			// destination (no jump out of the middle of it).
			F4SE::GetTaskInterface()->AddTask([actorID = current.role0, sceneID = to->id]() {
				if (const auto active = FindActiveScene(actorID)) {
					active->queuedScene = sceneID;
					REX::INFO("NavigateScene: \"{}\" queued until the transition ends", sceneID);
				}
			});
			return true;
		}
		const auto active = FindActiveScene(current.role0);
		const int  speed = active ? MappedSpeed(*active, *to) : std::min(current.speed, static_cast<int>(to->speeds.size()) - 1);
		// As OStim's Navigate (warpTo with the node's fadeOnEntry): into a
		// scene marked fadeOnEntry, the screen fades to black, the scene
		// changes 0.7 s in, and it fades back 0.55 s later.
		const bool withPlayer = std::ranges::any_of(current.ActorIDs(), [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		if (to->fadeOnEntry && withPlayer && g_vm && !g_sceneFade) {
			REX::INFO("NavigateScene: fading to \"{}\" (fadeOnEntry)", to->id);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeToBlack"sv, callback);
			Scheduler::AfterOnMain(std::chrono::milliseconds(700), [to, speed, role0 = current.role0]() {
				const auto now = GetPlayerScene();
				if (now.role0 == role0 && FindActiveScene(role0)) {
					PlayPlayerScene(*to, speed, now);
				}
			});
			Scheduler::After(std::chrono::milliseconds(700 + 550), []() {
				if (g_vm) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> after;
					Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeFromBlack"sv, after, 0.0F);
				}
			});
			return true;
		}
		PlayPlayerScene(*to, speed, current);
		return true;
	}

	bool NavigateScene(std::monostate, std::string a_destination)
	{
		return NavigateSceneImpl(a_destination);
	}

	// Fills {n} in a navigation label with the name of the actor in role n,
	// and capitalizes the first letter.
	std::string FormatNavLabel(std::string a_label, const PlayerScene& a_scene)
	{
		const auto ids = a_scene.ActorIDs();
		for (std::size_t role = 0; role < ids.size(); ++role) {
			const std::string token = "{" + std::to_string(role) + "}";
			std::string       name = "partner";
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role])) {
				if (const auto n = actor->GetDisplayFullName(); n && *n) {
					name = n;
				}
			}
			for (auto pos = a_label.find(token); pos != std::string::npos; pos = a_label.find(token, pos + name.size())) {
				a_label.replace(pos, token.size(), name);
			}
		}
		if (!a_label.empty()) {
			a_label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(a_label[0])));
		}
		return a_label;
	}
}
