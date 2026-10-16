#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SceneRegistry.h"

// Actor properties (docs/ACTORS.md), as OStim's: files in
// Data\F4SE\Plugins\4Stim\Actor Properties\ that each say, for the actors a
// condition holds for, their actor type, what they have for scene roles'
// requirements, equip objects they wear in scenes, an expression set, and
// whether they're muted or muffled. Where several files set the same thing
// for an actor, the one with the highest priority wins.
//
// OStim's conditions are perks in OStim.esp, whose condition functions are
// borrowed. 4Stim has no plugin, so its conditions are written in the file
// (sex, races, keywords, factions, worn items...); a mod with its own plugin
// can still point at a perk, as in OStim.
namespace ActorProperties
{
	// OStim's settings for actors with a penis who aren't men (futa: in
	// Fallout 4, wearing an equippable penis that an actor property gives
	// "sex": "male").
	struct Config
	{
		bool futaUseMaleRole = true;         // OStim's futaUseMaleRole: they can take men's roles too
		bool futaUseMaleExcitement = false;  // OStim's futaUseMaleExcitement: the male excitement rate
		bool futaUseMaleClimax = true;       // OStim's futaUseMaleClimax: their climax counts as a man's (ending scenes)
	};
	Config& Settings();

	// Re-reads the files.
	void Reload();

	// a_actor's actor type (OStim's): "npc" for people, others for actors
	// who play only scenes made for them. "" = none: not in scenes at all.
	std::string Type(RE::Actor* a_actor);

	// Whether a_actor has a penis (OStim's hasSchlong): an actor property's
	// "sex" if one sets it, else whether they're a man.
	bool HasPenis(RE::Actor* a_actor);

	// What a_actor brings to a role (OStim's ActorCondition::create).
	SceneRegistry::ActorCondition Condition(RE::Actor* a_actor);

	// The rest of their properties.
	std::string              Expression(RE::Actor* a_actor);  // expression set for the whole scene, "" = none
	bool                     Muted(RE::Actor* a_actor);
	bool                     Muffled(RE::Actor* a_actor);
	std::vector<std::string> EquipObjects(RE::Actor* a_actor);  // equip object types worn from the start

	// The excitement rate and climax of a man (OStim's futa settings).
	bool MaleExcitement(RE::Actor* a_actor);
	bool MaleClimax(RE::Actor* a_actor);

	// Conditions are worked out on the main thread and kept, for the
	// picker's lists (read on the interface's thread): Remember works them
	// out for a_actor now, Cached gives what was worked out (or works it out
	// if nothing was).
	void                          Remember(RE::Actor* a_actor);
	SceneRegistry::ActorCondition Cached(std::uint32_t a_id);
}
