#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "SceneRegistry.h"
#include "Sounds.h"

// Action types: what a scene's actors do to each other (vaginal sex,
// kissing, a handjob...), as OStim's actions. Defined in JSON files under
// Data\F4SE\Plugins\4Stim\Actions\ (see docs/ACTIONS.md), in 4Stim's layout
// (an "actions" list) or OStim's (one action per file, id = file name,
// Skyrim "strippingSlots" mapped to Fallout 4's slots); scenes list the
// actions they show with who does what ("actions" in a scene file,
// SCENES.md). Excitement, undressing, sounds and expressions build on these.
namespace Actions
{
	// What an action means for one of its sides.
	struct Side
	{
		float                    stimulation = 0.0F;       // per second, for this side's actor
		float                    maxStimulation = 100.0F;  // no stimulation from this action past this
		std::vector<std::string> requirements;             // body parts this side needs (lowercase): penis, vagina, mouth...
		bool                     fullStrip = false;        // undress this side's actor completely (docs/UNDRESS.md)
		std::vector<int>         undressSlots;             // else take off what's in these biped slots (30-61)
		bool                     moan = false;             // this side's actor moans (docs/SOUND.md)
		bool                     talk = false;             // and may talk (kept for dialogue, not used yet)
		bool                     muffled = false;          // their mouth is busy: muffled sounds
		std::string              expressionOverride;       // facial expression set while doing it ("openmouth", "tongue"...)
	};

	// How an action's peaks (its strokes, thrusts, slaps) are found, as
	// OStim's "peak": a timer ("loop") or two sets of bones moving closest
	// together ("bonedistance"; furthest apart with "inverse").
	struct Peak
	{
		enum class Kind
		{
			kNone,
			kLoop,
			kBoneDistance
		};
		Kind                     kind = Kind::kNone;
		float                    interval = 1.0F;  // loop: seconds between peaks (OStim: ms), divided by the speed's displaySpeed
		bool                     inverse = false;
		std::vector<std::string> actorBones;   // bonedistance: the action actor's bones
		std::vector<std::string> targetBones;  // and the target's
	};

	// A sound an action plays, as OStim's "sounds" entries: on each peak
	// ("peak"), when bones meet ("bonedistance"), or over and over ("loop").
	struct ActionSound
	{
		enum class Kind
		{
			kPeak,
			kBoneDistance,
			kLoop
		};
		Kind                     kind = Kind::kPeak;
		Sounds::Sound            sound;
		bool                     muteWithActor = false;
		bool                     muteWithTarget = false;
		bool                     playerThreadOnly = false;
		bool                     inverse = false;          // bonedistance
		float                    minInterval = 0.05F;      // bonedistance: seconds (OStim: ms, default 50)
		float                    maxInterval = 0.0F;       // bonedistance: 0 = none
		float                    delay = 0.0F;             // loop: seconds after one ends before the next
		std::vector<std::string> actorBones;
		std::vector<std::string> targetBones;
	};

	struct Type
	{
		std::string              id;    // lowercase
		std::string              name;  // display name
		std::vector<std::string> tags;  // lowercase: sexual, oral, intercourse...
		Side                     actor;      // who does it ("the one with the penis" in vaginal sex)
		Side                     target;     // who it's done to
		Side                     performer;  // who's moving (cowgirl: the target)
		Peak                     peak;       // OStim's "peak" (docs/EVENTS.md)
		std::vector<ActionSound> sounds;     // OStim's "sounds" (docs/SOUND.md)
		std::string              sourceFile;

		[[nodiscard]] bool HasTag(std::string_view a_tag) const;
	};

	// Re-reads the action files. Called by SceneRegistry before the scenes
	// (scenes name their actions).
	void Reload();

	// An action type by its id or one of its aliases (case-insensitive);
	// nullptr if there's none. Stays valid while held, across a Reload.
	std::shared_ptr<const Type> Find(std::string_view a_idOrAlias);

	// Every action type id, sorted.
	std::vector<std::string> List();

	// Whether an actor of sex a_sex has what a_requirement asks for. Until
	// strap-ons: a penis and testicles for men, a vagina and breasts for
	// women; hands, mouth, feet, nipples and anus for everyone. Anything unknown
	// (tail, vampire...) no one has yet. An actor of unknown sex has it all.
	bool Provides(SceneRegistry::Sex a_sex, std::string_view a_requirement);
}
