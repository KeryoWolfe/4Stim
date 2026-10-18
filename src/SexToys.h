#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SceneRegistry.h"

namespace FourStimAPI
{
	class SexToyInterface;
}

// Sex toys (docs/API.md), as OStim NG's SexToys: device plugins register toys
// through the "SexToys" plugin interface (include/4Stim/PluginInterface.h);
// 4Stim drives them from the player's scene. Each action side's "toySlot"
// (OStim's field: "genital", "anus"...) is where a toy goes; a toy is given
// to the first slot it's allowed on (sToySynchronization), and gets a
// baseline and peak strength from the scaling settings, an update on each of
// the action's peaks (or every second without peaks), and a strong peak when
// its actor climaxes.
//
// Departure from OStim: OStim keeps per-toy and per-slot settings in the
// save, edited in its own toy menu; 4Stim has one set in 4Stim.ini /
// the MCM ([SexToys]), OStim's global ones. OStim's defaults ("individual"
// at every level) work out to: any actor's slot, no scaling (the max
// strength), peaks and climax on; 4Stim's defaults are the same.
namespace SexToys
{
	struct Config
	{
		bool  enabled = true;           // bToysEnabled (OStim: the global slot's "enabled")
		int   synchronization = 3;      // sToySynchronization: 0 player, 1 male, 2 female, 3 scene (anyone)
		int   scaling = 0;              // sToyScaling: 0 none (max), 1 rate (stimulation rate), 2 value (stimulation value), 3 speed
		float minScale = 0.0F;          // fToyMinScale
		float maxScale = 1.0F;          // fToyMaxScale
		float minMagnitude = 0.1F;      // fToyMinMagnitude
		float maxMagnitude = 0.5F;      // fToyMaxMagnitude
		bool  peaks = true;             // bToyPeaks
		float minPeak = 0.6F;           // fToyMinPeak
		float maxPeak = 1.0F;           // fToyMaxPeak
		bool  climax = true;            // bToyClimax
		float climaxMagnitude = 1.0F;   // fToyClimaxMagnitude
	};
	Config& Settings();

	// A 4Stim.ini / MCM setting of this module (key lowercase). False if the
	// key isn't one of them.
	bool ReadSetting(const std::string& a_key, const std::string& a_value);

	FourStimAPI::SexToyInterface* Interface();

	// The device plugins register their toys again (a save loaded, the
	// player's scene started), on a detached thread.
	void Reload();

	// The player's scene plays a_scene (a_actors in role order) at a_speed:
	// toys are given to its slots anew when the scene changed. Main thread.
	void Enter(const std::vector<std::uint32_t>& a_actors, const std::shared_ptr<const SceneRegistry::Scene>& a_scene, int a_speed);
	// A peak of action a_action of the scene a_actors play (Peaks.h).
	void Peak(const std::vector<std::uint32_t>& a_actors, std::size_t a_action);
	void Climax(std::uint32_t a_actor);
	void Tick(float a_seconds);
	// The player's scene ended (any of these actors' scene).
	void Leave(const std::vector<std::uint32_t>& a_actors);
	void Clear();
}
