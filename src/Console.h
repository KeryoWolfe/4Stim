#pragma once

#include <string_view>

// Runs console commands from the plugin, as if typed in the console with
// a_ref selected (Fallout 4 has no Papyrus for some, e.g. "mfg morphs").
namespace Console
{
	// Main thread. a_ref may be null (no selection).
	void Run(RE::TESObjectREFR* a_ref, std::string_view a_command);
}
