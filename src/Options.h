#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// OStim's options menu (its "options" files, docs/OPTIONS.md), in the HUD's
// Utility tab: pages of options, per actor or not, each calling a Papyrus
// function. Files in Data\F4SE\Plugins\4Stim\Options\ (subfolders too), in
// OStim's format:
//
//   pages:   name, description, icon, parent (a page's name, or null),
//            border, repeat ("actor": one page per actor)
//   options: name, page, repeat ("actor", or "equipObject" with equipType:
//            a "toggle" option and one "object" option per object of the
//            type), states { <name>: { description, icon, border,
//            function: "Script/Function" } }
//
// The function gets (state) / (state, actor) / (state, actor, object id),
// as OStim's. "{actor}" and "{type}" in descriptions are the actor's name
// and the object's. 4Stim's additions: "state" on an option picks which
// state shows ("muted", "stripped" or "equipped:<type>": "on" / "off"; OStim
// always shows the first, or its stateFaction, which 4Stim doesn't have),
// and "english" (key -> text) for "$4Stim_..." texts (docs/TRANSLATIONS.md).
namespace Options
{
	// Re-reads the files.
	void Reload();

	// One row of the Utility tab.
	struct Entry
	{
		std::string id;     // OPTION_PREFIX...
		std::string label;
		std::string icon;   // as the scene files write it ("4Stim/symbols/x")
		std::string border; // "rrggbb", "" = none
	};

	// Ids of the rows below start with this (HUD.cpp hands them back to Pick).
	inline constexpr std::string_view OPTION_PREFIX = "__opt";

	// The rows for the page open now: at the top, the root pages; in a page,
	// "Back", its pages and options. For the focused scene's actors (role
	// order); a different scene goes back to the top.
	std::vector<Entry> Rows(const std::vector<std::uint32_t>& a_actors);

	// Whether a page is open (the Utility tab then shows only its rows).
	bool InPage(const std::vector<std::uint32_t>& a_actors);

	// A row picked: into a page, back up one, or the option's function.
	void Pick(const std::string& a_id, const std::vector<std::uint32_t>& a_actors);
}
