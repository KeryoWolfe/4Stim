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
// OStim plays SoS's bend animations. Fallout 4 has none: 4Stim turns a node
// of the actor's skeleton instead, by default ZeX's Penis_01_Offset (the
// shaft from its base; ZeX's _Offset nodes aren't animated, so the
// animation doesn't undo it, and A-Body's SMP genitals follow the skeleton's
// penis bones). Each step is fPenisBendStep degrees around the node's axis.
namespace PenisBend
{
	struct Config
	{
		bool        disabled = false;            // OStim's SetSchlongBending: 1 = no bending
		float       stepDegrees = 5.0F;          // 4Stim: fPenisBendStep (negative bends the other way)
		std::string node = "Penis_01_Offset";    // 4Stim: sPenisBendNode
		int         axis = 2;                    // 4Stim: iPenisBendAxis, the node's own x (0), y (1) or z (2)
	};
	Config& Settings();

	// a_id's bend in the scene now (OStim's setSoSBend: the alignment's, or
	// the scene's). Any thread.
	void Set(std::uint32_t a_id, float a_steps);

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
