#pragma once

#include <optional>
#include <thread>

// Who has a penis, from the body (roadmap 2b.7). OStim asks Schlongs of
// Skyrim's or TNG's factions when no actor property decides
// (CompatibilityTable::hasSchlong, its useSoSSex / useTNGSex). Fallout 4
// has neither, so 4Stim looks at what the actor wears, their naked body
// included: any mesh skinned to ZeX's Penis_* bones means a penis. 4Stim's
// own equip objects (the strap-on, the tongues) don't count; a wearable
// penis from another mod does (Fallout 4's futa).
namespace PenisMesh
{
	// Called with the game's main thread at kGameLoaded: the 3D is read
	// only there.
	void SetMainThread(std::thread::id a_id);

	// true / false by the body; nullopt when it can't tell (no 3D loaded,
	// or asked off the main thread before the body was ever looked at: a
	// look is then queued for the main thread). On the main thread the body
	// is looked at again when anything worn or the 3D changed.
	std::optional<bool> FromBody(RE::Actor* a_actor);

	// Forget everyone (a new game or a load).
	void Clear();
}
