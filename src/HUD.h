#pragma once

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
	void SetOptions(std::string a_theme, bool a_enabled);

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

	// A climax in the focused scene: the movie's flash (optional HUD API
	// function PlayClimax, HUD_API.md). a_strength 0-1. Any thread.
	void PlayClimax(float a_strength);

	// OStim's keyAlignment / keySearch: shows that tab (optional HUD API
	// function ShowTab) and gives the HUD the keys; pressed again, back to
	// Navigation. a_id: "align", "search", "utility", "navigation". Any thread.
	void ToggleTab(const char* a_id);

	// OStim's keyHideUI: hides the HUD or shows it again (optional HUD API
	// function SetHidden). Any thread.
	void ToggleHidden();

	[[nodiscard]] bool IsOpen();
	[[nodiscard]] bool IsFocused();
	void               SetFocus(bool a_focused);  // any thread

	// Input while the HUD is focused: arrow keys / d-pad, Enter / A, Esc /
	// Backspace / B, LB / RB, and the controller's speed buttons. Returns
	// true if a_event was one of them. Called from the input hook (main thread).
	bool HandleInput(const RE::ButtonEvent* a_event);
}
