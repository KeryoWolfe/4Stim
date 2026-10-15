#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SceneRegistry.h"

// Facial expressions during scenes (docs/EXPRESSIONS.md), as OStim's:
// expression files in Data\F4SE\Plugins\4Stim\Facial Expressions\ (OStim's
// format, one per file), in four layers, higher ones replacing lower ones
// in the slots they use (the emotion, the mouth, the eyelids, the brows):
//
// 1. underlying: picked from the scene's actions (actionActors /
//    actionTargets), else the "default" set, again every few seconds;
// 2. event: a short one for "moan" and "climax";
// 3. override: an action's expressionOverride set ("openmouth", "tongue").
//
// OStim sets Skyrim's expression, phoneme and modifier values; Fallout 4 has
// face morphs instead, so each Skyrim type is mapped onto morphs by
// Data\F4SE\Plugins\4Stim\Expression Morphs.json, and set with the console's
// "mfg morphs". An expression can also give Fallout 4 morphs directly
// ("morphs").
namespace Expressions
{
	struct Config
	{
		bool  disabled = false;      // OStim's noFacialExpressions
		float durationMin = 1.0F;    // SetExpressionDurationMin (OStim: ms), seconds between underlying picks
		float durationMax = 3.0F;    // SetExpressionDurationMax
	};
	Config& Settings();

	// Re-reads the expression files and the morph map.
	void Reload();

	// a_id plays role a_role of a_scene at a_relativeSpeed (speed / speed
	// count, as OStim's). Picks their expressions when the scene changed.
	// Main thread.
	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role, float a_relativeSpeed);

	// An event expression ("moan", "climax"): kept until ClearEvent, or for
	// its own duration with a_timed. Returns false if there's none for it.
	bool SetEvent(std::uint32_t a_id, const std::string& a_event, bool a_timed);
	void ClearEvent(std::uint32_t a_id);

	// Advances the timers and applies changes. Main thread (the scene clock).
	void Tick(float a_seconds);

	// The actors left their scene: their face back to normal.
	void Leave(const std::vector<std::uint32_t>& a_ids);
	void Clear();
}
