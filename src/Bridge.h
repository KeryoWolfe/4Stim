#pragma once

#include <string>
#include <vector>

namespace SceneRegistry
{
	struct Scene;
}

// What main.cpp shares with the HUD and scene-event code: the scene the HUD
// follows, and the actions the HUD can trigger on it. Implemented in main.cpp.
namespace FourStim
{
	// The scene the HUD follows: the one the player is in, or an NPC scene
	// the player started to watch with the free camera. role0 == 0: none.
	// Not save-persisted yet (that's the claim registry's job).
	struct FocusedScene
	{
		std::uint32_t role0 = 0;
		std::uint32_t role1 = 0;  // 0 = solo scene
		std::string   sceneID;
		int           speed = 0;  // index into the scene's speeds

		[[nodiscard]] bool Active() const { return role0 != 0; }

		[[nodiscard]] std::vector<std::uint32_t> ActorIDs() const
		{
			std::vector<std::uint32_t> ids;
			if (role0) {
				ids.push_back(role0);
			}
			if (role1) {
				ids.push_back(role1);
			}
			return ids;
		}
	};

	RE::BSScript::IVirtualMachine* GetVM();

	FocusedScene GetFocusedScene();

	// Speed of the focused scene by a_delta. Returns the new speed (1-based),
	// or 0 if there's no focused scene.
	int ChangeFocusedSpeed(int a_delta);

	// Moves the focused scene to a_sceneID (same actor count). False if not possible.
	bool NavigateFocused(const std::string& a_sceneID);

	// Ends the focused scene through Papyrus (FourStimMenu.EndPlayerScene).
	void EndFocusedScene();

	// Opens the scene picker listing every scene with the focused scene's
	// actor count; picking one moves the focused scene there.
	void OpenSearchForFocused();

	// Fills {n} in a label with the name of the actor in role n and
	// capitalizes the first letter.
	std::string FormatLabel(std::string a_label, const FocusedScene& a_scene);

	// Whether the focused scene's actors (in their roles) may play a_scene:
	// its roles' sexes, unless bMatchSex is off.
	bool FocusedCanPlay(const SceneRegistry::Scene& a_scene);

	// Form IDs to actors, keeping role order. An actor that can't be found
	// stays in its slot as nullptr (None in Papyrus).
	std::vector<RE::Actor*> ResolveActors(const std::vector<std::uint32_t>& a_ids);
}
