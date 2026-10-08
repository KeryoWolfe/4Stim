#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Scenes are defined in JSON files under Data\F4SE\Plugins\4Stim\Scenes\,
// so animation authors add content with an Idle-record plugin plus a JSON
// file, no scripting. See SCENES.md for the format.
namespace SceneRegistry
{
	struct SceneActor
	{
		std::string sex;  // "male", "female" or "any" (informational for now)
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
		std::string                                icon;  // HUD icon under Data\Interface\4Stim\Icons\, or empty
		float                                      length = 0.0F;  // seconds of one play-through; 0 = not given
		std::string                                destination;    // transition: the scene it moves on to after `length`
		std::string                                sourceFile;

		bool IsTransition() const { return !destination.empty(); }
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
	};

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
		std::string tags;  // comma-separated
	};

	// All scenes with exactly a_actorCount actors, sorted by name.
	std::vector<SceneSummary> List(std::size_t a_actorCount);

	// All sequences for a_actorCount actors, sorted by name.
	std::vector<SceneSummary> ListSequences(std::size_t a_actorCount);
}
