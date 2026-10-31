#include "PenisBend.h"

#include "Erection.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <chrono>
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
			bool  warned = false;  // said once that the node isn't there
			// The node's own turn before 4Stim touched it: bends go on top of
			// it, and it's put back when the scene ends.
			std::optional<RE::NiMatrix3> original;
		};

		// ZeX's genital bones as they were before the actor's first scene this
		// game (the skeleton's rest pose), put back when each scene ends. The
		// game's own behaviors don't animate ZeX's extra bones, so whatever
		// pose a scene's animation (an AAF pack's, keying Penis_00 and on) last
		// gave them stayed after the scene, at an odd angle, and the SMP
		// physics built on that swung about (testers' reports, the owner's
		// video). OStim has no such thing: Skyrim's SoS bones are animated by
		// SoS's own behavior.
		struct RestBone
		{
			RE::BSFixedString name;
			RE::NiTransform   local;
		};
		std::unordered_map<std::uint32_t, std::vector<RestBone>> g_rest;
		constexpr std::string_view                              REST_PREFIXES[] = { "Penis", "Vagina", "Anus", "Clit" };

		void CollectBones(RE::NiAVObject* a_object, std::vector<RestBone>& a_out, int a_depth)
		{
			const auto node = a_object && a_depth > 0 ? a_object->IsNode() : nullptr;
			if (!node) {
				return;
			}
			const std::string_view name{ node->name.c_str() };
			if (std::ranges::any_of(REST_PREFIXES, [&](std::string_view a_prefix) { return name.starts_with(a_prefix); })) {
				a_out.push_back({ node->name, node->local });
			}
			for (const auto& child : node->children) {
				CollectBones(child.get(), a_out, a_depth - 1);
			}
		}

		RE::NiAVObject* Body(std::uint32_t a_id)
		{
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
			return actor ? actor->Get3D(false) : nullptr;  // the player's third-person body
		}

		// Main thread. Returns how many bones were put back.
		std::size_t RestoreBones(std::uint32_t a_id, const std::vector<RestBone>& a_bones)
		{
			const auto root = Body(a_id);
			if (!root) {
				return 0;
			}
			std::size_t count = 0;
			for (const auto& bone : a_bones) {
				if (const auto node = root->GetObjectByName(bone.name)) {
					node->local = bone.local;
					++count;
				}
			}
			return count;
		}

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

		// Back to the node's own turn. Bends used to replace the node's turn
		// and put "none" back at the end, on the assumption that ZeX's offset
		// nodes have no turn of their own; a skeleton whose node has one was
		// left at an odd angle after every scene, and the SMP physics built on
		// it swung about (testers' reports).
		void Reset(std::uint32_t a_id, const State& a_state)
		{
			if (const auto node = Node(RE::TESForm::GetFormByID<RE::Actor>(a_id))) {
				node->SetLocalRotate(a_state.original.value_or(RE::NiMatrix3::IDENTITY));
				REX::INFO("Penis bend: {:08X} back to its own angle", a_id);
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
			// As OStim's bendSchlong: -10 to +9, where -10 is SoS's flaccid
			// state (OStim's SOSFlaccid): Erection.h makes the body flaccid,
			// and the node goes back to its own angle.
			float total = std::clamp(std::round(state.steps + static_cast<float>(state.offset)), -10.0F, 9.0F);
			Erection::SetBend(id, static_cast<int>(total));
			if (total <= -10.0F) {
				total = 0.0F;
			}
			if (total == 0.0F && !state.applied) {
				continue;  // never touched: leave the node alone
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (const auto node = Node(actor)) {
				if (!state.applied && total != 0.0F) {
					REX::INFO("Penis bend: {:08X} bent {} step(s) ({} degrees) at {}", id, total, total * g_config.stepDegrees, g_config.node);
				}
				if (!state.original) {
					state.original = node->GetLocalRotate();
				}
				node->SetLocalRotate(*state.original * Turn(total * g_config.stepDegrees));
				state.applied = total != 0.0F;
			} else if (actor && actor->Get3D(false) && !state.warned) {
				state.warned = true;
				REX::INFO("Penis bend: {:08X} has no \"{}\" node in their skeleton (not ZeX, or no penis): not bent", id, g_config.node);
			}
		}
	}

	void NoteRestPose(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			if (g_rest.contains(id)) {
				continue;
			}
			std::vector<RestBone> bones;
			CollectBones(Body(id), bones, 64);
			if (!bones.empty()) {
				REX::INFO("Genital bones: {:08X}'s rest pose noted ({} bone(s))", id, bones.size());
				g_rest.emplace(id, std::move(bones));
			}
		}
	}

	void RestoreRestPose(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<std::pair<std::uint32_t, std::vector<RestBone>>> todo;
		{
			std::scoped_lock lock(g_lock);
			for (const auto id : a_ids) {
				if (const auto it = g_rest.find(id); it != g_rest.end()) {
					todo.emplace_back(id, it->second);
				}
			}
		}
		if (todo.empty()) {
			return;
		}
		// Now, and twice more as the scene's animation blends out (it can
		// still pose the bones for a moment after IdleStop), all before the
		// physics swap back and rebuild (2 s and more after the scene).
		for (const auto& [id, bones] : todo) {
			REX::INFO("Genital bones: {:08X} back to their rest pose ({} bone(s))", id, RestoreBones(id, bones));
		}
		std::thread([todo]() {
			for (const auto wait : { 500, 1000 }) {
				std::this_thread::sleep_for(std::chrono::milliseconds(wait));
				F4SE::GetTaskInterface()->AddTask([todo]() {
					for (const auto& [id, bones] : todo) {
						RestoreBones(id, bones);
					}
				});
			}
		}).detach();
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			if (const auto it = g_states.find(id); it != g_states.end()) {
				if (it->second.original) {
					Reset(id, it->second);
				}
				g_states.erase(it);
			}
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
		g_rest.clear();
	}
}
