#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// OStim's per-actor stimulation overrides (Serialization's action and
// event stimulation, Graph/Action/ActionAttributes::getStimulation): for an
// actor base, an action or event, and a side of it, a stimulation and / or
// max stimulation in place of the action's or event's own. Set with
// FourStim.SetActionStimulation & co. (OStim's OData), saved with the game.
namespace StimOverrides
{
	// OStim's Graph::Role bits, as its OData functions take them.
	enum Role : int
	{
		kActor = 1,
		kTarget = 2,
		kPerformer = 4,
	};

	enum class Kind
	{
		kAction,
		kEvent,
	};

	// The override, if one is set. a_id: the action or event id (any case).
	std::optional<float> Stimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id);
	std::optional<float> MaxStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id);

	void SetStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id, float a_value);
	void SetMaxStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id, float a_value);
	void ResetStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id);
	void ResetMaxStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id);

	// An actor's base form ID (what overrides are kept by), 0 if none.
	std::uint32_t BaseOf(std::uint32_t a_actorID);

	// The co-save: everything set, in a record of the caller's; a new game
	// or another save clears them first.
	struct Entry
	{
		std::uint8_t  kind = 0;  // Kind
		std::uint8_t  role = 0;  // Role bit
		std::uint8_t  max = 0;   // 1: max stimulation
		std::uint32_t base = 0;
		std::string   id;
		float         value = 0.0F;
	};
	std::vector<Entry> All();
	void               Restore(const std::vector<Entry>& a_entries);
	void               Clear();
}
