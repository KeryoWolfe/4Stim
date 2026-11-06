#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

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
	// OStim's API forms on an action side (docs/API.md, "Factions, form
	// lists and keywords"): form IDs of factions and form lists, 4Stim.esl's
	// for OStim's own (ApiForms.h fills them in during scenes).
	struct ApiFormRefs
	{
		std::vector<std::uint32_t> factions;                        // "faction": in it while doing the action
		std::vector<std::uint32_t> statFactions;                    // "statFaction": +1 per scene they did it in
		std::vector<std::uint32_t> playerStatFactions;              // "playerStatFaction": the same, with the player
		std::vector<std::uint32_t> climaxStatFactions;              // "climaxStatFaction": +1 per climax while doing it
		std::vector<std::uint32_t> partnerClimaxStatFactions;       // "partnerClimaxStatFaction": +1 per scene the partner climaxed at it
		std::vector<std::uint32_t> playerClimaxStatFactions;        // "playerClimaxStatFaction": climaxed at it with the player
		std::vector<std::uint32_t> playerPartnerClimaxStatFactions; // "playerPartnerClimaxStatFaction": the player climaxed at it with them
		std::vector<std::uint32_t> playerStatLists;                 // "playerStatList": did it with the player
		std::vector<std::uint32_t> playerClimaxStatLists;
		std::vector<std::uint32_t> playerPartnerClimaxStatLists;

		void Merge(const ApiFormRefs& a_other);
		[[nodiscard]] bool Empty() const;
	};

	// {"mod": "...", "formid": "0x..."} or a list of them: their loaded form
	// IDs. OStim's "OStim.esp" means 4Stim.esl (the same form IDs,
	// tools/make-api-plugin.py). Forms that aren't loaded are left out.
	std::vector<std::uint32_t> ReadForms(const nlohmann::json& a_value);

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
		std::vector<std::string> equipObjects;             // OStim's "equipObject": equip object types this side's actor wears ("tongue"...), lowercase
		std::vector<std::string> toySlots;                 // OStim's "toySlot": sex toy slots this side drives ("genital", "anus"...), lowercase (SexToys.h)
		ApiFormRefs              forms;                    // OStim's factions and stat forms, with its action tags' (Action Tags\)
		// OStim's custom data for this side ("ints", "floats", "strings",
		// "intLists", "floatLists", "stringLists"): for other mods, through
		// FourMetadata's GetCustomAction... functions. Keys lowercase, as are
		// string values.
		std::map<std::string, int>                      ints;
		std::map<std::string, float>                    floats;
		std::map<std::string, std::string>              strings;
		std::map<std::string, std::vector<int>>         intLists;
		std::map<std::string, std::vector<float>>       floatLists;
		std::map<std::string, std::vector<std::string>> stringLists;
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
		float                    minRange = 3.0F;  // 4Stim: bonedistance: the bones must move at least this far (units)
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
		float                    minRange = 3.0F;          // 4Stim: bonedistance, as Peak's
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

	// Find, and for an id no Actions file defines, OStim's fallback: a type
	// with the "default" action's attributes under that id (logged once per
	// id and reload). Never null. For scenes: an unknown action is kept, so
	// action indexes match the file (4StimPeak.N, expressionAction, the API).
	std::shared_ptr<const Type> FindOrDefault(std::string_view a_idOrAlias);

	// OStim's GraphTable::getActionAlias: the id a_idOrAlias stands for, or
	// a_idOrAlias itself (lowercase) if it's no action's id or alias. For
	// matching a scene's actions by type, unknown types included.
	std::string ResolveAlias(std::string_view a_idOrAlias);

	// Every action type id, sorted.
	std::vector<std::string> List();

}
