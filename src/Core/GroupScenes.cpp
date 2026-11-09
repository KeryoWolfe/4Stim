#include "Core/Core.h"

namespace Core
{
	// The player's scene is about to end (end key, picker, HUD): nothing plays
	// on it any more (ActiveScene::ending), from the next frame. Main thread.
	void MarkPlayerSceneEnding()
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto focused = GetPlayerScene();
			if (const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr) {
				active->ending = true;
			}
		});
	}

	// An end's IdleStop in the same moment as a scene's PlayIdle can lose: the
	// game plays the scene's idle and the actors keep it after the end (the
	// owner's ZaZ pillory scene, 5 November: IdleStop returned true, and
	// both went on in the last scene's animation, the player held in place).
	// So when the end comes within a moment of a scene's idle, IdleStop goes
	// again shortly after, to whoever hasn't started another scene since.
	// From the IdleStop task (main thread), before TrackSceneStop.
	void StopAgainIfJustPlayed(const std::vector<std::uint32_t>& a_ids, RE::TESIdleForm* a_idle)
	{
		const auto active = a_ids.empty() ? nullptr : FindActiveScene(a_ids.front());
		if (!active || std::chrono::steady_clock::now() - active->lastPlayed > std::chrono::milliseconds(750)) {
			return;
		}
		REX::INFO("Scene end: \"{}\" played {} ms before its end: IdleStop again in a moment", active->sceneID,
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - active->lastPlayed).count());
		Scheduler::AfterOnMain(std::chrono::milliseconds(400), [a_ids, a_idle]() {
			for (const auto id : a_ids) {
				if (FindActiveScene(id)) {
					continue;  // in a new scene by now
				}
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, a_idle, nullptr);
				REX::INFO("Scene end: {:08X} IdleStop again returned {}", id, ok);
			}
		});
	}

	// Clears both actors' poses in the same task (same frame), mirroring
	// PlaySceneIdles, so the pair releases together.
	void StopPair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("StopPair called with a null actor");
			return;
		}

		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopPair: could not find IdleStop in Fallout4.esm");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor0, a_actor1, idle]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			const auto ok0 = p0 ? p0->PlayIdle(*a_actor0, idle, nullptr) : false;
			const auto ok1 = p1 ? p1->PlayIdle(*a_actor1, idle, nullptr) : false;
			REX::INFO("StopPair: {:08X} / {:08X} IdleStop returned {} / {}", a_actor0->GetFormID(), a_actor1->GetFormID(), ok0, ok1);
			StopAgainIfJustPlayed({ a_actor0->GetFormID(), a_actor1->GetFormID() }, idle);
			ClearFocusIfIn(a_actor0->GetFormID());
			ClearFocusIfIn(a_actor1->GetFormID());
			TrackSceneStop(a_actor0->GetFormID());
			TrackSceneStop(a_actor1->GetFormID());
		});
	}

	// ---- Group scenes (any number of actors) ----
	// The pair natives above, for a whole cast: all actors in role order.

	PlayerScene FocusFor(const std::vector<std::uint32_t>& a_ids, const std::string& a_sceneID)
	{
		PlayerScene scene;
		scene.role0 = a_ids.size() > 0 ? a_ids[0] : 0;
		scene.role1 = a_ids.size() > 1 ? a_ids[1] : 0;
		if (a_ids.size() > 2) {
			scene.more.assign(a_ids.begin() + 2, a_ids.end());
		}
		scene.sceneID = a_sceneID;
		return scene;
	}

	void IgnoreGroupCollision(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::erase(a_actors, nullptr);
		if (a_actors.size() < 2) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([a_actors]() {
			const PhysicsLock physics(a_actors.front());
			std::uint32_t     group = 0;
			for (const auto actor : a_actors) {
				const auto c = GetCharController(actor);
				const auto b = c ? c->GetBodyImpl() : nullptr;
				if (!b) {
					REX::WARN("IgnoreGroupCollision: {:08X} has no character controller", actor->GetFormID());
					continue;
				}
				const auto f = b->m_collisionFilterInfo;
				g_savedFilters.try_emplace(actor->GetFormID(), f);
				RE::CFilter cf{};
				cf.filter = f;
				if (group == 0) {
					group = cf.GetSystemGroup() != 0 ? cf.GetSystemGroup() : 0xF5A1;
				}
				cf.SetSystemGroup(group);
				const auto ok = c->SetCollisionFilterInfo(cf);
				REX::INFO("IgnoreGroupCollision: {:08X} filter {:08X} -> {:08X} ({})", actor->GetFormID(), f, cf.filter, ok);
			}
		});
	}

	// Puts the whole cast on the pending furniture's spot, or else on the
	// player's own spot (or the first actor's, without the player).
	void PlaceGroup(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::erase(a_actors, nullptr);
		if (a_actors.empty() || PlaceOnPendingFurniture(a_actors)) {
			return;
		}
		RE::Actor* anchor = a_actors.front();
		for (const auto actor : a_actors) {
			if (IsPlayer(actor)) {
				anchor = actor;
			}
		}
		Furniture::Spot spot{ anchor->data.location, anchor->data.angle.z };
		for (const auto actor : a_actors) {
			if (actor != anchor) {
				PlaceActorAt(actor, spot);
			}
		}
	}

	void PlayGroupIdles(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_sceneID)
	{
		std::erase(a_actors, nullptr);
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() != a_actors.size()) {
			REX::WARN("PlayGroupIdles: no {}-actor scene \"{}\"", a_actors.size(), a_sceneID);
			std::vector<std::uint32_t> failed;
			for (const auto actor : a_actors) {
				failed.push_back(actor->GetFormID());
			}
			ReleaseFailedStart(std::move(failed), "no such scene");
			return;
		}
		const int  speed = StartSpeed(*scene);  // OStim: a new thread starts at the scene's defaultSpeed
		const auto idles = scene->speeds[speed];
		const auto sceneID = scene->id;
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		F4SE::GetTaskInterface()->AddTask([ids, idles, sceneID, speed]() {
			bool        any = false;
			bool        withPlayer = false;
			std::string results;
			for (std::size_t role = 0; role < ids.size(); ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
				if (ok) {
					CuePlayed(ids[role], idles[role], sceneID);
				}
				any = any || ok;
				withPlayer = withPlayer || IsPlayer(actor);
				results += std::format("{}{:08X} {}", role ? ", " : "", ids[role], ok ? "ok" : "refused");
			}
			REX::INFO("PlayGroupIdles: \"{}\": {}", sceneID, results);
			const bool focus = g_focusNextScene.exchange(false) || withPlayer;
			if (!any) {
				ReleaseFailedStart(ids, "the game refused every animation");
				return;
			}
			TrackSceneStart(ids, sceneID, speed);
			if (focus) {
				auto focused = FocusFor(ids, sceneID);
				focused.speed = speed;
				if (const auto active = FindActiveScene(ids.front())) {
					focused.furniture = active->furnitureType;
				}
				SetPlayerScene(std::move(focused));
			}
		});
	}

	void StopGroup(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::erase(a_actors, nullptr);
		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopGroup: could not find IdleStop in Fallout4.esm");
			return;
		}
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		F4SE::GetTaskInterface()->AddTask([ids, idle]() {
			StopAgainIfJustPlayed(ids, idle);
			for (const auto id : ids) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, idle, nullptr);
				REX::INFO("StopGroup: {:08X} IdleStop returned {}", id, ok);
				ClearFocusIfIn(id);
				TrackSceneStop(id);
			}
		});
	}
}
