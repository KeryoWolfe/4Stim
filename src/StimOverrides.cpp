#include "StimOverrides.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

namespace StimOverrides
{
	namespace
	{
		// (kind, role, max, base, id lowercase) -> value. Read from the scene
		// clock and set from Papyrus, so behind a lock.
		using Key = std::tuple<std::uint8_t, std::uint8_t, std::uint8_t, std::uint32_t, std::string>;
		std::mutex             g_lock;
		std::map<Key, float>   g_values;

		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		Key MakeKey(Kind a_kind, int a_role, bool a_max, std::uint32_t a_base, const std::string& a_id)
		{
			return { static_cast<std::uint8_t>(a_kind), static_cast<std::uint8_t>(a_role), static_cast<std::uint8_t>(a_max ? 1 : 0), a_base, Lower(a_id) };
		}

		std::optional<float> Get(const Key& a_key)
		{
			std::scoped_lock lock(g_lock);
			const auto       it = g_values.find(a_key);
			return it != g_values.end() ? std::optional<float>(it->second) : std::nullopt;
		}

		void Set(const Key& a_key, float a_value)
		{
			std::scoped_lock lock(g_lock);
			g_values[a_key] = a_value;
		}

		void Erase(const Key& a_key)
		{
			std::scoped_lock lock(g_lock);
			g_values.erase(a_key);
		}

		bool ValidRole(int a_role) { return a_role == kActor || a_role == kTarget || a_role == kPerformer; }
	}

	std::optional<float> Stimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id)
	{
		return a_base ? Get(MakeKey(a_kind, a_role, false, a_base, a_id)) : std::nullopt;
	}

	std::optional<float> MaxStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id)
	{
		return a_base ? Get(MakeKey(a_kind, a_role, true, a_base, a_id)) : std::nullopt;
	}

	void SetStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id, float a_value)
	{
		if (a_base && ValidRole(a_role)) {
			Set(MakeKey(a_kind, a_role, false, a_base, a_id), a_value);
		}
	}

	void SetMaxStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id, float a_value)
	{
		if (a_base && ValidRole(a_role)) {
			Set(MakeKey(a_kind, a_role, true, a_base, a_id), a_value);
		}
	}

	void ResetStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id)
	{
		Erase(MakeKey(a_kind, a_role, false, a_base, a_id));
	}

	void ResetMaxStimulation(Kind a_kind, int a_role, std::uint32_t a_base, const std::string& a_id)
	{
		Erase(MakeKey(a_kind, a_role, true, a_base, a_id));
	}

	std::uint32_t BaseOf(std::uint32_t a_actorID)
	{
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actorID);
		const auto npc = actor ? actor->GetNPC() : nullptr;
		return npc ? npc->GetFormID() : 0;
	}

	std::vector<Entry> All()
	{
		std::scoped_lock   lock(g_lock);
		std::vector<Entry> out;
		for (const auto& [key, value] : g_values) {
			out.push_back({ std::get<0>(key), std::get<1>(key), std::get<2>(key), std::get<3>(key), std::get<4>(key), value });
		}
		return out;
	}

	void Restore(const std::vector<Entry>& a_entries)
	{
		std::scoped_lock lock(g_lock);
		g_values.clear();
		for (const auto& e : a_entries) {
			if (e.base && ValidRole(e.role) && e.kind <= 1) {
				g_values[{ e.kind, e.role, e.max, e.base, Lower(e.id) }] = e.value;
			}
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_values.clear();
	}
}
