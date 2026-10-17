#include "PenisBend.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace PenisBend
{
	namespace
	{
		struct State
		{
			float steps = 0.0F;   // the scene's / alignment's
			int   offset = 0;     // the annotation's
			bool  applied = false;
		};

		Config                                   g_config;
		std::mutex                               g_lock;
		std::unordered_map<std::uint32_t, State> g_states;

		RE::NiAVObject* Node(RE::Actor* a_actor)
		{
			const auto root = a_actor ? a_actor->Get3D(false) : nullptr;
			return root ? root->GetObjectByName(RE::BSFixedString(g_config.node)) : nullptr;
		}

		// A turn of a_degrees around the node's own axis.
		RE::NiMatrix3 Turn(float a_degrees)
		{
			const float r = a_degrees * 3.14159265F / 180.0F;
			const float c = std::cos(r), s = std::sin(r);
			switch (g_config.axis) {
			case 0:
				return { 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, c, -s, 0.0F, 0.0F, s, c, 0.0F };
			case 1:
				return { c, 0.0F, s, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, -s, 0.0F, c, 0.0F };
			default:
				return { c, -s, 0.0F, 0.0F, s, c, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F };
			}
		}

		// ZeX's offset nodes have no turn of their own: back to none.
		void Reset(std::uint32_t a_id)
		{
			if (const auto node = Node(RE::TESForm::GetFormByID<RE::Actor>(a_id))) {
				node->SetLocalRotate(RE::NiMatrix3::IDENTITY);
			}
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void Set(std::uint32_t a_id, float a_steps)
	{
		std::scoped_lock lock(g_lock);
		auto&            state = g_states[a_id];
		if (state.steps != a_steps) {
			REX::INFO("Penis bend: {:08X} at {} step(s)", a_id, a_steps);
		}
		state.steps = a_steps;
	}

	void Offset(std::uint32_t a_id, int a_steps)
	{
		std::scoped_lock lock(g_lock);
		if (const auto it = g_states.find(a_id); it != g_states.end()) {
			it->second.offset = a_steps;
		}
	}

	void Tick()
	{
		std::scoped_lock lock(g_lock);
		if (g_config.disabled) {
			return;
		}
		for (auto& [id, state] : g_states) {
			// As OStim's bendSchlong: -9 to +9 (its -10, SoS's flaccid
			// state, has no Fallout 4 counterpart).
			const float total = std::clamp(state.steps + static_cast<float>(state.offset), -9.0F, 9.0F);
			if (total == 0.0F && !state.applied) {
				continue;  // never touched: leave the node alone
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (const auto node = Node(actor)) {
				node->SetLocalRotate(Turn(total * g_config.stepDegrees));
				state.applied = total != 0.0F;
			}
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			if (const auto it = g_states.find(id); it != g_states.end()) {
				if (it->second.applied) {
					Reset(id);
				}
				g_states.erase(it);
			}
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
