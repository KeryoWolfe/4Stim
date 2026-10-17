#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// 4Stim's page in Fallout 4's Mod Configuration Menu (docs/MCM.md). MCM
// keeps what the player changes in Data\MCM\Settings\4Stim.ini, over the
// defaults in Data\MCM\Config\4Stim\settings.ini; 4Stim reads 4Stim.ini
// and then MCM's file over it. MCM's setting names carry a type letter:
// 4Stim's own already do (bLockScenes, fActorRadius); OStim's names get one
// with "4S_" (SetEndOnOrgasm is b4S_SetEndOnOrgasm), and a choice
// (sTheme...) is MCM's dropdown index, i4S_sTheme.
namespace MCM
{
	// MCM's file of the player's changes.
	std::filesystem::path SettingsPath();

	// Whether 4Stim's MCM page is installed (its defaults file is there).
	bool Installed();

	// MCM's setting name -> 4Stim.ini's (lowercase), and its value as
	// 4Stim.ini writes it (a dropdown's index as the choice's text).
	// Nothing for a name that isn't one of MCM's.
	std::optional<std::pair<std::string, std::string>> ToIni(std::string_view a_mcmKey, std::string_view a_value);

	// At game start: whichever of 4Stim.ini and MCM's file was changed last
	// wins. If 4Stim.ini is newer, its values are copied into MCM's file (so
	// the MCM shows them); else MCM's stay on top.
	void Sync();

	// The settings in effect (4Stim.ini with MCM's over it), to
	// Documents\My Games\Fallout4\4Stim\mcmsettings.json (OStim's file name).
	bool Export();

	// That file's values into MCM's file. False if there's no file.
	bool Import();

	// Forgets MCM's changes: 4Stim.ini's values again.
	void UseIni();
}
