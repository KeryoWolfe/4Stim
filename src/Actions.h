#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "SceneRegistry.h"

// Action types: what a scene's actors do to each other (vaginal sex,
// kissing, a handjob...), as OStim's actions. Defined in JSON files under
// Data\F4SE\Plugins\4Stim\Actions\ (see docs/ACTIONS.md); scenes list the
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
	};

	struct Type
	{
		std::string              id;    // lowercase
		std::string              name;  // display name
		std::vector<std::string> tags;  // lowercase: sexual, oral, intercourse...
		Side                     actor;      // who does it ("the one with the penis" in vaginal sex)
		Side                     target;     // who it's done to
		Side                     performer;  // who's moving (cowgirl: the target)
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
