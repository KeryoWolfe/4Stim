#pragma once

#include <cstdint>
#include <vector>

#include "SceneRegistry.h"

// Sounds actors make during scenes (docs/SOUND.md), as OStim's voice sets:
// files in Data\F4SE\Plugins\4Stim\Voice Sets\ (OStim's format, one per
// file) give each actor (by NPC, by voice type, or the default for their
// sex) moans and climax sounds. An actor whose scene actions say they moan
// moans every moanIntervalMin-Max, muffled while their mouth is busy, with
// the sound's event expression ("moan") while it plays; a climax plays the
// climax sound. A sound is OStim's sound descriptor ({"mod", "formid"}) or,
// in 4Stim, a sound file or a folder of them under Data\Sound\ ("file"),
// so no plugin records are needed.
namespace Sounds
{
	struct Config
	{
		float moanIntervalMin = 2.5F;  // moanIntervalMin (OStim: ms)
		float moanIntervalMax = 4.0F;  // moanIntervalMax
		float moanVolume = 1.0F;       // moanVolume, 0-1
		bool  disabled = false;        // 4Stim: bSounds=0
		// 4Stim: how sound files are played (BSAudioManager usage flags
		// and priority; iSoundFlags / iSoundPriority). OStim has no such
		// setting: Skyrim plays its voice sets from sound descriptors.
		std::uint32_t flags = 0x1A;
		std::uint8_t  priority = 128;
	};
	Config& Settings();

	// Re-reads the voice sets.
	void Reload();

	// a_id plays role a_role of a_scene. Main thread.
	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role);

	// a_id climaxed: their climax sound (OStim's playClimaxSound). Main thread.
	void Climax(std::uint32_t a_id);

	// Advances the moan timers. Main thread (the scene clock).
	void Tick(float a_seconds);

	// The actors left their scene: stop their sounds.
	void Leave(const std::vector<std::uint32_t>& a_ids);
	void Clear();
}
