#include "Core/Core.h"

namespace Core
{
	// ---- Hotkey ----
	// Hooks MenuControls::PerformInputProcessing (vtable slot 0, from its
	// BSInputEventReceiver base), which receives the whole input queue every
	// frame during normal play. Registering a BSInputEventUser with
	// MenuControls::RegisterHandler succeeded but never received any input.

	bool g_shiftDown = false;  // main thread (the input hook)

	// The hotkey went down and waits to be let go (OStim's hold-for-solo).
	bool g_hotkeyHeld = false;

	bool IsHotkeyEvent(const RE::ButtonEvent* a_event)
	{
		const auto code = static_cast<std::uint32_t>(a_event->idCode);
		if (a_event->device.get() == RE::INPUT_DEVICE::kKeyboard) {
			return code != 0 && code == g_settings.hotkey;
		}
		return a_event->device.get() == RE::INPUT_DEVICE::kGamepad && code != 0 && code == g_settings.padHotkey;
	}

	// Out of a scene the hotkey acts when it's let go (a short press: the
	// picker; held: alone). In a scene with the HUD it switches the HUD's
	// focus at once, as before.
	bool HotkeyWaitsForRelease()
	{
		return !(GetPlayerScene().Active() && HUD::IsOpen());
	}

	void HandleButton(const RE::ButtonEvent* a_event)
	{
		// Hold-to-scroll tracking for the picker: arrow keys and d-pad up/down.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
			int        dir = 0;
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				dir = code == 0x26 ? -1 : code == 0x28 ? 1 : 0;  // arrow up / down
			} else if (device == RE::INPUT_DEVICE::kGamepad) {
				dir = code == 0x1 ? -1 : code == 0x2 ? 1 : 0;  // d-pad up / down
			}
			if (dir != 0) {
				if (a_event->QJustPressed()) {
					g_navHeldSince = std::chrono::steady_clock::now();
					++g_navPressCount;
					g_navHeld = dir;
				} else if (!a_event->QPressed() && g_navHeld == dir) {
					g_navHeld = 0;
				}
			}
		} else {
			g_navHeld = 0;
		}

		// The thread API's control callbacks (OStim's UIState::HandleControl):
		// the navigation keys in a scene the player is in or watches, with
		// no menu up. With an external UI, that's all they do.
		if (const auto ui = RE::UI::GetSingleton(); a_event->QJustPressed() && GetPlayerScene().Active() && ui && !ui->GetMenuOpen(PICKER_MENU) &&
													ui->menuMode == 0 && !ui->GetMenuOpen("Console")) {
			if (const auto name = HUD::NavigationInputName(a_event)) {
				using C = FourStimNG_API::Thread::Controls;
				const std::string_view n(name);
				const std::optional<C> control = n == "Up"sv ? C::Up : n == "Down"sv ? C::Down : (n == "Left"sv || n == "PadLeft"sv) ? C::Left :
				                                 (n == "Right"sv || n == "PadRight"sv) ? C::Right : n == "Accept"sv ? C::Yes :
				                                 n == "Toggle"sv ? C::Toggle : (n == "Cancel"sv || n == "Back"sv) ? C::No : std::optional<C>{};
				if (control) {
					API::NotifyNGControl(*control);
				}
				if (HUD::ExternalUI()) {
					return;
				}
			}
		}

		// The HUD, while focused, gets the navigation keys (both press and
		// release), unless the console, a pausing menu or the picker is up.
		if (const auto ui = RE::UI::GetSingleton(); !HUD::ExternalUI() && HUD::IsFocused() && ui && !ui->GetMenuOpen(PICKER_MENU) &&
													ui->menuMode == 0 && !ui->GetMenuOpen("Console")) {
			if (HUD::HandleInput(a_event)) {
				return;
			}
		}

		// Shift (either one), held or not, for Shift + the hotkey. Keyboard
		// codes are Windows virtual-key codes: 0x10 Shift, 0xA0 / 0xA1 left / right.
		if (a_event->device.get() == RE::INPUT_DEVICE::kKeyboard) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			if (code == 0x10 || code == 0xA0 || code == 0xA1) {
				g_shiftDown = a_event->QPressed();
			}
		}

		// OStim's hold-for-solo: the hotkey is acted on when it's let go.
		// Held 2 s or more, out of a scene: a scene with you alone (OStim
		// starts one when no one is under the crosshair; 4Stim's picker
		// finds the partner, so a short press opens it as before).
		if (a_event->QReleased() && g_hotkeyHeld && IsHotkeyEvent(a_event)) {
			g_hotkeyHeld = false;
			const auto ui = RE::UI::GetSingleton();
			if (ui && (ui->menuMode > 0 || ui->GetMenuOpen("Console"))) {
				return;
			}
			if (a_event->QHeldDownSecs() >= 2.0F && !GetPlayerScene().Active()) {
				REX::INFO("Hotkey held {:.1f} s: a scene with you alone", a_event->QHeldDownSecs());
				StartSoloScene();
			} else {
				RunKeyAction("hotkey");
			}
			return;
		}

		if (!a_event->QJustPressed()) {
			return;
		}

		// Escape hatch: Esc (or the controller's B) always closes the picker,
		// whatever its AS3 side is doing.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				TypeIntoPicker(code);
			}
			if ((device == RE::INPUT_DEVICE::kKeyboard && code == 0x1B) || (device == RE::INPUT_DEVICE::kGamepad && code == 0x2000)) {
				if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
					queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kHide);
				}
			}
			return;
		}

		// Ignore the hotkey while the console or a game-pausing menu is open.
		if (const auto ui = RE::UI::GetSingleton(); ui && (ui->menuMode > 0 || ui->GetMenuOpen("Console"))) {
			return;
		}

		// The controller's A in a scene you're in or watching, while the HUD is
		// hidden or doesn't have the keys: shows it and gives it the keys (B
		// hid it: 4Stim's controller scheme, docs/HUD.md).
		if (a_event->device.get() == RE::INPUT_DEVICE::kGamepad) {
			const auto button = static_cast<std::uint32_t>(a_event->idCode);
			// A (and with the Classic scheme the d-pad too, as OStim's
			// navigation keys bring its menu up) shows the HUD and gives it the keys.
			const bool wakes = button == 0x1000 || (HUD::PadClassic() && (button == 0x1 || button == 0x2 || button == 0x4 || button == 0x8));
			if (wakes && HUD::IsOpen() && (HUD::IsHidden() || !HUD::IsFocused()) && GetPlayerScene().Active()) {
				HUD::Show();
				return;
			}
			// The controller buttons set in [Controller] (none by default).
			if (button == 0) {
				return;
			}
			const auto& st = g_settings;
			const std::string_view padAction = button == st.padSpeedUp ? "speedup"sv :
			                                   button == st.padSpeedDown ? "speeddown"sv :
			                                   button == st.padAutoMode ? "automode"sv :
			                                   button == st.padPullOut ? "pullout"sv :
			                                   button == st.padEnd ? "end"sv :
			                                   button == st.padSearch ? "search"sv :
			                                   button == st.padAlign ? "align"sv :
			                                   button == st.padFreeCam ? "freecam"sv :
			                                   button == st.padHideUI ? "hideui"sv :
			                                   button == st.padNpcScene ? "npcscene"sv :
			                                   button == st.padHotkey ? "hotkey"sv :
			                                                            ""sv;
			if (padAction == "hotkey"sv && HotkeyWaitsForRelease()) {
				g_hotkeyHeld = true;
				return;
			}
			if (!padAction.empty()) {
				RunKeyAction(padAction);
			}
			return;
		}

		// Keyboard codes are Windows virtual-key codes (N = 0x4E).
		if (a_event->device.get() != RE::INPUT_DEVICE::kKeyboard) {
			return;
		}

		const auto code = static_cast<std::uint32_t>(a_event->idCode);
		if (code == 0) {
			return;
		}
		// OStim's navigation keys, while the HUD doesn't have the keys: as
		// OStim shows its bars on any of them, show the HUD and give it the
		// keys (it has them from then on, so the next press moves).
		if (HUD::IsNavigationKey(code) && HUD::IsOpen() && GetPlayerScene().Active()) {
			HUD::Show();
			return;
		}
		// The NPC scene key (or Shift + the hotkey) before the hotkey itself.
		const bool npcKey = g_settings.npcSceneKey != 0 ? code == g_settings.npcSceneKey : (code == g_settings.hotkey && g_shiftDown);
		const std::string_view action = code == g_settings.speedUpKey ? "speedup"sv :
		                                code == g_settings.speedDownKey ? "speeddown"sv :
		                                (g_autoConfig.key != 0 && code == g_autoConfig.key) ? "automode"sv :
		                                code == g_settings.pullOutKey ? "pullout"sv :
		                                code == g_settings.endKey ? "end"sv :
		                                code == g_settings.searchKey ? "search"sv :
		                                code == g_settings.alignmentKey ? "align"sv :
		                                code == g_settings.freeCamKey ? "freecam"sv :
		                                code == g_settings.hideUIKey ? "hideui"sv :
		                                npcKey ? "npcscene"sv :
		                                code == g_settings.hotkey ? "hotkey"sv :
		                                                            ""sv;
		if (action == "hotkey"sv && HotkeyWaitsForRelease()) {
			g_hotkeyHeld = true;
			return;
		}
		if (!action.empty()) {
			RunKeyAction(action);
		}
	}

	// What a scene key does (4Stim.ini's keys, and the MCM's hotkeys through
	// FourStim.PressKey). Main thread.
	void RunKeyAction(std::string_view a_action)
	{
		const auto focused = GetPlayerScene();
		if (a_action == "speedup" || a_action == "speeddown") {
			// Speed keys, while the player is in a scene.
			if (focused.role0 != 0) {
				ChangeSceneSpeedImpl(a_action == "speedup" ? 1 : -1);
			}
		} else if (a_action == "automode") {
			// On / off for the scene you're in or watching.
			if (const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr; active && !active->noPlayerControl) {
				if (active->autoMode.on) {
					StopAutoMode(*active);
				} else {
					StartAutoMode(*active);
				}
				Notify(active->autoMode.on ? TR("$4stim_auto_mode_on", "Auto mode on") : TR("$4stim_auto_mode_off", "Auto mode off"));
			}
		} else if (a_action == "pullout" || a_action == "end" || a_action == "search" || a_action == "align" || a_action == "freecam" || a_action == "hideui") {
			// OStim's other scene keys, for the scene you're in or watching:
			// as OStim's EventListener.
			if (!focused.Active()) {
				return;
			}
			if (a_action == "pullout") {
				// OStim's No key in its align menu (4Stim: the pull-out key,
				// with the Align tab open): the next step size.
				if (HUD::CycleAlignStep()) {
					return;
				}
				if (const auto active = FindActiveScene(focused.role0); active && !AutoPullOut(*active)) {
					Notify(TR("$4stim_no_pullout", "4Stim: nowhere to pull out to from here"));
				}
			} else if (a_action == "end") {
				if (g_vm) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					MarkPlayerSceneEnding();
					Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndPlayerScene"sv, callback);
				}
			} else if (a_action == "search") {
				API::NotifyNGControl(FourStimNG_API::Thread::Controls::SearchMenu);
				if (!HUD::ExternalUI()) {
					ShowPicker(PickerMode::kSearch);
				}
			} else if (a_action == "align") {
				API::NotifyNGControl(FourStimNG_API::Thread::Controls::AlignMenu);
				HUD::ToggleTab("align");
			} else if (a_action == "freecam") {
				ToggleSceneFreeCamera();
			} else {
				API::NotifyNGControl(FourStimNG_API::Thread::Controls::KEY_HIDE);
				HUD::ToggleHidden();
			}
		} else if (a_action == "npcscene") {
			// The picker for a scene without you, whether or not you're in one.
			if (g_vm) {
				REX::INFO("NPC scene key pressed");
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "OnNPCSceneHotkey"sv, callback,
					g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
			}
		} else if (a_action == "hotkey") {
			// In a scene with the HUD up, the hotkey switches the HUD's focus.
			if (focused.Active() && HUD::IsOpen()) {
				HUD::SetFocus(!HUD::IsFocused());
				return;
			}
			if (g_vm) {
				REX::INFO("Hotkey pressed");
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "OnHotkey"sv, callback,
					g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
			}
		} else {
			REX::WARN("Keys: unknown action \"{}\"", a_action);
		}
	}
}
