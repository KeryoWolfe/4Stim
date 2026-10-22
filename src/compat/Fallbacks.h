#pragma once

#include <cstdint>
#include <functional>

// 4Stim: stand-ins for the few game functions the Runtime Database can't find
// on 1.10.163 (docs/OLDGEN.md), so 4Stim loads there with the feature done
// another way, or off, instead of not loading at all. On AE and NG every one
// of these just calls the game's function.
namespace Compat
{
	// An address 4Stim can do without, and what's lost when it isn't found.
	struct OptionalID
	{
		std::uint64_t id;
		const char*   without;  // for the log
	};

	inline constexpr OptionalID OPTIONAL_IDS[] = {
		{ 2229674, "the actor's sex comes from their base (TESNPC::GetSex)" },
		{ 2698044, "nearby actors and furniture are found through the worldspace's loaded cells" },
		{ 2194566, "nearby actors and furniture are found through the worldspace's loaded cells" },
		{ 2195747, "4Stim releases its raycasts' Havok world reference itself" },
		{ 2231227, "NPCs using furniture or an idle marker are taken out of it through Papyrus (MoveTo their own spot)" },
		{ 2267064, "4Stim's sounds follow the actor without a start position" },
		{ 2269259, "4Stim works out sound files' resource IDs itself" },
		{ 2295298, "MCM texts are added to the game's translator by 4Stim itself" },
	};

	// Looks the addresses above up now (at load), not mid-game: a miss in
	// the Runtime Database takes a moment.
	void Prime();

	// Actor::GetSex, or the base's sex where it isn't found.
	RE::SEX GetSex(RE::Actor* a_actor);

	// TES::ForEachReferenceInRange (the player's interior cell, or the
	// attached exterior cells, and the sky cell), done through the player's
	// cell and its worldspace's cell map where TES isn't found.
	void ForEachReferenceInRange(const RE::NiPoint3& a_origin, float a_radius, std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR*)> a_callback);

	// Call before a bhkPickData goes out of scope: drops the Havok world
	// reference the game's Pick took, itself, where
	// hkReferencedObject::RemoveReference isn't found (so the destructor
	// doesn't call it).
	void ReleasePick(RE::bhkPickData& a_pick);

	// ReleasePick when it goes out of scope: declare it right after the
	// bhkPickData, so it runs before the pick's destructor.
	struct PickGuard
	{
		RE::bhkPickData& pick;
		~PickGuard() { ReleasePick(pick); }
	};

	// BSResource::ID(a_path) (a path under Data\), worked out by 4Stim where
	// BSResource::ID::GenerateFromPath isn't found: as Fallout 4's BA2
	// archives store their files (the game looks them up by this ID), the
	// lowercase stem's and folder's CRC-32 and the extension's first 4
	// characters.
	RE::BSResource::ID ResourceID(const char* a_path);

	bool CanStopInteractingQuick();  // Actor::StopInteractingQuick
	bool CanSetSoundPosition();      // BSSoundHandle::SetPosition
	bool CanAddTranslations();       // BSScaleformTranslator::AddTranslations
}
