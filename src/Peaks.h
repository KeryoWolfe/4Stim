#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "SceneRegistry.h"

// Peaks and action sounds (docs/EVENTS.md, docs/SOUND.md), as OStim's node
// handler and sound players. A peak is the high point of an action's motion
// (a thrust, a stroke): found per action from its "peak" (a timer, or two
// sets of bones coming closest), or sent by the animation's 4StimPeak
// annotation when the scene says "peaksAnnotated". On a peak, the action's
// "peak" sounds play; its "bonedistance" and "loop" sounds keep their own
// time. Main thread (the scene clock).
namespace Peaks
{
	// The scene a_actors (role order) plays is a_scene at a_speed (0-based).
	// A new scene starts its peaks and sounds over; the same scene only
	// takes the new speed. a_playerScene: the player is in it (OStim's
	// player thread, for "playerThreadOnly" sounds).
	void Enter(const std::vector<std::uint32_t>& a_actors, const std::shared_ptr<const SceneRegistry::Scene>& a_scene, int a_speed, bool a_playerScene);

	// A 4StimPeak annotation on a_actorID's animation: a peak of action
	// a_action of their scene (if it has "peaksAnnotated").
	void Annotated(std::uint32_t a_actorID, int a_action);

	void Tick(float a_seconds);

	// The scene with any of these actors ended.
	void Leave(const std::vector<std::uint32_t>& a_actors);
	void Clear();
}
