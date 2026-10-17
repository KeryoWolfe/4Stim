#pragma once

#include <cstdint>
#include <string>
#include <vector>

// OStim's eye directions (scene actors' lookUp / lookDown / lookLeft /
// lookRight, docs/EXPRESSIONS.md): where each role looks, on top of the
// face. OStim sets Skyrim's look modifiers (8-11); Fallout 4's face has no
// such morphs, so 4Stim turns the face skeleton's eye bones
// (skin_bone_L_Eye / skin_bone_R_Eye) instead: 100 is sLookMaxSide /
// fLookMaxUp degrees, turned about the actor's up and side axes.
namespace EyeLook
{
	struct Config
	{
		bool        disabled = false;
		float       maxSide = 25.0F;  // fLookMaxSide: degrees at 100 left / right
		float       maxUp = 18.0F;    // fLookMaxUp: degrees at 100 up / down
		std::string leftEye = "skin_bone_L_Eye";
		std::string rightEye = "skin_bone_R_Eye";
	};
	Config& Settings();

	// a_id looks a_up (+ up, - down) and a_left (+ left, - right), 0-100
	// as OStim's values; both 0: their eyes are their own again. Any thread.
	void Set(std::uint32_t a_id, float a_up, float a_left);

	// Keeps the eyes turned. Main thread (the scene clock).
	void Tick();

	// The actors left their scene: their eyes back.
	void Leave(const std::vector<std::uint32_t>& a_ids);

	// A game is loading: forget everyone.
	void Clear();
}
