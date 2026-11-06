#pragma once

#include <cstdint>
#include <string>
#include <utility>
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

	// An event's own stimulation and max stimulation for a side (0 actor,
	// 1 target, 2 performer), its supertypes' where it has none: what
	// OStim's OData.GetEventDefaultStimulation gives.
	std::pair<float, float> DefaultStimulation(const std::string& a_event, int a_role);

	// Whether a_event is a_parent or has it among its supertypes
	// (OStim's Event::isChildOf). Case-insensitive.
	bool IsChildOf(const std::string& a_event, const std::string& a_parent);

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
