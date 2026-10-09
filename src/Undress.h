#pragma once

#include <cstdint>
#include <vector>

#include "SceneRegistry.h"

// Undressing (docs/UNDRESS.md), as OStim's: scene actors take off what's in
// some biped slots, all of them or only what the scene's actions need, and
// put it back on when the scene ends. The equipment itself is handled in
// Papyrus (FourStimUndress.psc); this keeps track of who took off what.
namespace Undress
{
	struct Config
	{
		bool             enabled = true;
		bool             atStart = false;     // everything off when a scene starts
		bool             partial = true;      // what each scene's actions need, as the scene gets there
		bool             fullMidScene = true; // actions marked fullStrip take everything off
		bool             player = true;       // the player too
		bool             redress = true;      // back on when the scene ends
		std::vector<int> slots{ 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 50 };  // biped slots 4Stim may undress
	};
	Config& Settings();

	// a_ids[role] play a_scene: takes off what its actions need (and at a
	// scene's start, with atStart, everything). Main thread.
	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start);

	// Takes off every undress slot (a_full) of a_id. Any thread.
	void StripAll(std::uint32_t a_id);

	// Papyrus reports what it took off.
	void NoteStripped(std::uint32_t a_id, const std::vector<std::uint32_t>& a_items);

	// Puts a_id's things back on (if redress is on, or a_force). Forgets them.
	void Redress(std::uint32_t a_id, bool a_force = false);

	// A game is loading: forget everything.
	void Clear();
}
