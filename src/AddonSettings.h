#pragma once

#include <cstddef>

// OStim's add-on settings (its SKSE\Plugins\OStim\settings folder and MCM
// Addons page, Settings/AddonPage.cpp): an add-on's JSON file declares
// toggles, sliders, dropdowns and keymaps, each kept in one of its global
// variables. 4Stim reads them from Data\F4SE\Plugins\4Stim\Settings\ in
// OStim's format (docs/MCM.md, "Add-ons").
//
// Departure from OStim, forced by Fallout 4: SkyUI builds MCM pages at run
// time; Fallout 4's MCM reads a fixed config.json. So at startup, before
// the game reads any menu, 4Stim writes an "Add-Ons" page into its own
// config.json (Data\MCM\Config\4Stim\), bound straight to the add-ons'
// globals (MCM's GlobalValue), and takes it out again when no add-on has
// settings. The globals are saved with the game, as OStim's.
namespace AddonSettings
{
	// Reads the folder and writes (or removes) the page. Returns how many
	// settings it has. Safe to call when the MCM page isn't installed (does
	// nothing).
	std::size_t WritePage();
}
