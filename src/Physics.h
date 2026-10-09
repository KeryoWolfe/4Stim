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
	void SceneEnded(const std::vector<std::uint32_t>& a_actors);
}
