#include "Core/Core.h"

namespace Core
{
	// ---- Visibility check (diagnostic) ----
	// Testers and the owner have had an actor placed and animated in a scene
	// (the log says so) but not drawn: the player once, an NPC on 1.10.163.
	// Every 2 s the scene guard looks at each scene actor's 3D and logs it at
	// the start and whenever it changes, with a warning when something would
	// hide them: no 3D, the root culled, faded out, no geometry or all of it
	// culled, or the 3D somewhere other than the actor. Logging only.
	struct Visibility
	{
		bool  has3D = false;
		bool  rootCulled = false;
		float fade = 1.0F;  // the root fade node's, 1 = fully shown
		int   geometry = 0;
		int   culledGeometry = 0;
		float offset = 0.0F;  // the 3D's distance from the actor's position
	};

	void CountGeometry(RE::NiAVObject* a_object, Visibility& a_out, int a_depth, bool a_culled)
	{
		if (!a_object) {
			return;
		}
		const bool culled = a_culled || a_object->GetAppCulled();
		if (a_object->IsGeometry()) {
			++a_out.geometry;
			if (culled) {
				++a_out.culledGeometry;
			}
			return;
		}
		if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
			for (const auto& child : node->children) {
				CountGeometry(child.get(), a_out, a_depth - 1, culled);
			}
		}
	}

	Visibility CheckVisibility(RE::Actor* a_actor)
	{
		Visibility v;
		const auto root = IsPlayer(a_actor) ? a_actor->Get3D(false) : a_actor->Get3D();  // the player's third-person 3D
		if (!root) {
			return v;
		}
		v.has3D = true;
		v.rootCulled = root->GetAppCulled();
		if (const auto fade = root->IsFadeNode()) {
			v.fade = fade->currentFade;
		}
		CountGeometry(root, v, 64, false);
		v.offset = root->GetWorldTranslate().GetDistance(a_actor->data.location);
		return v;
	}

	void LogVisibility(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		const auto id = a_actor->GetFormID();
		const auto now = std::chrono::steady_clock::now();
		auto& last = g_visibilityLast[id];
		if (!last.first.empty() && now - last.second < std::chrono::seconds(2)) {
			return;
		}
		last.second = now;
		const auto v = CheckVisibility(a_actor);
		const bool hidden = !v.has3D || v.rootCulled || v.fade < 0.1F || v.geometry == 0 || v.culledGeometry == v.geometry || v.offset > 100.0F;
		// What's compared for "changed": rounded, so a fade or a sway doesn't log every time.
		const auto summary = !v.has3D ? std::string("no 3D") :
		                                std::format("root {}, fade {:.1f}, {} geometry ({} culled), 3D {} the actor", v.rootCulled ? "CULLED" : "shown", v.fade,
											v.geometry, v.culledGeometry, v.offset > 100.0F ? std::format("{:.0f} units from", v.offset) : std::string("at"));
		if (summary == last.first) {
			return;
		}
		last.first = summary;
		if (hidden) {
			REX::WARN("Visibility: {:08X} in \"{}\" may not be drawn: {}", id, a_sceneID, summary);
		} else {
			REX::INFO("Visibility: {:08X} in \"{}\": {}", id, a_sceneID, summary);
		}
	}

	// Fallout 4 only (OStim has nothing like it): the player in a scene but
	// not drawn. In first person the game culls the player's third-person 3D;
	// a scene started from first person forces third person and the free
	// camera, and a first-person body mod (True First Person, in the owner's
	// log: "First-person body OFF", "Third person forced by the engine") can
	// leave it culled, so the player was in the scene, animated, and invisible
	// ("the player character is missing"). Shown again. Main thread.
	void ShowPlayerBody(RE::Actor* a_player, const std::string& a_sceneID)
	{
		static int shown = 0;
		const auto body = a_player ? a_player->Get3D(false) : nullptr;  // third person
		if (!body || !body->GetAppCulled()) {
			return;
		}
		// Not in first person: there the culling is the game's own, and the
		// third-person skeleton isn't animated. A scene ending put the camera
		// back in first person half a second before the scene was let go, and
		// this showed the body then (the owner's log, 5 November): a T-posed
		// player with his head bent, and FSMP crashed in its collider when
		// the physics swap went back on that body a moment later.
		if (const auto camera = RE::PlayerCamera::GetSingleton(); camera && camera->QCameraEquals(RE::CameraState::kFirstPerson)) {
			return;
		}
		body->SetAppCulled(false);
		if (++shown <= 5) {
			REX::INFO("Scene guard: the player's body was hidden in \"{}\" (third-person 3D culled, as first person leaves it): shown again", a_sceneID);
		}
	}

	// Fallout 4 only (OStim has nothing like it): an actor in a scene drawn
	// fully faded out. The third-person camera fades the player (and an NPC it
	// comes close to) out as it closes in on them, as it does against a wall;
	// once the scene switches to the free camera nothing fades them back in,
	// so the player was in the scene, animated, and invisible (the owner's
	// log, a wall scene: "fade 0.0" on the player's 3D, 1.0 in the next
	// scene). Faded back in. Main thread.
	// The actor's own fade, below the 3D's: Actor::UpdateAlpha (vtable slot
	// 0xFB) sets the 3D's fade every update from the high process's
	// fadeAlpha (its fade-in / fade-out / teleport-fade state, ID 2231941
	// steps it), times middleHigh's alphaMult (Papyrus SetAlpha), times
	// what Invisibility leaves. So an actor left in a fade-out
	// (kOut, alpha 0) is pushed back to invisible every frame, whatever the
	// scene guard does to the 3D: a 1.10.163 tester's male NPCs, "keeps
	// being faded out" 57 times a second even with the camera's fade off.
	// A plain fade-out on a scene actor (or a fade-in still under way) is
	// ended at once, fully shown, and an alphaMult below 1 is put back to
	// 1; each logged once a scene. Teleport fades (a MoveTo with a fade) and
	// fades before disabling or deleting are the game's and left alone.
	// Both fields are where AE has them on 1.10.163 too (checked against its
	// executable: the same fade functions write the same offsets).
	void RestoreActorFade(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		if (!process) {
			return;
		}
		static std::unordered_map<std::uint32_t, std::string> logged;
		const auto log = [&](const std::string& a_what) {
			if (auto& last = logged[a_actor->GetFormID()]; last != a_sceneID + a_what) {
				last = a_sceneID + a_what;
				REX::INFO("Scene guard: {:08X} in \"{}\" {}", a_actor->GetFormID(), a_sceneID, a_what);
			}
		};
		if (const auto high = process->high) {
			// Done here, not left to the game: turned into a fade-in (kIn),
			// the player's never got anywhere (the owner's log, 6 November:
			// a chair scene's start, the camera fading him out before its
			// fade was off; then "actor fade state 1, alpha 0.00" for the
			// whole scene, invisible). So a fade-out, or a fade-in still
			// short of 1, ends at once: no fade, fully shown.
			using FADE = RE::HighProcessData::FADE_STATE;
			const auto state = high->fadeState.get();
			if (state == FADE::kOut || (state == FADE::kIn && high->fadeAlpha < 0.99F)) {
				log(std::format("was fading {} (the game's own actor fade, alpha {:.2f}): shown again", state == FADE::kOut ? "out" : "in", high->fadeAlpha));
				high->fadeState = FADE::kNormal;
				high->fadeAlpha = 1.0F;
			}
		}
		if (const auto middle = process->middleHigh; middle && middle->alphaMult < 0.99F) {
			log(std::format("had alpha {:.2f} (SetAlpha, by a script or another mod): back to 1", middle->alphaMult));
			middle->alphaMult = 1.0F;
		}
	}

	void RestoreFade(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		RestoreActorFade(a_actor, a_sceneID);
		const auto root = IsPlayer(a_actor) ? a_actor->Get3D(false) : a_actor->Get3D();
		const auto fade = root ? root->IsFadeNode() : nullptr;
		if (!fade || fade->currentFade >= 0.99F) {
			return;
		}
		// Logged once per actor and scene, then how many times it came back:
		// in the owner's log a single fade-in wasn't enough (the player was
		// still missing), so it's now done on every tick of the scene clock,
		// and the count says whether the game keeps fading them out.
		struct Seen
		{
			std::string                           scene;
			int                                   again = 0;
			std::chrono::steady_clock::time_point lastLog{};
		};
		static std::unordered_map<std::uint32_t, Seen> seen;
		auto&      entry = seen[a_actor->GetFormID()];
		const auto now = std::chrono::steady_clock::now();
		if (entry.scene != a_sceneID) {
			entry = { a_sceneID, 0, now };
			REX::INFO("Scene guard: {:08X} in \"{}\" was faded out ({:.2f}, the camera's fade as it closed in): faded back in", a_actor->GetFormID(), a_sceneID,
				fade->currentFade);
		} else if (++entry.again; now - entry.lastLog > std::chrono::seconds(5)) {
			entry.lastLog = now;
			// What the game's alpha is made of (Actor::UpdateAlpha): if these
			// are all 1, it's Invisibility or something else.
			const auto process = a_actor->currentProcess;
			const auto high = process ? process->high : nullptr;
			const auto middle = process ? process->middleHigh : nullptr;
			REX::WARN("Scene guard: {:08X} in \"{}\" keeps being faded out ({} times so far, now {:.2f}; actor fade state {}, alpha {:.2f}, alpha mult {:.2f}): fading them back in each tick",
				a_actor->GetFormID(), a_sceneID, entry.again, fade->currentFade, high ? static_cast<int>(std::to_underlying(high->fadeState.get())) : -1,
				high ? high->fadeAlpha : -1.0F, middle ? middle->alphaMult : -1.0F);
		}
		fade->currentFade = 1.0F;
		fade->currentDecalFade = 1.0F;
	}

	// Every tick of the scene clock (not the guard's quarter second): keeps
	// every scene actor faded in.
	// ---- No camera fade on scene actors ----
	// Not in OStim (Skyrim's camera doesn't fade actors this way). Fallout
	// 4's camera finds what's between it and the player with a shape cast
	// (ID 2214659 on 1.11.240) and hands back the actor it's inside or
	// near, for the game to fade out: within fActorFadeOutLimit:Camera, plus
	// the actor's bound up to fActorFadeOutBoundMax:Camera. A scene's free
	// camera sits right by the actors, so one of them was faded out every
	// frame, faster than the scene guard could fade them back in (a 1.10.163
	// tester's log: "keeps being faded out", 586 times in 10 seconds, and
	// the male actor never seen). With both at 0 the cast's distance check
	// never passes, so nothing is faded: set while any scene runs, the
	// player's own values back when the last one ends.
	struct CameraFadeSetting
	{
		const char* name;
		float       own = 0.0F;
	};
	CameraFadeSetting g_cameraFade[]{ { "fActorFadeOutLimit:Camera" }, { "fActorFadeOutBoundMax:Camera" } };
	bool              g_cameraFadeOff = false;

	void SetCameraFadeOff(bool a_off)
	{
		if (a_off == g_cameraFadeOff) {
			return;
		}
		const auto ini = RE::INISettingCollection::GetSingleton();
		if (!ini) {
			return;
		}
		for (auto& setting : g_cameraFade) {
			const auto found = ini->GetSetting(setting.name);
			if (!found || found->GetType() != RE::Setting::SETTING_TYPE::kFloat) {
				REX::WARN("Scene camera: {} not found; the camera may still fade scene actors", setting.name);
				continue;
			}
			if (a_off) {
				setting.own = found->GetFloat();
				found->SetFloat(0.0F);
			} else {
				found->SetFloat(setting.own);
			}
		}
		g_cameraFadeOff = a_off;
		REX::INFO("Scene camera: actor fade {} ({} {:.0f}, {} {:.0f})", a_off ? "off while scenes run" : "back on", g_cameraFade[0].name, g_cameraFade[0].own,
			g_cameraFade[1].name, g_cameraFade[1].own);
	}

	void KeepFadedIn()
	{
		SetCameraFadeOff(!g_activeScenes.empty());
		if (!g_guardScenes) {
			return;
		}
		for (const auto& active : g_activeScenes) {
			for (const auto id : active.actors) {
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
					RestoreFade(actor, active.sceneID);
				}
			}
		}
	}
}
