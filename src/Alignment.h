#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "SceneRegistry.h"

// Alignment (docs/ALIGNMENT.md), as OStim's: per set of actors, scene and
// role, how far an actor is moved from the scene's spot (sideways, forward,
// up), turned, and scaled, on top of where the animation puts them. Set in
// game from the HUD's Align tab and saved to
// Documents\My Games\Fallout4\4Stim\alignment.json, in OStim's layout:
//
//   { "<actor set key>": { "<scene id>": { "<role>": { "offsetX", "offsetY",
//     "offsetZ", "rotation", "scale", "sosBend" } } } }
//
// The actor set key is OStim's: one "<sex><height>x<heels>" per role, joined
// by "&" (e.g. "M100x0&F100x0"). Sex is M / F (or N when not grouped by sex),
// height the actor's scale x 100 (100 when not grouped by height), heels the
// heel height x 100 (0 when not grouped by heels; Fallout 4 has no standard
// heels system, so it's always 0 for now). The scene lock (main.cpp) holds
// each actor at the scene's spot plus their offset.
namespace Alignment
{
	struct Offset
	{
		float x = 0.0F;      // offsetX: to the actor's right, game units
		float y = 0.0F;      // offsetY: forward
		float z = 0.0F;      // offsetZ: up
		float rot = 0.0F;    // rotation: degrees, clockwise seen from above
		float scale = 1.0F;  // times the actor's own scale
		float bend = 0.0F;   // sosBend (penisBend in 4Stim): kept, not used yet

		[[nodiscard]] bool IsZero() const { return x == 0.0F && y == 0.0F && z == 0.0F && rot == 0.0F && scale == 1.0F && bend == 0.0F; }
	};

	// OStim's alignment grouping settings (4Stim.ini).
	struct Config
	{
		bool groupBySex = true;      // alignmentGroupBySex
		bool groupByHeight = false;  // alignmentGroupByHeight
		bool groupByHeels = true;    // alignmentGroupByHeels
	};
	Config& Settings();

	// One actor of a scene, for the key.
	struct ActorInfo
	{
		SceneRegistry::Sex sex = SceneRegistry::Sex::kAny;
		float              scale = 1.0F;  // their own scale, before alignment
		float              heels = 0.0F;  // heel height
	};

	// The actor set key for these actors in role order (OStim's ThreadKey).
	std::string KeyFor(const std::vector<ActorInfo>& a_actors);

	// The offset for role a_role of a_sceneID with actor set a_key (all zero
	// if none was set). Case-insensitive. Any thread.
	Offset Get(std::string_view a_key, std::string_view a_sceneID, std::size_t a_role);

	// Sets it and saves the file. Any thread.
	void Set(std::string_view a_key, std::string_view a_sceneID, std::size_t a_role, const Offset& a_offset);

	// Re-reads the file (FourStim.ReloadScenes).
	void Reload();
}
