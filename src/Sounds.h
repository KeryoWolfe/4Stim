#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
		float soundVolume = 1.0F;      // soundVolume, 0-1: action and event sounds
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

	// A sound to play: OStim's sound descriptor, or 4Stim's sound files (one
	// picked at random). Shared by voice sets, actions and events.
	struct Sound
	{
		std::uint32_t            descriptor = 0;  // a sound descriptor (SNDR) form
		std::vector<std::string> files;           // paths under Data\ ("Sound\...")

		[[nodiscard]] bool Empty() const { return descriptor == 0 && files.empty(); }
	};

	// From a JSON object with "file" (a file or folder under Data\Sound\) or
	// OStim's "sound" ({"mod", "formid"}). Empty if neither is usable.
	Sound ReadSound(const nlohmann::json& a_entry);

	// {"mod": "Fallout4.esm", "formid": "0x..."} -> its full form ID (0 if
	// not loaded).
	std::uint32_t ReadForm(const nlohmann::json& a_value);

	// A sound playing on an actor.
	struct Playing
	{
		RE::BSSoundHandle                     handle;           // files: the engine's handle
		std::chrono::steady_clock::time_point until{};          // descriptors: an estimate
		bool                                  started = false;

		[[nodiscard]] bool IsPlaying() const;
		void               Stop();
	};

	// Plays a_sound on a_actor (following them) at a_volume (0-1). Main
	// thread. Nothing plays with sound off (bSounds=0).
	Playing Play(const Sound& a_sound, RE::Actor* a_actor, float a_volume);

	// An event happened to a_id (docs/EVENTS.md): their voice set's reaction
	// for it after a_delay seconds, if it has one. a_events is the event and
	// its supertypes, nearest first; a_which is "actor", "target" or
	// "performer" (OStim's eventActorReactions...). Main thread.
	void ReactToEvent(std::uint32_t a_id, const std::vector<std::string>& a_events, std::string_view a_which, float a_delay);

	// a_id plays role a_role of a_scene. Main thread.
	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role);

	// a_id climaxed: their climax sound (OStim's playClimaxSound). Main thread.
	void Climax(std::uint32_t a_id);

	// Advances the moan timers. Main thread (the scene clock).
	void Tick(float a_seconds);

	// OStim's OActor.Mute / Unmute / IsMuted, for actors in a scene: no moans,
	// climax or event sounds while muted (an actor property's mute counts as
	// muted too). Main thread.
	void SetMuted(std::uint32_t a_id, bool a_muted);
	bool IsMuted(std::uint32_t a_id);

	// The actors left their scene: stop their sounds.
	void Leave(const std::vector<std::uint32_t>& a_ids);
	void Clear();
}
