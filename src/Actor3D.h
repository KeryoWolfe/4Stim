#pragma once

#include <cstdint>
#include <vector>

// Notices when a scene actor's 3D is rebuilt, and puts back what 4Stim had
// done to it. SKSE has an event for this (RegisterForNiNodeUpdate, its
// NiNodeUpdateEvent stream for plugins); F4SE has none, so 4Stim watches the
// actors itself: an actor whose third-person 3D root is a different object
// than last time (or is back after being unloaded) has been rebuilt. The game
// rebuilds an actor for equipment changes, race or body changes, and
// Actor::Reset3D (4Stim's own undress redraw, other mods'); FSMP then builds
// its physics from the mesh's own file again, so A-Body's scene physics swap
// is lost (the owner's cot scene, 1 November). OStim needs none of this:
// Skyrim's SoS and SMP keep their state across a rebuild.
//
// What's put back: the physics swap (Physics::Reapply). The penis bend, eye
// look and expressions are applied every tick and find the new nodes
// themselves; LooksMenu puts its morphs back itself.
namespace Actor3D
{
	// Checks a_actors (every scene's). Main thread (the scene clock).
	void Tick(const std::vector<std::uint32_t>& a_actors);

	// The actors left their scene: stop watching them.
	void Leave(const std::vector<std::uint32_t>& a_actors);

	// A game is loading: forget everyone.
	void Clear();
}
