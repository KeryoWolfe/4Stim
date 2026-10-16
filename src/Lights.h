#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Lights on actors during scenes (docs/LIGHTS.md), as OStim's light equip
// objects: when a scene starts (only in the dark with SetOnlyLightInDark),
// each actor gets the light chosen for their sex (sMaleLight / sFemaleLight:
// a file in Data\F4SE\Plugins\4Stim\Lights\, OStim's ids: facelight,
// facelightdim, crotchlight, crotchlightdim, asslight, asslightdim, nolight).
//
// OStim's lights are armor items with a light in their mesh. 4Stim has no
// plugin, so a light here is a game light (LIGH) placed by the actor and kept
// at one of their bones (the head, the pelvis) every frame, and removed when
// the scene ends and before the game saves.
namespace Lights
{
	struct Config
	{
		bool        onlyInDark = false;          // SetOnlyLightInDark (OStim)
		float       darkLevel = 20.0F;           // 4Stim: fLightLevelDark, OStim's fixed 20
		std::string maleLight = "nolight";       // 4Stim: sMaleLight (OStim: the MCM's equip object)
		std::string femaleLight = "nolight";     // 4Stim: sFemaleLight
	};
	Config& Settings();

	// Re-reads the light files.
	void Reload();

	// A scene with a_actors started (or changed): lights the actors who
	// have none yet (OStim lights them once, at the start). Main thread.
	void Enter(const std::vector<std::uint32_t>& a_actors);

	// Keeps the lights at their bones. Main thread (the scene clock).
	void Tick();

	// The actors left their scene: their lights go.
	void Leave(const std::vector<std::uint32_t>& a_actors);

	// Removes every light placed (before a save; the next Tick puts them
	// back) or forgets them (a game loaded).
	void RemoveAll();
	void Clear();
}
