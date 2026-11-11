#pragma once

#include <optional>
#include <string>
#include <thread>
#include <vector>

// Who has a penis, from the body (roadmap 2b.7). OStim asks Schlongs of
// Skyrim's or TNG's factions when no actor property decides
// (CompatibilityTable::hasSchlong, its useSoSSex / useTNGSex). Fallout 4
// has neither, so 4Stim looks at what the actor wears, their naked body
// included: any mesh skinned to ZeX's Penis_* bones, or named like a penis
// (sPenisMeshNames: older mods' meshes, from before ZeX's bones, are skinned
// to the pelvis), means a penis. 4Stim's own equip objects (the strap-on,
// the tongues) don't count; a wearable penis from another mod does
// (Fallout 4's futa), and Undress leaves it on (GivesPenis).
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

	// sPenisMeshNames: parts of a mesh's name that make it a penis, lowercase.
	void SetMeshNames(std::vector<std::string> a_names);

	// a_item, worn by a_actor, is what gives them a penis (Undress keeps it
	// on). Looked at again first when on the main thread.
	bool GivesPenis(RE::Actor* a_actor, std::uint32_t a_item);

	// Forget everyone (a new game or a load).
	void Clear();
}
