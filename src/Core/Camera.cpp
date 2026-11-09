#include "Core/Core.h"

namespace Core
{
	// ---- Player controls during scenes ----
	// A dedicated input-enable layer, held for the whole scene, blocks
	// everything a scene shouldn't allow (menus, favorites, combat, activate,
	// jumping, VATS...) while leaving looking enabled. Movement is toggled
	// by the scene view: on in the free camera (which flies with the
	// movement keys), off in first person. Replaces SetPlayerAIDriven, which
	// also disabled the look/move input the free camera needs.

	RE::BSTSmartPointer<RE::BSInputEnableLayer> g_sceneLayer;

	void SetLayerUserEvents(std::int32_t a_flags, bool a_enable)
	{
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (manager && g_sceneLayer) {
			manager->EnableUserEvent(g_sceneLayer->layerID, static_cast<RE::UserEvents::USER_EVENT_FLAG>(a_flags), a_enable, RE::UserEvents::SENDER_ID::kScript);
		}
	}

	void SetLayerOtherEvents(std::int32_t a_flags, bool a_enable)
	{
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (manager && g_sceneLayer) {
			manager->EnableOtherEvent(g_sceneLayer->layerID, static_cast<RE::OtherInputEvents::OTHER_EVENT_FLAG>(a_flags), a_enable, RE::UserEvents::SENDER_ID::kScript);
		}
	}

	void LockPlayerControls(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto manager = RE::BSInputEnableManager::GetSingleton();
			if (!manager || g_sceneLayer) {
				return;
			}
			if (!manager->AllocateNewLayer(g_sceneLayer, "4Stim") || !g_sceneLayer) {
				REX::WARN("LockPlayerControls: couldn't allocate an input layer");
				return;
			}
			using U = RE::UserEvents::USER_EVENT_FLAG;
			using O = RE::OtherInputEvents::OTHER_EVENT_FLAG;
			SetLayerUserEvents(flag(U::kActivate) | flag(U::kMenu) | flag(U::kPOVSwitch) | flag(U::kFighting) | flag(U::kSneaking) |
								   flag(U::kMainFour) | flag(U::kJumping) | flag(U::kVATS),
				false);
			SetLayerOtherEvents(flag(O::kJournalTabs) | flag(O::kActivation) | flag(O::kFastTravel) | flag(O::kPOVChange) | flag(O::kVATS) |
									flag(O::kFavorites) | flag(O::kPipboyLight) | flag(O::kZKey) | flag(O::kRunning) | flag(O::kSprinting),
				false);
			REX::INFO("LockPlayerControls: layer {} active", g_sceneLayer->layerID);
		});
	}

	void ReleaseSceneLayer()  // main thread only
	{
		if (g_sceneLayer) {
			SetLayerUserEvents(flag(RE::UserEvents::USER_EVENT_FLAG::kAll), true);
			SetLayerOtherEvents(flag(RE::OtherInputEvents::OTHER_EVENT_FLAG::kAll), true);
			g_sceneLayer.reset();
			REX::INFO("Player controls restored");
		}
	}

	void UnlockPlayerControls(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() { ReleaseSceneLayer(); });
	}

	// ---- Scene camera and HUD ----
	// While a scene with the player (or one the player chose to watch) runs,
	// the vanilla HUD is hidden and the camera is the third-person free
	// camera, slowed by fFreeCameraSpeed from 4Stim.ini. At the end the HUD
	// comes back, the speed is restored, and the camera returns to the view
	// the player started in. The camera's real state is queried each time
	// (QCameraEquals) so it can't get out of step with the game.
	// (A first-person option is shelved: the game's first-person mode hides
	// the player's body, so it needs a head-pinned camera instead.)

	std::atomic<bool> g_sceneCameraActive = false;
	bool              g_startedFirstPerson = false;
	bool              g_startViewSaved = false;
	float             g_savedFreeCamSpeed = -1.0f;  // < 0: nothing saved

	constexpr auto FREE_CAM_SPEED_SETTING = "fFreeCameraTranslationSpeed:Camera"sv;

	void SetHUDVisible(bool a_visible)
	{
		if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage("HUDMenu", a_visible ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
		}
	}

	void ScaleFreeCameraSpeed()
	{
		const auto setting = RE::GetINISetting(FREE_CAM_SPEED_SETTING);
		if (!setting) {
			REX::WARN("Scene camera: game setting {} not found, free camera speed unchanged", FREE_CAM_SPEED_SETTING);
			return;
		}
		if (g_savedFreeCamSpeed < 0.0f) {
			g_savedFreeCamSpeed = setting->GetFloat();
		}
		// As OStim: the speed is set outright (SetCameraSpeed). The older
		// fFreeCameraSpeed multiplied the game's own instead.
		const float speed = g_settings.freeCamSpeedMult > 0.0F ? g_savedFreeCamSpeed * g_settings.freeCamSpeedMult : g_settings.freeCamSpeed;
		setting->SetFloat(speed);
		REX::INFO("Scene camera: free camera speed {} -> {}", g_savedFreeCamSpeed, setting->GetFloat());
	}

	// The world FOV during the scene (OStim's SetFreeCamFOV), put back after.
	float g_savedWorldFOV = -1.0F;  // < 0: nothing saved

	void SetSceneFOV()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || g_settings.freeCamFOV <= 0.0F) {
			return;
		}
		if (g_savedWorldFOV < 0.0F) {
			g_savedWorldFOV = camera->worldFOV;
		}
		camera->worldFOV = g_settings.freeCamFOV;
		REX::INFO("Scene camera: FOV {} -> {}", g_savedWorldFOV, camera->worldFOV);
	}

	void RestoreFOV()
	{
		if (g_savedWorldFOV < 0.0F) {
			return;
		}
		if (const auto camera = RE::PlayerCamera::GetSingleton()) {
			camera->worldFOV = g_savedWorldFOV;
		}
		g_savedWorldFOV = -1.0F;
	}

	// OStim's SetCustomTimescale: the game's TimeScale during a scene with
	// the player (0 = unchanged), put back after. Fallout4.esm's TimeScale
	// global is 0x3A, as Skyrim.esm's.
	float g_savedTimescale = -1.0F;  // < 0: nothing saved

	RE::TESGlobal* TimescaleGlobal()
	{
		return RE::TESForm::GetFormByID<RE::TESGlobal>(0x3A);
	}

	void SetSceneTimescale()
	{
		const auto global = g_settings.customTimescale > 0.0F ? TimescaleGlobal() : nullptr;
		if (!global) {
			return;
		}
		if (g_savedTimescale < 0.0F) {
			g_savedTimescale = global->value;
		}
		global->value = g_settings.customTimescale;
		REX::INFO("Scene camera: time scale {} -> {} (SetCustomTimescale)", g_savedTimescale, global->value);
	}

	void RestoreTimescale()
	{
		if (g_savedTimescale < 0.0F) {
			return;
		}
		if (const auto global = TimescaleGlobal()) {
			global->value = g_savedTimescale;
		}
		g_savedTimescale = -1.0F;
	}

	void RestoreFreeCameraSpeed()
	{
		if (g_savedFreeCamSpeed < 0.0f) {
			return;
		}
		if (const auto setting = RE::GetINISetting(FREE_CAM_SPEED_SETTING)) {
			setting->SetFloat(g_savedFreeCamSpeed);
		}
		g_savedFreeCamSpeed = -1.0f;
	}

	// Enters the free camera from third person. First person hides the
	// player's body (only the first-person arms are drawn), and entering the
	// free camera straight from first person keeps it hidden, so switch to
	// third person first and wait -- the game blends between views over a
	// few frames -- until it has actually taken effect. Main thread only.
	void EnterFreeCamera(int a_attempt)
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive || camera->QCameraEquals(RE::CameraState::kFree)) {
			return;
		}
		if (!camera->QCameraEquals(RE::CameraState::k3rdPerson)) {
			if (a_attempt == 0) {
				camera->SetState(camera->cameraStates[RE::CameraState::k3rdPerson].get());
			}
			if (a_attempt < 60) {  // about a second at 60 fps
				F4SE::GetTaskInterface()->AddTask([a_attempt]() { EnterFreeCamera(a_attempt + 1); });
				return;
			}
			REX::WARN("Scene camera: third person didn't take effect, entering free camera anyway");
		}
		camera->ToggleFreeCameraMode(false);  // false = don't freeze time
		REX::INFO("Scene camera: free camera");
	}

	// Records whether the player is in first person, so the scene can force
	// third person (Game.ForceThirdPerson, the game's full view change that
	// also swaps the first-person arms for the body) and still return the
	// player to first person afterwards. Call before forcing third person.
	void SaveStartView(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (camera && !g_sceneCameraActive) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
				g_startViewSaved = true;
			}
			g_focusNextScene = true;  // the scene being prepared is one the player watches
		});
	}

	// A loading screen or a screen fade is up (a teleport, a cell load).
	// Switching to the free camera then can leave the game stuck on the
	// loading screen, so the scene camera waits for it to clear.
	bool LoadingOrFading()
	{
		const auto ui = RE::UI::GetSingleton();
		return ui && (ui->GetMenuOpen("LoadingMenu") || (!g_sceneFade && ui->GetMenuOpen("FaderMenu")));
	}

	// Papyrus: FourStim.UseFades() / SetSceneFade (FourStimMenu's fades,
	// OStim's SetUseFades).
	bool UseFades(std::monostate)
	{
		return g_settings.useFades;
	}

	void SetSceneFade(std::monostate, bool a_fading)
	{
		g_sceneFade = a_fading;
	}

	// Runs a_task on the main thread once no loading screen or fade is up
	// (or after 20 s regardless).
	void AfterLoading(std::function<void()> a_task)
	{
		if (!LoadingOrFading()) {
			F4SE::GetTaskInterface()->AddTask(std::move(a_task));
			return;
		}
		REX::INFO("Scene camera: waiting for a loading screen / fade to clear");
		// Checked every 50 ms (20 s at most), then a little longer: the game
		// finishes settling after the menu closes.
		Scheduler::WhileThenOnMain(std::chrono::milliseconds(50), 400, []() { return LoadingOrFading(); }, std::chrono::milliseconds(250), std::move(a_task));
	}

	void BeginSceneCamera(std::monostate)
	{
		AfterLoading([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (!camera || g_sceneCameraActive) {
				return;
			}
			// The start this camera was for just failed (ReleaseFailedStart):
			// no camera, or nothing would ever end it.
			if (!GetPlayerScene().Active() && NowMs() - g_failedStartAt.load() < 3000) {
				REX::INFO("Scene camera: not started, the scene didn't start");
				return;
			}
			if (!g_startViewSaved) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
			}
			g_sceneCameraActive = true;
			SetHUDVisible(false);
			SetSceneFOV();
			SetSceneTimescale();
			if (g_settings.useFreeCam) {
				ScaleFreeCameraSpeed();
				EnterFreeCamera(0);
			}
		});
	}

	void EndSceneCamera(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			if (!g_sceneCameraActive) {
				return;
			}
			g_sceneCameraActive = false;
			g_startViewSaved = false;
			if (const auto camera = RE::PlayerCamera::GetSingleton()) {
				if (camera->QCameraEquals(RE::CameraState::kFree)) {
					camera->ToggleFreeCameraMode(false);
				}
				// As OStim's endSceneMode: third person, or first person with
				// SetForceFirstPerson (until 0.5.0 4Stim went back to the view
				// the scene started in). g_startedFirstPerson is still noted,
				// for the log.
				const bool firstPerson = g_settings.forceFirstPerson;
				const auto restore = firstPerson ? RE::CameraState::kFirstPerson : RE::CameraState::k3rdPerson;
				if (!camera->QCameraEquals(restore)) {
					camera->SetState(camera->cameraStates[restore].get());
				}
			}
			RestoreFreeCameraSpeed();
			RestoreFOV();
			RestoreTimescale();
			SetHUDVisible(true);
			REX::INFO("Scene camera: restored ({}; the scene started in {})", g_settings.forceFirstPerson ? "first person" : "third person",
				g_startedFirstPerson ? "first person" : "third person");
		});
	}

	// What a player scene changes outside the scene itself, put back: the
	// input layer, the scene camera's flags, free camera speed, FOV, slow
	// motion and the hidden HUD. For a game loaded (or started) while the
	// player was in a scene: none of these are saved, and none of them end
	// with the old game, so they stayed (the player's controls locked, the
	// camera zoomed). OStim resets its state on every load. The camera's own
	// state comes from the save, so it isn't touched. Main thread.
	void ResetPlayerSceneState(std::string_view a_why)
	{
		const bool wasActive = g_sceneCameraActive.exchange(false);
		const bool locked = static_cast<bool>(g_sceneLayer);
		g_startViewSaved = false;
		g_focusNextScene = false;
		g_sceneFade = false;
		RestoreFreeCameraSpeed();
		RestoreFOV();
		RestoreTimescale();
		EndSlowMotion();
		ReleaseSceneLayer();
		if (locked && g_vm) {
			// The player's AI driving (LockForScene) goes with the scene.
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "Game"sv, "SetPlayerAIDriven"sv, callback, false);
		}
		if (wasActive) {
			SetHUDVisible(true);
		}
		if (wasActive || locked) {
			REX::INFO("Player scene state reset ({}): controls, camera speed, FOV and HUD back", a_why);
		}
	}

	// OStim's SetFreeCamToggleKey: in and out of the free camera during a
	// scene. Main thread.
	void ToggleSceneFreeCamera()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive) {
			return;
		}
		if (camera->QCameraEquals(RE::CameraState::kFree)) {
			camera->ToggleFreeCameraMode(false);
			RestoreFreeCameraSpeed();
			REX::INFO("Scene camera: free camera off");
		} else {
			ScaleFreeCameraSpeed();
			EnterFreeCamera(0);
		}
	}

	// The scene moved (a furniture change): out of the free camera and back
	// in, so it starts at the player again. Main thread.
	void RecenterFreeCamera()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive || !camera->QCameraEquals(RE::CameraState::kFree)) {
			return;
		}
		camera->ToggleFreeCameraMode(false);
		F4SE::GetTaskInterface()->AddTask([]() { EnterFreeCamera(0); });
	}
}
