#pragma once

// 4Stim's settings in F4SE Menu Framework (DCCStudios' ImGui menu, a
// Fallout 4 fork of SKSE Menu Framework): the MCM's pages, in the same
// order, with the same labels, help and choices, read from 4Stim's MCM
// config (Data\MCM\Config\4Stim\config.json). A change is written to MCM's
// file of the player's changes (Data\MCM\Settings\4Stim.ini, as the MCM
// does) and takes effect at once. 4Stim's own; OStim has no counterpart.
// Nothing happens without the framework installed. docs/MCM.md.
namespace MenuPages
{
	// Once, after the translations are loaded: adds the pages if F4SE Menu
	// Framework is installed.
	void Register();
}
