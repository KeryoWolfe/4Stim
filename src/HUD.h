#pragma once

#include <array>
#include <string>

// The in-scene HUD: Data\Interface\FourStimHUDMenu.swf, a non-pausing menu
// shown while there's a focused scene (see Bridge.h). The plugin only sends
// it data and reacts to its calls; everything on screen belongs to the
// movie, so HUD mods can replace it. The contract is HUD_API.md.
namespace HUD
{
	inline constexpr auto MENU_NAME = "FourStimHUDMenu";
	inline constexpr int  API_VERSION = 1;

	// From 4Stim.ini: the theme file name (without .json) and whether the HUD
	// is used at all.
	// a_columns: OStim's control scheme (sHUDControls=Columns): Up / Down
	// move in the list, Left / Right between the list and the tab buttons.
	void SetOptions(std::string a_theme, bool a_enabled, bool a_columns);

	// Reads the active theme and the Utility entries. Called on game load and
	// by FourStim.ReloadScenes.
	void LoadConfig();

	// Registers the menu with the game. Once, on kGameLoaded.
	void RegisterMenu();

	// The focused scene started, changed, changed speed or ended. Opens,
	// updates or closes the HUD. Any thread.
	void OnFocusedSceneChanged();

	// Closes the HUD and drops focus, e.g. when a save is loaded. Main thread.
	void Reset();

	// The Utility tab's rows again (an option changed something). Any thread.
	void RefreshUtility();

	// A climax in the focused scene: the movie's flash (optional HUD API
	// function PlayClimax, HUD_API.md). a_strength 0-1; a_size: how far in
	// from the screen's edges the glow reaches, as a share of the screen's
	// smaller side. Any thread.
	void PlayClimax(float a_strength, float a_size);

	// OStim's keyAlignment / keySearch: shows that tab (optional HUD API
	// function ShowTab) and gives the HUD the keys; pressed again, back to
	// Navigation. a_id: "align", "search", "utility", "navigation". Any thread.
	void ToggleTab(const char* a_id);

	// OStim's keyHideUI: hides the HUD or shows it again (optional HUD API
	// function SetHidden). Any thread.
	void ToggleHidden();

	// OStim's navigation keys (SetOsaUpKey, Down, Left, Right, Yes, Tog):
	// Up, Down, Left, Right, Accept and Toggle for the HUD, besides the arrow
	// keys, Enter and Esc. Main thread.
	void SetNavigationKeys(const std::array<std::uint32_t, 6>& a_keys);
	[[nodiscard]] bool IsNavigationKey(std::uint32_t a_code);

	// The controller's A while the HUD is hidden or doesn't have the keys:
	// shows it and gives it the keys. Any thread.
	void               Show();
	[[nodiscard]] bool IsHidden();

	// The controller's scheme (sPadScheme: false = 4Stim Modern, true =
	// OStim Classic) and fHUDFadeTime (OStim's fadeTime: seconds without
	// input before the HUD fades out and hands the keys back; 0 = never).
	// Sent with the control scheme (optional SetPadScheme / SetFadeTime).
	void               SetControllerOptions(bool a_classic, float a_fadeTime);
	[[nodiscard]] bool PadClassic();

	[[nodiscard]] bool IsOpen();
	[[nodiscard]] bool IsFocused();
	void               SetFocus(bool a_focused);  // any thread

	// Input while the HUD is focused: arrow keys / d-pad, Enter / A, Esc /
	// Backspace, B ("Back": the tab buttons, then hide). The controller's
	// other buttons are [Controller]'s, in main.cpp. Returns
	// true if a_event was one of them. Called from the input hook (main thread).
	bool HandleInput(const RE::ButtonEvent* a_event);
}
