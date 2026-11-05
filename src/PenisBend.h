#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Penis bend (docs/ALIGNMENT.md), as OStim's sosBend: each role of a scene
// can have the penis bent up or down, in Schlongs of Skyrim's steps (-9 to
// +9, and -10 for flaccid: Erection.h), from the scene file (penisBend, or OStim's sosBend / tngBend), the
// Align tab, and the animation's 4StimBendSoS annotations (OStim's
// OStimBendSoS, an offset on top).
//
// OStim plays SoS's bend animations; TNG answers them with its own poses
// (auxbones\TNG: TngBendUp / TngBendDown, blended with TngErect by the step),
// which curve the whole shaft: up, Genitals01 43 degrees, 02 9, 03 9, 04 4;
// down, 01 53, 02 3, 04 3.5, 05 4 (read from TNG's clips, 1 November 2026).
// Fallout 4 has neither: 4Stim turns ZeX's segment pivots the same way,
// Penis_01_Offset to Penis_05_Offset (the shaft from its base onward; ZeX's
// _Offset nodes aren't animated, so the animation doesn't undo them, and
// A-Body's SMP genitals follow the skeleton's penis bones), each its share
// of the bend (TNG's). Each step is fPenisBendStep degrees in all. Step n
// is n/9 of the full pose, as in SoS's and TNG's behavior graphs (their bend
// clips are additive, blended at weights 0.111 to 1.0; checked 1 November
// 2026). SoS's own poses, for comparison: up 50 then 5 on each of
// Genitals02-06, down 55 then 5 each, flaccid 60/10/5/5/5.
namespace PenisBend
{
	struct Config
	{
		bool        disabled = false;            // OStim's SetSchlongBending: 1 = no bending
		float       stepDegrees = 7.2F;          // 4Stim: fPenisBendStep, the whole bend per step (TNG's full bend, about 65 degrees, over 9 steps; negative bends the other way)
		// 4Stim: sPenisBendNode, the nodes turned, base first (a comma list);
		// sPenisBendUp / sPenisBendDown, each node's share of the bend up /
		// down (TNG's; a missing share is 0, one node alone takes it all).
		std::vector<std::string> nodes{ "Penis_01_Offset", "Penis_02_Offset", "Penis_03_Offset", "Penis_04_Offset", "Penis_05_Offset" };
		std::vector<float>       shareUp{ 0.657F, 0.139F, 0.136F, 0.067F, 0.0F };
		std::vector<float>       shareDown{ 0.834F, 0.049F, 0.0F, 0.055F, 0.062F };
		// 4Stim: sPenisFlaccid, degrees down for each node at a bend of -10 on
		// a body that can't go flaccid itself (A-Body): TNG's TngFlaccid pose.
		std::vector<float>       flaccid{ 50.1F, 15.1F, 0.0F, 0.0F, 0.0F };
		int                      axis = 2;       // 4Stim: iPenisBendAxis, the nodes' own x (0), y (1) or z (2)
	};
	Config& Settings();

	// a_id's bend in the scene now (OStim's setSoSBend: the alignment's, or
	// the scene's). a_fromAlignment: the bend is the player's own, from the
	// Align tab, which counts even on an aimed animation (Aim). Any thread.
	void Set(std::uint32_t a_id, float a_steps, bool a_fromAlignment = false);

	// The 4StimPenisAim annotation: the actor's animation already points the
	// penis at the partner (AnimBridge bends the whole Penis_00..05 chain toward
	// the partner's vagina, anus or mouth when it converts a pack), so the scene
	// file's bend would only bend it off target: it's left out until the scene
	// changes. A bend from the Align tab and 4StimBendSoS annotations still
	// count. Any thread.
	void Aim(std::uint32_t a_id, bool a_aimed);

	// An annotation's offset on top (OStim's offsetSoSBend; 0 = none).
	void Offset(std::uint32_t a_id, int a_steps);

	// Keeps the nodes turned. Main thread (the scene clock).
	void Tick();

	// The actors left their scene: the nodes back as they were.
	void Leave(const std::vector<std::uint32_t>& a_ids);

	// ZeX's genital bones (Penis*, Vagina*, Anus*, Clit*): their pose before
	// the actor's first scene this game is noted, and put back when a scene
	// ends (the game's behaviors never animate them, so a scene's last pose
	// stayed). Main thread.
	void NoteRestPose(const std::vector<std::uint32_t>& a_ids);
	void RestoreRestPose(const std::vector<std::uint32_t>& a_ids);

	// A game is loading: forget everyone.
	void Clear();
}
