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

	// Whether a_actor has a swap for their scene (Erection.h: such a body
	// needs no erection, only the penis bend).
	[[nodiscard]] bool IsSwapped(std::uint32_t a_actor);
}
