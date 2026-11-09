#include "Core/Core.h"

namespace Core
{
	// ---- FourSequence (OStim's OSequence) ----
	// OStim's GraphTable::getRandomSequence: a random sequence for these
	// actors in this order, on this furniture type or a supertype of it ("":
	// sequences without furniture), passing a_mode's tag test (0 none, 1 has
	// a_tags[0], 2 any of them, 3 all of them). "" if none.
	std::string SQ_Random(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_tags, std::string a_csv, std::int32_t a_mode)
	{
		std::erase(a_actors, nullptr);
		if (a_actors.empty()) {
			return {};
		}
		if (!a_csv.empty()) {
			a_tags = SplitCSV(a_csv);
		}
		for (auto& tag : a_tags) {
			tag = LowerCopy(tag);
		}
		const auto                    furniture = LowerCopy(a_furniture);
		SceneRegistry::ListFilter     filter;
		for (const auto a : a_actors) {
			filter.actors.push_back(ActorProperties::Condition(a));
		}
		filter.fixedOrder = true;
		std::vector<std::string> picks;
		for (const auto& summary : SceneRegistry::ListSequences(a_actors.size(), filter)) {
			const auto sequence = SceneRegistry::FindSequence(summary.id);
			if (!sequence) {
				continue;
			}
			if (furniture.empty() ? !sequence->furniture.empty() : (sequence->furniture.empty() || !Furniture::IsA(furniture, sequence->furniture))) {
				continue;
			}
			const auto has = [&](const std::string& t) { return std::ranges::find(sequence->tags, t) != sequence->tags.end(); };
			const bool ok = a_mode == 0 || (a_mode == 1 && !a_tags.empty() && has(a_tags[0])) || (a_mode == 2 && std::ranges::any_of(a_tags, has)) ||
			                (a_mode == 3 && std::ranges::all_of(a_tags, has));
			if (ok) {
				picks.push_back(sequence->id);
			}
		}
		if (picks.empty()) {
			return {};
		}
		static std::mt19937 rng{ std::random_device{}() };
		return picks[std::uniform_int_distribution<std::size_t>(0, picks.size() - 1)(rng)];
	}
}
