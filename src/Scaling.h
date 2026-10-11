#pragma once

#include <string>

// Actor scaling and heels (docs/SCALING.md), as OStim's ThreadActor::scale
// and heel offsets:
//
// - Scaling (OStim's SetScaling, on unless set to 1): every actor in a scene
//   is scaled to the height the animation was made for: the role's "scale"
//   in the scene file (1 by default) times their alignment's scale, divided
//   by their own NPC height, so a tall NPC and a short one both play the
//   animation at its size.
// - Heels: an actor wearing heels (a height from Fallout 4's High Heels
//   System .txt files, "Height=" next to the shoe's mesh) is scaled down so
//   heels plus body match the animation (OStim's scaleHeight / (scaleHeight +
//   heels)) when their feet are on the ground (role tag standing / squatting,
//   or "feetOnGround"); otherwise the heels are taken off their height: they
//   are lowered by it (OStim moves the skeleton down by the offset).
namespace Scaling
{
	struct Config
	{
		bool disabled = false;  // SetScaling=1: no scaling (actors keep their own scale, times their alignment's)
	};
	Config& Settings();

	// a_actor's own height factor (their NPC record's height, 1 = normal).
	float NPCHeight(RE::Actor* a_actor);

	// How high a_actor's heels lift them, in game units (0 = none): the
	// highest High Heels System height among what they wear. Main thread.
	float HeelHeight(RE::Actor* a_actor);

	// Forgets the heel heights read from files (FourStim.ReloadScenes).
	void Reload();
}
