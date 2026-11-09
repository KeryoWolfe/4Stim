#include "Core/Core.h"

namespace Core
{
	std::mt19937 g_shakeRandom{ std::random_device{}() };

	RE::FreeCameraState* FreeCamera()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->QCameraEquals(RE::CameraState::kFree)) {
			return nullptr;
		}
		return camera->GetState<RE::FreeCameraState>().get();
	}

	void UpdateClimaxShake()
	{
		auto& shake = g_climaxShake;
		if (!shake.active) {
			return;
		}
		const auto free = FreeCamera();
		if (!free) {
			// The free camera is gone (the scene ended): nothing to put back.
			shake = {};
			return;
		}
		const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - shake.start).count() / shake.duration;
		RE::NiPoint3 offset{};
		float        turnX = 0.0F;
		float        turnY = 0.0F;
		if (t < 1.0F) {
			// Strong at once, fading out quickly.
			const float fade = (1.0F - t) * (1.0F - t);
			std::uniform_real_distribution<float> unit(-1.0F, 1.0F);
			const float move = 0.6F * shake.strength * fade;     // game units
			const float turn = 0.0025F * shake.strength * fade;  // radians
			offset = { unit(g_shakeRandom) * move, unit(g_shakeRandom) * move, unit(g_shakeRandom) * move };
			turnX = unit(g_shakeRandom) * turn;
			turnY = unit(g_shakeRandom) * turn;
		} else {
			shake.active = false;
		}
		free->translation += offset - shake.moved;
		free->rotation.x += turnX - shake.turnedX;
		free->rotation.y += turnY - shake.turnedY;
		shake.moved = offset;
		shake.turnedX = turnX;
		shake.turnedY = turnY;
	}

	// Slow motion at a climax, as OStim's SetSlowMoOrgasms: the game at 0.3x
	// for 2.5 s (OStim runs the console's setGameSpeed; here the game timer's
	// global time multiplier, what Fallout 4's "sgtm" sets). A climax during
	// it starts the 2.5 s over. Main thread.
	std::atomic<std::uint32_t> g_slowMoGeneration = 0;

	void EndSlowMotion()
	{
		if (!g_slowMo) {
			return;
		}
		g_slowMo = false;
		if (const auto timer = RE::BSTimer::GetSingleton()) {
			timer->SetGlobalTimeMultiplier(1.0F, true);
		}
	}

	void StartSlowMotion()
	{
		const auto timer = RE::BSTimer::GetSingleton();
		if (!timer) {
			return;
		}
		timer->SetGlobalTimeMultiplier(0.3F, true);
		g_slowMo = true;
		const auto generation = ++g_slowMoGeneration;
		Scheduler::AfterOnMain(std::chrono::milliseconds(2500), [generation]() {
			if (g_slowMoGeneration == generation) {
				EndSlowMotion();
			}
		});
	}

	// The player's climax effects: shake, blur, glow, rumble, slow motion
	// (Excitement::Config). Slow motion only in scenes with the player, as
	// OStim's (its player thread); the rest also in a scene being watched.
	// a_playerClimax: the player is the one climaxing; OStim blurs only then
	// (its OStimNutEffect), the other effects for anyone's climax.
	void PlayClimaxEffects(bool a_withPlayer, bool a_playerClimax)
	{
		const auto& config = Excitement::Settings();
		const bool  freeCamera = FreeCamera() != nullptr;
		if (config.slowMo && a_withPlayer) {
			StartSlowMotion();
		}
		// OStim's SetUseScreenShake gates every shake (off by default).
		const bool shakeOn = config.useScreenShake && config.shake > 0.0F;
		if (shakeOn && freeCamera) {
			if (g_climaxShake.active) {
				// Already shaking: restart it from where the camera is now.
				g_climaxShake.start = std::chrono::steady_clock::now();
			} else {
				g_climaxShake = { true, std::chrono::steady_clock::now(), 0.8F, config.shake };
			}
			StartAutoplayTicks();
		}
		if (config.blurOn && config.blur > 0.0F && a_playerClimax) {
			// Depth of field with no in-focus range: everything blurs, for
			// a moment.
			RE::ImageSpaceModifierInstanceDOF::Trigger(0.0F, 0.0F, 0.0F, 0.0F,
				RE::ImageSpaceModifierInstanceDOF::DepthOfFieldMode::kFrontBack, std::min(config.blur, 1.0F), 0.5F);
		}
		if (config.flash > 0.0F) {
			HUD::PlayClimax(std::min(config.flash, 1.0F), config.glowSize);
		}
		// The normal cameras' shake and the rumble go through Papyrus.
		const bool papyrusShake = shakeOn && !freeCamera;
		if ((papyrusShake || config.rumble) && g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "ClimaxEffects"sv, callback, papyrusShake ? config.shake : 0.0F, config.rumble);
		}
	}

	// Fires an event (docs/EVENTS.md) in a_active: a_actor did it to
	// a_target, a_performer moving (roles; -1 = none). Its sound,
	// stimulation and reactions come from Events; the camera shake and
	// rumble only in a scene the player is in or watching (OStim shakes for
	// any thread). Then FourStim_OnEvent for registered scripts. Main thread.
	bool FireEvent(ActiveScene& a_active, const std::string& a_event, int a_actor, int a_target, int a_performer)
	{
		const auto effects = Events::Fire(a_event, a_active.actors, a_actor, a_target, a_performer);
		if (!effects.found) {
			return false;
		}
		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		const auto focused = GetPlayerScene();
		const bool watched = focused.Active() && focused.role0 == a_active.actors[0];
		if (withPlayer || watched) {
			const auto& config = Excitement::Settings();
			// SetUseScreenShake / SetUseRumble, as OStim's CameraUtil and
			// ControlUtil check (4Stim: fClimaxShake 0 turns shakes off).
			const float shake = config.useScreenShake && config.shake > 0.0F && effects.shakeDuration > 0.0F ? effects.shakeStrength : 0.0F;
			const float rumble = config.rumble && effects.rumbleDuration > 0.0F ? effects.rumbleStrength : 0.0F;
			const bool  freeCamera = FreeCamera() != nullptr;
			if (shake > 0.0F && freeCamera) {
				if (g_climaxShake.active) {
					// Already shaking: start over from where the camera is now
					// (its offset is undone as it goes).
					g_climaxShake.start = std::chrono::steady_clock::now();
					g_climaxShake.duration = std::max(effects.shakeDuration, 0.05F);
					g_climaxShake.strength = shake;
				} else {
					g_climaxShake = { true, std::chrono::steady_clock::now(), std::max(effects.shakeDuration, 0.05F), shake };
				}
				StartAutoplayTicks();
			}
			const float papyrusShake = freeCamera ? 0.0F : shake;
			if ((papyrusShake > 0.0F || rumble > 0.0F) && g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EventEffects"sv, callback, papyrusShake, effects.shakeDuration, rumble, effects.rumbleDuration);
			}
		}
		auto role = [&](int a_index) {
			return a_index >= 0 && static_cast<std::size_t>(a_index) < a_active.actors.size() ? a_active.actors[static_cast<std::size_t>(a_index)] : 0u;
		};
		SceneEvents::EventFired(a_event, role(a_actor), role(a_target), role(a_performer), a_active.actors, a_active.sceneID, a_active.threadID);
		return true;
	}

	// Hands a running scene's current scene and speed to the excitement
	// system (rates come from its actions), and keeps the clock running.
	void RefreshExcitement(const ActiveScene& a_scene)
	{
		const auto scene = SceneRegistry::Find(a_scene.sceneID);
		if (!scene) {
			return;
		}
		// The excitement rate by sex: a futa's the man's with futaUseMaleExcitement (OStim's).
		std::vector<SceneRegistry::Sex> sexes;
		for (const auto id : a_scene.actors) {
			sexes.push_back(ActorProperties::MaleExcitement(RE::TESForm::GetFormByID<RE::Actor>(id)) ? SceneRegistry::Sex::kMale : SceneRegistry::Sex::kFemale);
		}
		Excitement::Enter(a_scene.actors, sexes, *scene, a_scene.speed);
		for (const auto id : a_scene.actors) {
			Excitement::SetThreadStalled(id, a_scene.stallClimax);  // actors new to the thread too
		}
		// OStim's playbackSpeed for this speed.
		if (a_scene.speed >= 0 && static_cast<std::size_t>(a_scene.speed) < scene->speedInfo.size()) {
			PlaybackSpeed::Set(a_scene.actors, std::clamp(scene->speedInfo[static_cast<std::size_t>(a_scene.speed)].playbackSpeed, 0.05F, 10.0F));
		} else {
			PlaybackSpeed::Set(a_scene.actors, 1.0F);
		}
		// Faces and voices follow the scene too (OStim's changeNode).
		const float relativeSpeed = scene->speeds.empty() ? 0.0F : static_cast<float>(a_scene.speed) / static_cast<float>(scene->speeds.size());
		for (std::size_t role = 0; role < a_scene.actors.size(); ++role) {
			Expressions::Enter(a_scene.actors[role], *scene, role, relativeSpeed);
			Sounds::Enter(a_scene.actors[role], *scene, role, a_scene.actors);
		}
		// Peaks and action sounds (OStim's node handler), and lights.
		const bool withPlayer = std::ranges::any_of(a_scene.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		Peaks::Enter(a_scene.actors, scene, a_scene.speed, withPlayer);
		if (withPlayer) {
			SexToys::Enter(a_scene.actors, scene, a_scene.speed);  // OStim drives toys from the player's thread
		}
		Lights::Enter(a_scene.actors);
		StartAutoplayTicks();
	}

	// Ends a running scene through Papyrus: the focused scene the way the
	// HUD's "End scene" does, any other without touching the camera.
	void EndActiveScene(ActiveScene& a_scene, std::string_view a_why)
	{
		if (!g_vm || a_scene.actors.empty()) {
			return;
		}
		a_scene.ending = true;
		REX::INFO("Scene \"{}\": ending, {}", a_scene.sceneID, a_why);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_scene.actors[0]) {
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndPlayerScene"sv, callback);
		} else {
			std::vector<std::int32_t> ids;
			for (const auto id : a_scene.actors) {
				ids.push_back(static_cast<std::int32_t>(id));
			}
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndSceneOf"sv, callback, ids);
		}
	}

	// a_actorID, in a_active, climaxes now (OStim's climaxInner): counts it,
	// a man drops to the slowest speed, other mods are told, the player sees
	// the effects, and the scene ends if the settings say so.
	// OStim's climaxInner: the climax itself.
	void ClimaxInner(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		const auto& config = Excitement::Settings();
		std::erase_if(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; });
		Sounds::HoldForClimax(a_actorID, false);
		Excitement::Climaxed(a_actorID, ActorProperties::FemalePostClimax(RE::TESForm::GetFormByID<RE::Actor>(a_actorID)));
		SexToys::Climax(a_actorID);
		// As OStim's setAutoModeToMainStage: any climax moves auto mode on
		// to its main stage, from foreplay too.
		if (a_active.autoMode.on && a_active.autoMode.stage != ActiveScene::Auto::Stage::kMain) {
			a_active.autoMode.stage = ActiveScene::Auto::Stage::kMain;
			a_active.autoMode.cooldown = std::min(a_active.autoMode.cooldown, 3.0F);
		}
		const int  times = Excitement::TimesClimaxed(a_actorID);
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actorID);
		const bool penis = ActorProperties::HasPenis(actor);        // OStim's hasSchlong
		const bool maleClimax = ActorProperties::MaleClimax(actor);  // a man's, or a futa's with futaUseMaleClimax
		const auto role = static_cast<std::size_t>(std::ranges::find(a_active.actors, a_actorID) - a_active.actors.begin());
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		REX::INFO("Climax: {:08X} (role {}) in \"{}\", climax {}", a_actorID, role, a_active.sceneID, times);

		// As OStim: the climax of someone with a penis drops the scene to its
		// slowest speed (still in the climax animation: the scene it goes back
		// to starts slowest).
		if (penis && scene) {
			if (scene->IsTransition()) {
				a_active.relativeSpeed = 0.0F;
			} else if (a_active.speed > 0) {
				PlayOnActiveScene(a_active, a_active.sceneID, 0);  // sends the speed event
			}
		}

		SceneEvents::Climaxed(a_actorID, a_active.actors, a_active.sceneID, times, a_active.threadID);
		if (scene) {
			ApiForms::Climaxed(a_actorID, a_active.actors, *scene, times, a_active.threadID == 0);
		}
		// As OStim: climaxing takes it out of them (its 250 Stamina; Fallout
		// 4's is Action Points).
		if (const auto values = RE::ActorValue::GetSingleton(); actor && values && values->actionPoints) {
			actor->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *values->actionPoints, -250.0F);
		}

		// OStim's playClimaxSound: their climax sound, and the others'
		// climaxCommentOther, their own climaxCommentSelf after it.
		Sounds::Climax(a_actorID, a_active.actors, IsPlayer(actor));

		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		const auto focused = GetPlayerScene();
		const bool watched = focused.Active() && focused.role0 == a_active.actors[0];
		if (withPlayer || watched) {
			PlayClimaxEffects(withPlayer, IsPlayer(actor));
		}

		// As OStim's climaxInner: the all / player / male / female rules in
		// every scene, and endNPCSceneOnOrgasm on top for scenes without the
		// player. Each climax that counts sets the end timer again.
		bool end = false;
		if (config.endOnAll) {
			end = std::ranges::all_of(a_active.actors, [](std::uint32_t id) { return Excitement::TimesClimaxed(id) > 0; });
		} else {
			end = (IsPlayer(actor) && config.endOnPlayer) ||
			      (maleClimax && config.endOnMale) ||
			      (!maleClimax && config.endOnFemale);
		}
		if (!withPlayer && config.endNPCScenes) {
			end = true;
		}
		if (end && (a_active.endIn < 0.0F || a_active.endIn < config.endDelay)) {
			a_active.endIn = config.endDelay;
		}
	}

	// OStim's climax: at once, or once the moan they're making has ended
	// (awaitingClimaxInner; ResolveAwaitingClimaxes checks).
	void ClimaxNow(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		if (Sounds::IsMakingSound(a_actorID)) {
			std::erase_if(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; });
			a_active.awaitingClimax.push_back({ a_actorID, a_active.sceneID, false, true });
			Sounds::HoldForClimax(a_actorID, true);  // no new moan or reaction meanwhile
			REX::INFO("Climax: {:08X} waits for their moan to end (as OStim)", a_actorID);
			return;
		}
		ClimaxInner(a_active, a_actorID);
	}

	// a_actorID, in a_active, reached 100 (OStim's orgasm): with a climax
	// animation for their role (SetAutoClimaxAnims), it plays and the climax
	// itself waits for the animation's 4StimClimax annotation (OStim's
	// OStimClimax), or for the scene to move on from it; otherwise they
	// climax at once.
	void HandleClimax(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		const auto& config = Excitement::Settings();
		if (std::ranges::any_of(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; })) {
			return;  // already on its way
		}
		const auto role = static_cast<std::size_t>(std::ranges::find(a_active.actors, a_actorID) - a_active.actors.begin());
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto climaxScene = scene && role < scene->actors.size() ? scene->actors[role].AutoTransition("climax") : std::string{};
		if (config.climaxScenes && scene && !scene->IsTransition() && !climaxScene.empty() && a_active.queuedScene.empty() && !a_active.sequence &&
			PlayOnActiveScene(a_active, climaxScene, -1)) {
			if (ActorProperties::HasPenis(RE::TESForm::GetFormByID<RE::Actor>(a_actorID))) {
				a_active.relativeSpeed = 0.0F;  // back at the slowest speed after it
			}
			ArmAutoplay(a_active);
			a_active.awaitingClimax.push_back({ a_actorID, a_active.sceneID, false });
			REX::INFO("Climax: {:08X} (role {}) plays \"{}\", climaxing at its 4StimClimax annotation or when it ends", a_actorID, role, a_active.sceneID);
			return;
		}
		ClimaxNow(a_active, a_actorID);
	}

	// Climaxes waiting on their climax animation: the annotation came, or
	// the scene moved on from the animation (OStim climaxes on the next
	// scene change). Called from the scene clock.
	void ResolveAwaitingClimaxes(ActiveScene& a_active)
	{
		std::vector<std::uint32_t> due;
		std::vector<std::uint32_t> soundDone;
		for (const auto& entry : a_active.awaitingClimax) {
			if (entry.afterSound) {
				if (!Sounds::IsMakingSound(entry.actor)) {
					soundDone.push_back(entry.actor);
				}
			} else if (entry.annotated || _stricmp(entry.sceneID.c_str(), a_active.sceneID.c_str()) != 0) {
				due.push_back(entry.actor);
			}
		}
		for (const auto id : soundDone) {
			ClimaxInner(a_active, id);
		}
		for (const auto id : due) {
			ClimaxNow(a_active, id);
		}
	}
}
