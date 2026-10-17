#include "EyeLook.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace EyeLook
{
	namespace
	{
		constexpr float DEG = 3.14159265F / 180.0F;

		// One eye: its turn as the game left it (the base 4Stim turns from)
		// and what 4Stim last set. If the game sets the bone itself (eye
		// tracking), the next frame's base is the game's; if it doesn't, the
		// base stays and the turn isn't added up frame after frame.
		struct Eye
		{
			RE::NiMatrix3 base;
			RE::NiMatrix3 written;
			bool          known = false;
		};

		struct State
		{
			float up = 0.0F;
			float left = 0.0F;
			Eye   eyes[2];
			bool  warned = false;
		};

		Config                                   g_config;
		std::mutex                               g_lock;
		std::unordered_map<std::uint32_t, State> g_states;

		RE::NiAVObject* EyeNode(RE::Actor* a_actor, int a_which)
		{
			const auto root = a_actor ? a_actor->Get3D(false) : nullptr;
			return root ? root->GetObjectByName(RE::BSFixedString(a_which == 0 ? g_config.leftEye : g_config.rightEye)) : nullptr;
		}

		// A turn of a_angle radians about the unit axis a_axis (Rodrigues),
		// for column vectors as NiMatrix3 uses them.
		RE::NiMatrix3 AxisTurn(const RE::NiPoint3& a_axis, float a_angle)
		{
			const float c = std::cos(a_angle), s = std::sin(a_angle), t = 1.0F - c;
			const float x = a_axis.x, y = a_axis.y, z = a_axis.z;
			return { t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0.0F,
				t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0.0F,
				t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0.0F };
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void Set(std::uint32_t a_id, float a_up, float a_left)
	{
		std::scoped_lock lock(g_lock);
		auto             it = g_states.find(a_id);
		if (a_up == 0.0F && a_left == 0.0F && it == g_states.end()) {
			return;  // never turned: nothing to do
		}
		if (it == g_states.end()) {
			it = g_states.emplace(a_id, State{}).first;
		}
		if (it->second.up != a_up || it->second.left != a_left) {
			REX::INFO("Eyes: {:08X} look up {}, left {}", a_id, a_up, a_left);
		}
		it->second.up = a_up;
		it->second.left = a_left;
	}

	void Tick()
	{
		std::scoped_lock lock(g_lock);
		if (g_config.disabled) {
			return;
		}
		for (auto& [id, state] : g_states) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			// About the actor's up axis (left is a positive turn) and its
			// right-hand axis (up is a positive turn), in the world.
			const float heading = actor->data.angle.z;
			const auto  side = AxisTurn({ std::cos(heading), -std::sin(heading), 0.0F }, std::clamp(state.up, -100.0F, 100.0F) / 100.0F * g_config.maxUp * DEG);
			const auto  turn = AxisTurn({ 0.0F, 0.0F, 1.0F }, std::clamp(state.left, -100.0F, 100.0F) / 100.0F * g_config.maxSide * DEG) * side;
			bool        found = false;
			for (int which = 0; which < 2; ++which) {
				const auto node = EyeNode(actor, which);
				if (!node || !node->parent) {
					continue;
				}
				found = true;
				auto& eye = state.eyes[which];
				if (!eye.known || !(node->local.rotate == eye.written)) {
					eye.base = node->local.rotate;  // the game's (first time, or it set the bone itself)
					eye.known = true;
				}
				// The new world turn is turn x the eye's world turn without
				// 4Stim's; back into the parent's frame for the local one.
				const auto parent = node->parent->world.rotate;
				const auto world = parent * eye.base;
				eye.written = parent.Transpose() * (turn * world);
				node->SetLocalRotate(eye.written);
			}
			if (!found && actor->Get3D(false) && !state.warned) {
				state.warned = true;
				REX::INFO("Eyes: {:08X} has no \"{}\" / \"{}\" bones: OStim's look directions skipped", id, g_config.leftEye, g_config.rightEye);
			}
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			const auto it = g_states.find(id);
			if (it == g_states.end()) {
				continue;
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			for (int which = 0; which < 2; ++which) {
				const auto& eye = it->second.eyes[which];
				if (const auto node = eye.known ? EyeNode(actor, which) : nullptr; node && node->local.rotate == eye.written) {
					node->SetLocalRotate(eye.base);
				}
			}
			g_states.erase(it);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
