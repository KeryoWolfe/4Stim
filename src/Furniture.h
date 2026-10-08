#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Furniture types: which object references a scene can be played on (beds,
// chairs, tables...). Defined in JSON files under
// Data\F4SE\Plugins\4Stim\Furniture\ (see docs/FURNITURE.md). A scene names
// the type it needs ("furniture" in its scene file); it plays on furniture
// of that type or of any of its subtypes (a bench is a kind of chair).
namespace Furniture
{
	// Where a scene on a piece of furniture is placed: the point and
	// direction the actors' animations start from.
	struct Spot
	{
		RE::NiPoint3 position;
		float        heading = 0.0F;  // radians, game convention (0 = north)
	};

	// A piece of furniture found near the player.
	struct Found
	{
		std::string   type;  // furniture type id
		std::string   name;  // its display name
		std::uint32_t ref = 0;
		float         distance = 0.0F;
	};

	// Re-reads the type files (they're small: cheap enough to do whenever
	// the picker opens, so edits apply without restarting the game).
	void Reload();

	// The furniture type of a reference: its most specific match, or "" if
	// it isn't furniture any scene type knows.
	std::string Classify(RE::TESObjectREFR* a_ref);

	// Whether furniture of type a_type can play scenes made for a_wanted:
	// the same type, or a subtype of it.
	bool IsA(std::string_view a_type, std::string_view a_wanted);

	// The display name of a type (its id if it has none).
	std::string Name(std::string_view a_type);

	// The nearest piece of each type within a_radius of a_center (and within
	// a_maxHeight up or down), nearest first. Main thread only (walks the
	// loaded cells). With a_log, every candidate object is written to the
	// log with its model, for writing type files.
	std::vector<Found> FindNear(const RE::NiPoint3& a_center, float a_radius, float a_maxHeight, bool a_log);

	// Where a scene goes on a_ref, a piece of furniture of type a_type. For
	// types anchored on an edge, the side nearest a_near is used.
	Spot SpotFor(RE::TESObjectREFR* a_ref, std::string_view a_type, const RE::NiPoint3& a_near);
}
