#include "Core/Core.h"

namespace Core::Natives::FourActorUtil
{
	// ---- FourActorUtil (OStim's OActorUtil, docs/API.md) ----
	bool FulfillsCondition(std::monostate, RE::Actor* a_actor, RE::BGSPerk* a_condition)
	{
		return a_actor && a_condition && a_condition->perkConditions.IsTrue(a_actor, a_actor);
	}
	bool FulfillsAnyCondition(std::monostate, RE::Actor* a_actor, std::vector<RE::BGSPerk*> a_conditions)
	{
		return a_actor && std::ranges::any_of(a_conditions, [&](RE::BGSPerk* a_perk) { return a_perk && a_perk->perkConditions.IsTrue(a_actor, a_actor); });
	}
	bool FulfillsAllConditions(std::monostate, RE::Actor* a_actor, std::vector<RE::BGSPerk*> a_conditions)
	{
		return a_actor && std::ranges::all_of(a_conditions, [&](RE::BGSPerk* a_perk) { return !a_perk || a_perk->perkConditions.IsTrue(a_actor, a_actor); });
	}
	std::vector<RE::Actor*> EmptyArray(std::monostate) { return {}; }
	std::vector<RE::Actor*> CreateArray(std::monostate, std::int32_t a_size, RE::Actor* a_filler)
	{
		return a_size > 0 ? std::vector<RE::Actor*>(static_cast<std::size_t>(std::min(a_size, 128)), a_filler) : std::vector<RE::Actor*>{};
	}
	std::vector<RE::Actor*> ToArray(std::monostate, RE::Actor* a1, RE::Actor* a2, RE::Actor* a3, RE::Actor* a4, RE::Actor* a5, RE::Actor* a6, RE::Actor* a7,
		RE::Actor* a8, RE::Actor* a9, RE::Actor* a10)
	{
		std::vector<RE::Actor*> out;
		for (const auto a : { a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 }) {
			if (a) {
				out.push_back(a);
			}
		}
		return out;
	}
	// OActorUtil.Sort (OStim's ActorUtil::sort): the dominant actors first,
	// then those with a penis (otherwise in the order given); then the player
	// moved to a_playerIndex, or for two without one, where the
	// PlayerAlways... settings put them.
	std::vector<RE::Actor*> Sort(std::monostate, std::vector<RE::Actor*> a_actors, std::vector<RE::Actor*> a_dominant, std::int32_t a_playerIndex)
	{
		std::erase(a_actors, nullptr);
		const auto dominant = [&](RE::Actor* a) { return std::ranges::find(a_dominant, a) != a_dominant.end(); };
		std::ranges::stable_sort(a_actors, [&](RE::Actor* a, RE::Actor* b) {
			if (dominant(a) != dominant(b)) {
				return dominant(a);
			}
			return ActorProperties::HasPenis(a) && !ActorProperties::HasPenis(b);
		});
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto at = std::ranges::find(a_actors, player);
		if (at == a_actors.end()) {
			return a_actors;
		}
		int want = a_playerIndex;
		if (want < 0 || static_cast<std::size_t>(want) >= a_actors.size()) {
			std::vector<std::uint32_t> ids;
			for (const auto a : a_actors) {
				ids.push_back(a->GetFormID());
			}
			want = PlayerAlwaysIndex(ids);
		}
		if (want >= 0 && static_cast<std::size_t>(want) < a_actors.size()) {
			a_actors.erase(at);
			a_actors.insert(a_actors.begin() + want, player);
		}
		return a_actors;
	}
	// OActorUtil.GetActorsInRangeV2: actors around a_center, nearest first;
	// not disabled or ghosts; a_condition: only those who fulfill the perk's
	// conditions; a_onlyKnown: only actors with an actor type (OStim's
	// OStimActorsOnly).
	std::vector<RE::Actor*> GetActorsInRangeV2(std::monostate, RE::TESObjectREFR* a_center, float a_range, bool a_includeCenter, bool a_includePlayer,
		bool a_onlyKnown, RE::BGSPerk* a_condition)
	{
		std::vector<RE::Actor*> out;
		if (!a_center) {
			return out;
		}
		const auto center = a_center->data.location;
		Compat::ForEachReferenceInRange(center, a_range, [&](RE::TESObjectREFR* a_ref) {
			const auto actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
			if (!actor || (!a_includeCenter && a_ref == a_center) || (!a_includePlayer && IsPlayer(actor)) || actor->IsDisabled()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			if (a_condition && !a_condition->perkConditions.IsTrue(actor, actor)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			if (a_onlyKnown && ActorProperties::Type(actor).empty()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			out.push_back(actor);
			return RE::BSContainer::ForEachResult::kContinue;
		});
		std::ranges::sort(out, {}, [&](RE::Actor* a) { return a->data.location.GetSquaredDistance(center); });
		return out;
	}
	std::vector<std::string> ActorsToNames(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::vector<std::string> out;
		for (const auto a : a_actors) {
			const char* name = a ? a->GetDisplayFullName() : nullptr;
			out.emplace_back(name ? name : "");
		}
		return out;
	}
}
