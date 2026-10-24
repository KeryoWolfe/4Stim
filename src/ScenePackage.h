#pragma once

#include <cstdint>
#include <vector>

// The scene package (docs/ACTORS.md, "NPCs in a scene"), as OStim's: an NPC in
// a scene does nothing but the scene. OStim hooks the game's alias-package
// check (GameHooks.h PackageStart: Skyrim's Actor function at
// RELOCATION_ID(36404, 37398) + 0x47, its tail jump to the extra data's alias
// package) and, for an actor in a thread, returns OStim.esp's
// OStimScenePackage (a "Wait" with StopMovement) instead. An alias package
// comes before the NPC's own packages, so no sandbox, work spot, settler
// job, follow or dialogue package starts while the scene runs.
//
// Fallout 4's check is TESObjectREFR::CheckForCurrentAliasPackage (vtable
// slot 0x5B; Actor's is ID 2229802, whose tail jump at +0x39 is the same as
// Skyrim's): the Actor package chooser (ID 2229562) asks it before the NPC's
// package list, and starting dialogue or follow packages (ID 2229809) asks it
// too. 4Stim hooks that slot on Actor's vtable, which 4Stim already finds on
// every game version (playbackSpeed), rather than patching the jump: no
// new address for 1.10.163. The player has their own vtable and isn't
// touched, as in OStim.
//
// The package: 4Stim has no plugin file for one, so it's made the way the
// game makes its own do-nothing package (Actor::InitiateDoNothingPackage):
// a package of procedure type 0x1B, must complete. Where those two functions
// aren't found (1.10.163 without them in the Runtime Database), Fallout4.esm's
// MQ101VaultTecRepDoNothingScene (a "Wait" with StopMovement, as OStim's).
namespace ScenePackage
{
	// Hooks Actor's alias-package check. At plugin load.
	void Install();

	// Makes or finds the package. Main thread, once forms are loaded
	// (kGameLoaded).
	void Prepare();

	// a_id does nothing but the scene from now on (call EvaluatePackage
	// after, for them to switch now). Any thread.
	void Add(std::uint32_t a_id);

	// a_ids are free of it (call before handing them back their routine).
	void Remove(const std::vector<std::uint32_t>& a_ids);

	// Drops anyone not in a_inScenes who was added more than a few seconds
	// ago (a scene that never started, or ended some way that didn't call
	// Remove), so no NPC is ever left doing nothing; returns them (call
	// EvaluatePackage on them). Main thread, often.
	std::vector<std::uint32_t> Prune(const std::vector<std::uint32_t>& a_inScenes);

	void Clear();
}
