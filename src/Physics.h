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
	struct Config
	{
		// bResetPhysicsAfterScene: after a scene, each actor's SMP physics
		// rebuilt from the reference pose (FSMP's full ResetPhysics; with a
		// swap, after the swap back). Off by default since 0.5.0: three of
		// the owner's crashes (23 October twice, 4 November) were inside
		// FSMP's rebuild (TryRebuildPendingActorsLocked -> ClearSystem,
		// freeing a collider tree already freed), the last a second after
		// the NPC's after-scene reset. The swap back alone rebuilds the
		// swapped body, 2 s after the scene, out of its pose.
		bool resetAfterScene = false;
	};
	Config& Settings();

	// a_actors in role order.
	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	// The scene moved on: an actor whose swap has a "toReceiving" switches
	// between it and "to" when they go from giving to receiving or back.
	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	void SceneEnded(const std::vector<std::uint32_t>& a_actors);

	// a_actor's 3D was rebuilt mid-scene (Undress's redraw of the player):
	// FSMP built their physics again from the mesh's own file, so the
	// scene's swap goes in again, a moment later. Any thread.
	void Reapply(std::uint32_t a_actor);

	// Scene actors dressed when their scene started (their naked body, whose
	// mesh names the physics file, not loaded): swapped once it is. Main
	// thread (the scene clock); at most once a second.
	void Tick();

	// Whether a_actor has a swap for their scene (Erection.h: such a body
	// needs no erection, only the penis bend).
	[[nodiscard]] bool IsSwapped(std::uint32_t a_actor);
}
