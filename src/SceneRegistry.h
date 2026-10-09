#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Actions
{
	struct Type;
}

// Scenes are defined in JSON files under Data\F4SE\Plugins\4Stim\Scenes\,
// so animation authors add content with an Idle-record plugin plus a JSON
// file, no scripting. See SCENES.md for the format.
namespace SceneRegistry
{
	enum class Sex : std::uint8_t
	{
		kAny,  // a role open to anyone; for an actor, not known
		kMale,
		kFemale
	};

	// An actor's sex as the scenes see it.
	Sex SexOf(RE::Actor* a_actor);  // GetSex() isn't const

	struct SceneActor
	{
		Sex                      sex = Sex::kAny;  // who may take this role
		std::vector<std::string> requirements;     // what the role needs (penis, mouth...): its own and its actions', lowercase
		std::vector<std::string> tags;             // what the role is doing: standing, kneeling, lyingback... lowercase
		// Scenes to move to on an event for this role's actor (OStim's
		// autoTransitions): "climax", "pullout"... key lowercase.
		std::vector<std::pair<std::string, std::string>> autoTransitions;

		[[nodiscard]] bool HasTag(std::string_view a_tag) const;
		// The scene for a_event, or "".
		[[nodiscard]] std::string AutoTransition(std::string_view a_event) const;
	};

	// One action in a scene: who does what to whom (docs/ACTIONS.md).
	struct SceneAction
	{
		std::shared_ptr<const Actions::Type> type;  // never null
		std::size_t                          actor = 0;      // role doing it
		std::size_t                          target = 0;     // role it's done to (= actor: done to themselves)
		std::size_t                          performer = 0;  // role moving
	};

	struct Navigation
	{
		std::string to;     // destination scene id
		std::string label;  // may contain {n} = name of the actor in role n
		std::string icon;   // HUD icon movie under Data\Interface\4Stim\Icons\; empty = the destination's
	};

	struct Scene
	{
		std::string                                id;
		std::string                                name;
		std::vector<SceneActor>                    actors;       // index = role
		std::vector<std::vector<RE::TESIdleForm*>> speeds;       // [speed][role]; at least one speed
		std::vector<Navigation>                    navigations;  // only to scenes that exist
		std::vector<std::string>                   tags;
		std::vector<SceneAction>                   actions;
		std::string                                icon;  // HUD icon under Data\Interface\4Stim\Icons\, or empty
		float                                      length = 0.0F;  // seconds of one play-through; 0 = not given
		std::string                                destination;    // transition: the scene it moves on to after `length`
		int                                        defaultSpeed = 0;   // 0-based speed it starts at (auto mode, new scenes)
		bool                                       noRandomSelection = false;  // auto mode never picks it
		bool                                       noStrip = false;            // no undressing in this scene
		std::string                                furniture;      // furniture type it's played on (lowercase); "" = anywhere
		std::array<float, 4>                       furnitureOffset{};  // x, y, z, degrees from the furniture's spot
		std::string                                sourceFile;

		bool IsTransition() const { return !destination.empty(); }

		// Whether one of its actions is of a_type (an id or alias) / has a_tag.
		bool HasAction(std::string_view a_type) const;
		bool HasActionTag(std::string_view a_tag) const;
	};

	// A fixed run of scenes played one after another (OStim's sequences).
	struct SequenceEntry
	{
		std::string scene;     // scene id
		float       duration;  // seconds in this scene before the next one
		int         speed;     // 0-based speed to play it at
	};

	struct Sequence
	{
		std::string                id;
		std::string                name;
		std::size_t                actorCount = 0;
		std::vector<SequenceEntry> entries;  // at least one
		std::vector<std::string>   tags;
		std::string                sourceFile;
		std::vector<SceneActor>    actors;     // per role, what its scenes ask for
		std::string                furniture;  // what its scenes are played on (all the same)
	};

	// Which actor takes each role of a scene (or sequence) whose roles ask for
	// a_roles: for each role, an index into a_actors. Empty if they can't fill
	// it (wrong count, or a role's sex doesn't match). Keeps a_actors' own
	// order when it fits, otherwise tries the other order (two actors).
	std::vector<std::size_t> AssignRoles(std::span<const SceneActor> a_roles, std::span<const Sex> a_actors);

	// Whether a_actors, in this role order, fit a_roles: each role's sex,
	// and what its actions need (Actions::Provides).
	bool Fits(std::span<const SceneActor> a_roles, std::span<const Sex> a_actors);

	// Re-reads every scene file. Returns the number of scenes loaded.
	int Reload();

	// Case-insensitive lookup. Loads the scene files on first use.
	// Returns nullptr if the scene doesn't exist. The scene stays valid for
	// as long as the pointer is held, even across a Reload.
	std::shared_ptr<const Scene> Find(std::string_view a_id);

	// Where a scene ends up: a_scene itself, or for a transition, its
	// destination (followed through chained transitions). Never null for a
	// non-null a_scene.
	std::shared_ptr<const Scene> Settled(std::shared_ptr<const Scene> a_scene);

	// Case-insensitive sequence lookup; nullptr if there's no such sequence.
	std::shared_ptr<const Sequence> FindSequence(std::string_view a_id);

	// A copy of a scene's display data, safe to keep after the call.
	struct SceneSummary
	{
		std::string id;
		std::string name;
		std::string tags;       // comma-separated
		std::string furniture;  // furniture type it needs, "" = none
		std::string actions;    // its actions' names, comma-separated
	};

	// Which of the lists below to give: every scene for that many actors, or
	// only those these actors can play.
	struct ListFilter
	{
		std::vector<Sex> sexes;               // the actors; empty = no sex check
		bool             fixedOrder = false;  // roles already given (a running scene)
	};

	// All scenes with exactly a_actorCount actors that pass a_filter, sorted
	// by name.
	std::vector<SceneSummary> List(std::size_t a_actorCount, const ListFilter& a_filter = {});

	// All sequences for a_actorCount actors that pass a_filter, sorted by name.
	std::vector<SceneSummary> ListSequences(std::size_t a_actorCount, const ListFilter& a_filter = {});
}
