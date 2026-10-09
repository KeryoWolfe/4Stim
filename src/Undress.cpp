#include "Undress.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "Actions.h"
#include "Bridge.h"

namespace Undress
{
	namespace
	{
		struct State
		{
			std::uint64_t              handled = 0;  // bit (slot - 30): already taken off (or asked to)
			std::vector<std::uint32_t> items;        // what was taken off, to put back on
		};

		Config                                   g_config;
		std::mutex                               g_lock;
		std::unordered_map<std::uint32_t, State> g_states;

		bool Allowed(int a_slot)
		{
			return std::ranges::find(g_config.slots, a_slot) != g_config.slots.end();
		}

		bool IsPlayer(std::uint32_t a_id)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			return player && player->GetFormID() == a_id;
		}

		// Takes off a_slots (those not taken off yet) through Papyrus.
		void Strip(std::uint32_t a_id, const std::vector<int>& a_slots)
		{
			if (!g_config.enabled || (!g_config.player && IsPlayer(a_id))) {
				return;
			}
			std::vector<std::int32_t> indices;  // F4SE's GetWornItem counts from slot 30
			{
				std::scoped_lock lock(g_lock);
				auto& state = g_states[a_id];
				for (const auto slot : a_slots) {
					const auto bit = std::uint64_t{ 1 } << (slot - 30);
					if (slot >= 30 && slot <= 61 && Allowed(slot) && !(state.handled & bit)) {
						state.handled |= bit;
						indices.push_back(slot - 30);
					}
				}
			}
			if (indices.empty()) {
				return;
			}
			if (const auto vm = FourStim::GetVM()) {
				REX::INFO("Undress: {:08X}, {} slot(s)", a_id, indices.size());
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				vm->DispatchStaticCall("FourStimUndress"sv, "Strip"sv, callback, static_cast<std::int32_t>(a_id), indices);
			}
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start)
	{
		if (!g_config.enabled || a_scene.noStrip) {
			return;
		}
		for (std::size_t role = 0; role < a_ids.size(); ++role) {
			if (a_start && g_config.atStart) {
				Strip(a_ids[role], g_config.slots);
				continue;
			}
			std::vector<int> slots;
			bool             full = false;
			for (const auto& action : a_scene.actions) {
				auto take = [&](std::size_t a_role, const Actions::Side& a_side) {
					if (a_role != role) {
						return;
					}
					if (a_side.fullStrip && g_config.fullMidScene) {
						full = true;
					} else if (g_config.partial) {
						slots.insert(slots.end(), a_side.undressSlots.begin(), a_side.undressSlots.end());
					}
				};
				take(action.actor, action.type->actor);
				take(action.target, action.type->target);
				take(action.performer, action.type->performer);
			}
			if (full) {
				Strip(a_ids[role], g_config.slots);
			} else if (!slots.empty()) {
				Strip(a_ids[role], slots);
			}
		}
	}

	void StripAll(std::uint32_t a_id)
	{
		Strip(a_id, g_config.slots);
	}

	void NoteStripped(std::uint32_t a_id, const std::vector<std::uint32_t>& a_items)
	{
		std::scoped_lock lock(g_lock);
		auto& items = g_states[a_id].items;
		for (const auto item : a_items) {
			if (std::ranges::find(items, item) == items.end()) {
				items.push_back(item);
			}
		}
	}

	void Redress(std::uint32_t a_id, bool a_force)
	{
		std::vector<std::int32_t> items;
		{
			std::scoped_lock lock(g_lock);
			const auto it = g_states.find(a_id);
			if (it == g_states.end()) {
				return;
			}
			for (const auto item : it->second.items) {
				items.push_back(static_cast<std::int32_t>(item));
			}
			g_states.erase(it);
		}
		if (items.empty() || (!g_config.redress && !a_force)) {
			return;
		}
		if (const auto vm = FourStim::GetVM()) {
			REX::INFO("Undress: dressing {:08X} again ({} item(s))", a_id, items.size());
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("FourStimUndress"sv, "Redress"sv, callback, static_cast<std::int32_t>(a_id), items);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
