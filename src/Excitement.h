#pragma once

#include <cstdint>
#include <vector>

#include "SceneRegistry.h"

// Excitement and climax, as OStim's. Every actor in a running scene has an
// excitement from 0 to 100. Each second it rises by what the scene's actions
// give that actor (their "stimulation", docs/ACTIONS.md): the strongest
// action in full, plus a tenth of each other one, faster at higher speeds.
// It only rises up to the highest "maxStimulation" of those actions (kissing
// alone tops out at 50), and above that it falls back slowly. At 100 the
// actor climaxes (main.cpp plays the consequences). Thread-safe.
namespace Excitement
{
	struct Config
	{
		bool  enabled = true;
		float maleMult = 1.0F;        // excitement gain multipliers, by sex
		float femaleMult = 1.0F;
		float decayRate = 0.5F;       // per second, down to what the scene allows
		float decayGrace = 5.0F;      // seconds after it last rose before it falls
		float postClimax = 10.0F;     // a woman's excitement after a climax, times climaxes so far...
		float postClimaxMax = 30.0F;  // ...up to this (a man's starts over at 0)
		bool  climaxScenes = true;    // play a role's "climax" scene when it climaxes
		bool  endOnPlayer = false;    // end a scene when the player climaxes,
		bool  endOnMale = true;       // when a man does,
		bool  endOnFemale = false;    // when a woman does,
		bool  endOnAll = false;       // or only once everyone has
		bool  endNPCScenes = true;    // NPC-only scenes: end at the first climax
		float endDelay = 4.0F;        // seconds after the climax

		// What the player sees at a climax in a scene they're in or watching.
		float shake = 1.0F;           // camera shake strength (0 = off)
		bool  blurOn = true;          // a brief full-screen blur (OStim's SetBlurOrgasms)...
		float blur = 0.5F;            // ...this strong (0 = off)
		float flash = 1.0F;           // white glow at the screen's edges (0 = off), from the HUD
		bool  rumble = true;          // controller rumble
		bool  slowMo = true;          // the game slows to 0.3x for 2.5 s (OStim's SetSlowMoOrgasms)
	};
	Config& Settings();

	// An actor in a scene: a_ids[role] plays role of a_scene at a_speed
	// (0-based). Starts tracking actors new to it and sets every actor's rate
	// and ceiling from the scene's actions. Call on every scene start,
	// change and speed change.
	void Enter(const std::vector<std::uint32_t>& a_ids, const std::vector<SceneRegistry::Sex>& a_sexes,
		const SceneRegistry::Scene& a_scene, int a_speed);

	// The actors left their scene: stop tracking them.
	void Leave(const std::vector<std::uint32_t>& a_ids);
	void Clear();

	// Advances everyone by a_seconds. Returns the actors who reached 100 and
	// should climax now (each once, until Climaxed is called for them).
	std::vector<std::uint32_t> Tick(float a_seconds);

	// a_id climaxed: counts it and drops their excitement (see Config).
	void Climaxed(std::uint32_t a_id);

	// Queries and changes, for Papyrus and the HUD. Get returns -1 for an
	// actor not in a scene.
	float Get(std::uint32_t a_id);
	void  Set(std::uint32_t a_id, float a_value);
	void  Add(std::uint32_t a_id, float a_value, bool a_useMultiplier);
	int   TimesClimaxed(std::uint32_t a_id);
	float Multiplier(std::uint32_t a_id);
	void  SetMultiplier(std::uint32_t a_id, float a_multiplier);  // on top of the sex multiplier
	void  SetStalled(std::uint32_t a_id, bool a_stalled);         // held just short of climax while true
	bool  IsStalled(std::uint32_t a_id);
	// Seconds until a_id climaxes at the current rate, or -1 if they won't.
	float TimeUntilClimax(std::uint32_t a_id);
}
