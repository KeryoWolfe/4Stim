#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace RE
{
	class TESIdleForm;
}

// 4Stim's annotation cues, timed by 4Stim itself. Fallout 4 doesn't fire an
// animation's own text annotations (4StimEvent.spankright,1, 4StimClimax,
// 4StimPeak.0, 4StimPenisAim...) as animation events for idles played the
// way scenes play them: the owner's log (5 November) had only the behavior
// graph's own events (UncullWeapons, HeadTrackingOn) across five spanks,
// never the spank annotation 0.42 s into the clip, so its slap was never
// heard. OStim gets them from Skyrim as animation events (its
// AnimationEventHandler); here 4Stim reads them from the idle's .hkx when
// the idle plays (packfile, hk_2014: the annotation tracks and the clip's
// duration), and fires each at its time on the scene clock, again on every
// loop of a looping clip, at the actor's playback speed. They go through the
// same handler as animation events. If the game ever does send one itself,
// the timing stands down (EngineForwarded), so nothing fires twice.
// Main thread, except EngineForwarded.
namespace AnimCues
{
	struct Due
	{
		std::uint32_t actor = 0;
		std::string   tag;      // before the first '.', as the game splits annotations
		std::string   payload;  // after it
	};

	// a_actor started a_idle's clip now. a_loops: it repeats (a scene's
	// loop) rather than playing once (a transition). Replaces what they had.
	void Played(std::uint32_t a_actor, const RE::TESIdleForm* a_idle, bool a_loops);

	// They left their scene (or played something that isn't a scene's).
	void Stop(const std::vector<std::uint32_t>& a_actors);
	void Clear();

	// a_seconds of scene clock passed (paused in menus, slowed in slow
	// motion); a_rate(actor) is their animation playback rate. The cues now due.
	std::vector<Due> Tick(float a_seconds, const std::function<float(std::uint32_t)>& a_rate);

	// The game sent a 4Stim annotation as an animation event itself. Any thread.
	void EngineForwarded(const char* a_tag);
}
