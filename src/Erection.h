#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Erections (docs/ERECTION.md). OStim tells Schlongs of Skyrim to erect a
// scene actor's penis (its SOSBend<n> events, one a step of the penis bend)
// and to go flaccid at a bend of -10 and when the actor leaves the scene
// (SOSFlaccid). Fallout 4 has no Schlongs of Skyrim: each male body does it
// its own way, so 4Stim works out per actor which one they have:
//
// - a body whose .tri has an "Erection" morph (BodyTalk, Atomic Muscle's
//   .tri install): LooksMenu's BodyGen morphs (FourStimErection.psc);
// - an SMP body 4Stim swaps to its rigid physics file (Physics.h, A-Body):
//   nothing. A-Body's penis is rigid in its mesh; its own physics file is
//   what makes it hang, and the swap turns that off. It only gets the bend;
// - a body that is a separate erect bodysuit armor (Atomic Muscle's
//   bodysuit install): the bodysuit goes on while they're naked;
// - anything else: the penis bend only.
//
// The bodies and morph names are in Data\F4SE\Plugins\4Stim\Erection\.
namespace Erection
{
	struct Config
	{
		bool enabled = true;  // 4Stim: bErections (OStim has no setting: SoS does it)
	};
	Config& Settings();

	// Re-reads the files.
	void Reload();

	// a_ids started a scene: those with a penis get erect. Main thread.
	void SceneStarted(const std::vector<std::uint32_t>& a_ids);

	// The penis bend in steps (PenisBend.h, after the annotations' offset):
	// -10 is flaccid, as OStim's SOSFlaccid; the rest, erect. Any thread.
	void SetBend(std::uint32_t a_id, int a_steps);

	// Whether a_id's body can go flaccid (morphs, a bodysuit). Those that
	// can't (A-Body, other bodies) get -10 as the furthest bend down.
	[[nodiscard]] bool CanGoFlaccid(std::uint32_t a_id);

	// Works out each actor's body, keeps the bodysuits on while they're
	// naked and the morphs set. Main thread (the scene clock).
	void Tick();

	// The actors left their scene: flaccid again, bodysuits off and gone,
	// morphs gone. Main thread.
	void Leave(const std::vector<std::uint32_t>& a_ids);

	// A bodysuit armor (undressing leaves those alone).
	[[nodiscard]] bool IsErectionItem(std::uint32_t a_formID);

	// The bodysuit 4Stim gave a_id for their scene (0: none), for the save.
	[[nodiscard]] std::uint32_t Given(std::uint32_t a_id);

	// A save made mid-scene was loaded: what 4Stim put on a_id for that
	// scene goes (the morphs, a_given). The scene, if it starts again, does
	// it again. Main thread.
	void AfterLoad(std::uint32_t a_id, std::uint32_t a_given);

	// A game is loading: forget everyone.
	void Clear();
}
