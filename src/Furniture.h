#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SceneRegistry.h"

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

	// A piece of furniture found near the player. A wall (a type anchored
	// on "wall") is no object: ref is 0 and spot is where it was found, at
	// the foot of the wall, facing out from it.
	struct Found
	{
		std::string         type;  // furniture type id
		std::string         name;  // its display name
		std::uint32_t       ref = 0;
		float               distance = 0.0F;
		std::optional<Spot> spot;  // walls
	};

	// A loose item near a scene (OStim's clutter: misc items, weapons,
	// books...), where it lay when the scene started.
	struct ClutterItem
	{
		std::uint32_t ref = 0;
		RE::NiPoint3  position;
		RE::NiPoint3  angle;  // radians
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
	// log with its model, for writing type files. With a_from (who stands
	// at a_center: its cell is searched by ray casts), also the nearest wall
	// for types anchored on "wall".
	std::vector<Found> FindNear(const RE::NiPoint3& a_center, float a_radius, float a_maxHeight, bool a_log, RE::TESObjectREFR* a_from = nullptr);

	// Where a scene goes on a found piece: SpotFor on its object, or for a
	// wall its spot with the type's offset and rotation. a_near: who the
	// edge side is chosen for (the player).
	Spot SpotOf(const Found& a_found, const RE::NiPoint3& a_near, bool a_floorScene = false);

	// Loose items within a_radius of a_center (OStim's SetResetClutter):
	// where they lie now, to put back the ones a scene knocked about.
	// Main thread.
	std::vector<ClutterItem> SnapshotClutter(const RE::NiPoint3& a_center, float a_radius);

	// The items of a_items that moved, back where they were (through
	// Papyrus: FourStimFurniture.PutBack). Returns how many. Main thread.
	std::size_t RestoreClutter(const std::vector<ClutterItem>& a_items);

	// Whether a scene with no furniture of its own may play on furniture of
	// a_type (one whose type chain reaches "none"): its "floorScenes" tag
	// rules (a mattress only takes lying scenes). True for unknown types.
	bool AllowsFloorScene(std::string_view a_type, const SceneRegistry::Scene& a_scene);

	// Where a scene goes on a_ref, a piece of furniture of type a_type. For
	// types anchored on an edge, the side nearest a_near is used; with
	// a_floorScene (a scene without furniture of its own, on a bed), the
	// middle, lengthwise, at the edge spot's height.
	Spot SpotFor(RE::TESObjectREFR* a_ref, std::string_view a_type, const RE::NiPoint3& a_near, bool a_floorScene = false);

	// OStim's multiplyScale: actors on furniture of a_type are scaled by it
	// (1 for unknown types).
	float MultiplyScale(std::string_view a_type);

	// Whether type a_type places scenes on an edge (beds, tables): floor
	// scenes on it get their own spot (SpotFor's a_floorScene).
	bool IsEdgeAnchored(std::string_view a_type);

	// How much of the straight line a_from -> a_to is clear (1 = all of it),
	// by a line-of-sight ray: walls, furniture and clutter stop it; actors
	// and a_ignore don't. Main thread.
	float ClearFraction(RE::TESObjectREFR* a_ignore, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to);
}
