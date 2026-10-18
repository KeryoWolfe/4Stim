#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SceneRegistry.h"

// Equip objects (docs/ACTORS.md), as OStim's: armor items 4Stim puts on
// actors during scenes and takes away when they end: strap-ons, tongues and
// anything an addon wants. Each is a file in
// Data\F4SE\Plugins\4Stim\Equip Objects\ (OStim's format: a type, a name, the
// armor, its variants). An actor wears one of a type when a scene's action
// asks for it ("equipObject"), when their actor properties say so, when an
// animation's 4StimEquipObject annotation does, or when a script asks; a
// woman without a penis wears a strap-on in a role that needs one.
//
// Departure from OStim: lights are 4Stim's own (Lights.h, placed game
// lights), not equip objects, since 4Stim has no plugin for light armors.
namespace EquipObjects
{
	struct Config
	{
		bool unequipStrapOnIfNotNeeded = false;  // OStim's: off in roles that don't need one (else only when one needs a vagina)
	};
	Config& Settings();

	// Re-reads the files.
	void Reload();

	// The types there are files for, and whether an armor is one of them
	// (undressing leaves those alone).
	std::vector<std::string> Types();
	bool                     IsObjectItem(std::uint32_t a_formID);

	// a_ids[role] play a_scene (a_start: it's just started). As OStim's
	// changeNode: the scene's equip objects change, and strap-ons go on or
	// off. Main thread.
	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start);

	// The actors left their scene: everything comes off and goes away.
	void Leave(const std::vector<std::uint32_t>& a_ids);

	// OStim's OActor functions, for actors in a scene. Any thread.
	bool Equip(std::uint32_t a_id, std::string a_type);
	void Unequip(std::uint32_t a_id, std::string a_type);
	bool IsEquipped(std::uint32_t a_id, std::string a_type);
	bool SetVariant(std::uint32_t a_id, std::string a_type, std::string a_variant, float a_seconds);
	void UnsetVariant(std::uint32_t a_id, std::string a_type);

	// The objects of a type (id, name), the one a_id wears of it, and
	// choosing another (the HUD's options, OStim's options menu; worn now:
	// swapped). Any thread.
	std::vector<std::pair<std::string, std::string>> ObjectsOf(std::string a_type);
	std::string                                      Chosen(std::uint32_t a_id, std::string a_type);
	bool                                             SetChosen(std::uint32_t a_id, std::string a_type, std::string a_objectID);

	// Times variants (SetVariant with a duration). Main thread.
	void Tick(float a_seconds);

	// The items 4Stim gave a_id (for the save), and taking them away again
	// after a load.
	std::vector<std::uint32_t> Given(std::uint32_t a_id);
	void                       TakeBack(std::uint32_t a_id, const std::vector<std::uint32_t>& a_items);

	// Each actor's chosen objects (type -> object id), for the save; put
	// back from it (a game loading or a new one: ClearChosen first).
	std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> AllChosen();
	void RestoreChosen(std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> a_chosen);
	void ClearChosen();

	// A game is loading: forget who wears what (the choices stay).
	void Clear();
}
