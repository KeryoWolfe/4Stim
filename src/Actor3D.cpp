#include "Actor3D.h"

#include <unordered_map>

#include "Physics.h"

namespace Actor3D
{
	namespace
	{
		// The 3D root each watched actor had last time (only compared, never
		// used: it may be gone). nullptr: unloaded.
		std::unordered_map<std::uint32_t, const void*> g_roots;
	}

	void Tick(const std::vector<std::uint32_t>& a_actors)
	{
		for (const auto id : a_actors) {
			const auto  actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			const void* root = actor ? static_cast<const void*>(actor->Get3D(false)) : nullptr;
			const auto  it = g_roots.find(id);
			if (it == g_roots.end()) {
				g_roots.emplace(id, root);  // first look: nothing to compare with
				continue;
			}
			if (root == it->second) {
				continue;
			}
			const bool back = it->second == nullptr;
			it->second = root;
			if (!root) {
				REX::INFO("Actor 3D: {:08X}'s 3D was unloaded", id);
				continue;
			}
			REX::INFO("Actor 3D: {:08X}'s 3D was {}: putting back what the scene changed", id, back ? "loaded again" : "rebuilt");
			Physics::Reapply(id);
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_actors)
	{
		for (const auto id : a_actors) {
			g_roots.erase(id);
		}
	}

	void Clear()
	{
		g_roots.clear();
	}
}
