#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Physics swaps: for the length of a scene, an actor's SMP physics file is
// swapped for another (e.g. a male body's floppy genital physics for a rigid
// version), through FSMP's DynamicHDT.SwapPhysicsFile, and swapped back when
// the scene ends. Defined in JSON files under
// Data\F4SE\Plugins\4Stim\Physics\ (see docs/PHYSICS.md).
namespace Physics
{
	// a_actors in role order.
	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	// The scene moved on: an actor whose swap has a "toReceiving" switches
	// between it and "to" when they go from giving to receiving or back.
	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	void SceneEnded(const std::vector<std::uint32_t>& a_actors);

	// Whether a_actor has a swap for their scene (Erection.h: the swap is
	// their erection).
	[[nodiscard]] bool IsSwapped(std::uint32_t a_actor);

	// Flaccid mid-scene (a penis bend of -10, OStim's SOSFlaccid): back to
	// the body's own physics file; erect again: the scene's swap again.
	// A-Body's penis is rigid and erect in its mesh, and its own physics
	// file is what makes it hang: so flaccid is a swap, not a morph.
	void SetFlaccid(std::uint32_t a_actor, bool a_flaccid);
}
