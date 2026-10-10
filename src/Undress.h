#pragma once

#include <cstdint>
#include <string>
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
		float            itemDelay = 0.3F;    // seconds between items, taking off and putting on (0 = all at once)
		std::string      redressIdle;         // "Plugin.esp|0xID": an idle played before dressing again after a scene ("" = none)
		float            redressIdleLength = 3.0F;  // seconds the redress idle plays before the clothes go on
		std::vector<int> slots{ 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 50 };  // biped slots 4Stim may undress
	};
	Config& Settings();

	// a_ids[role] play a_scene: takes off what its actions need (and at a
	// scene's start, with atStart, everything). Main thread.
	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start);

	// Takes off every undress slot of a_id. a_byHand: asked for by the
	// player (the HUD), so it also undresses the player with
	// bUndressPlayer=0. Any thread.
	void StripAll(std::uint32_t a_id, bool a_byHand = false);

	// Whether 4Stim has taken anything off a_id (that isn't back on), and what.
	[[nodiscard]] bool                       IsStripped(std::uint32_t a_id);
	[[nodiscard]] std::vector<std::uint32_t> Stripped(std::uint32_t a_id);

	// Papyrus reports what it took off.
	void NoteStripped(std::uint32_t a_id, const std::vector<std::uint32_t>& a_items);

	// Puts a_id's things back on (if redress is on, or a_force). Forgets them.
	// a_afterScene: the scene ended, so the redress idle (if any) plays first.
	void Redress(std::uint32_t a_id, bool a_force = false, bool a_afterScene = false);

	// A game is loading: forget everything.
	void Clear();
}
