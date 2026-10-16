#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Events (docs/EVENTS.md), as OStim's: something that happens in a scene, like
// a spank, fired by an animation's 4StimEvent annotation or by Papyrus
// (FourStim.CallEvent). Event files are in Data\F4SE\Plugins\4Stim\Events\
// (OStim's format, one per file, the id its file name): for each of the
// event's actor, target and performer, the stimulation it gives (up to a
// maxStimulation) and how long before they react (their voice set's event
// reaction, docs/SOUND.md); and a sound, camera shake and controller rumble.
// An event can name a "supertype" whose values it uses where it gives none
// (spankleft -> spank).
namespace Events
{
	// Re-reads the event files.
	void Reload();

	// The ids of all loaded events (lowercase).
	std::vector<std::string> List();

	// What the player should feel, for main.cpp to play (only in a scene
	// the player is in or watching).
	struct Effects
	{
		bool  found = false;  // the event exists
		float shakeStrength = 0.0F;
		float shakeDuration = 0.0F;
		float rumbleStrength = 0.0F;
		float rumbleDuration = 0.0F;
	};

	// Fires event a_event (case-insensitive) in a scene whose actors are
	// a_actors (role order): a_actor did it to a_target, a_performer moving
	// (roles; -1 = none). Plays its sound on the actor, gives each its
	// stimulation and their voice's reaction. Main thread.
	Effects Fire(const std::string& a_event, const std::vector<std::uint32_t>& a_actors, int a_actor, int a_target, int a_performer);
}
