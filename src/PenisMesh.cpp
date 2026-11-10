#include "PenisMesh.h"

#include "EquipObjects.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace PenisMesh
{
	namespace
	{
		struct Known
		{
			std::size_t         fingerprint = 0;
			std::optional<bool> penis;
		};

		std::mutex                               g_lock;
		std::unordered_map<std::uint32_t, Known> g_known;
		std::unordered_set<std::uint32_t>        g_queued;  // looks waiting for the main thread
		std::thread::id                          g_mainThread;

		// What the body is made of right now: the 3D's root and each worn
		// piece's clone in it. Equipping, unequipping or reloading the 3D
		// changes it, and only then is the body looked at again.
		std::size_t Fingerprint(RE::Actor* a_actor)
		{
			std::size_t hash = std::hash<const void*>{}(a_actor->Get3D(false));
			if (const auto biped = a_actor->biped.get()) {
				for (const auto& object : biped->object) {
					hash = hash * 31 + std::hash<const void*>{}(object.partClone.get());
				}
			}
			return hash;
		}

		bool IsPenisBone(const RE::NiAVObject* a_bone)
		{
			const char* name = a_bone ? a_bone->name.c_str() : nullptr;
			return name && _strnicmp(name, "Penis_", 6) == 0;
		}

		// Every geometry under a_object, skipping a_skip's subtrees.
		template <class F>
		void ForEachGeometry(RE::NiAVObject* a_object, int a_depth, const std::unordered_set<const RE::NiAVObject*>& a_skip, F&& a_do)
		{
			if (!a_object || a_skip.contains(a_object)) {
				return;
			}
			if (const auto geometry = a_object->IsGeometry()) {
				a_do(geometry);
			}
			if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
				for (const auto& child : node->children) {
					ForEachGeometry(child.get(), a_depth - 1, a_skip, a_do);
				}
			}
		}

		// Main thread: does anything the actor wears (but 4Stim's equip
		// objects) have a mesh skinned to Penis_* bones?
		std::optional<bool> Look(RE::Actor* a_actor)
		{
			const auto root = a_actor->Get3D(false);
			if (!root) {
				return std::nullopt;
			}
			std::unordered_set<const RE::NiAVObject*> skip;
			if (const auto biped = a_actor->biped.get()) {
				for (const auto& object : biped->object) {
					const auto form = object.parent.object;
					if (form && object.partClone && EquipObjects::IsObjectItem(form->GetFormID())) {
						skip.insert(object.partClone.get());
					}
				}
			}
			std::string found;
			ForEachGeometry(root, 64, skip, [&](RE::BSGeometry* a_geometry) {
				const auto skin = found.empty() ? a_geometry->skinInstance.get() : nullptr;
				if (!skin) {
					return;
				}
				for (const auto bone : skin->bones) {
					if (IsPenisBone(bone)) {
						found = std::format("\"{}\" (skinned to {})", a_geometry->name.c_str() ? a_geometry->name.c_str() : "?", bone->name.c_str());
						return;
					}
				}
			});
			REX::INFO("Penis from body: {:08X} {}{}", a_actor->GetFormID(), found.empty() ? "has none" : "has one: ", found);
			return !found.empty();
		}
	}

	void SetMainThread(std::thread::id a_id)
	{
		g_mainThread = a_id;
	}

	std::optional<bool> FromBody(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return std::nullopt;
		}
		const auto id = a_actor->GetFormID();
		if (std::this_thread::get_id() != g_mainThread) {
			std::scoped_lock lock(g_lock);
			if (const auto it = g_known.find(id); it != g_known.end()) {
				return it->second.penis;
			}
			if (g_queued.insert(id).second) {
				F4SE::GetTaskInterface()->AddTask([id]() {
					{
						std::scoped_lock lock(g_lock);
						g_queued.erase(id);
					}
					FromBody(RE::TESForm::GetFormByID<RE::Actor>(id));
				});
			}
			return std::nullopt;
		}
		const auto fingerprint = Fingerprint(a_actor);
		{
			std::scoped_lock lock(g_lock);
			if (const auto it = g_known.find(id); it != g_known.end() && it->second.fingerprint == fingerprint) {
				return it->second.penis;
			}
		}
		const auto penis = Look(a_actor);
		if (!penis) {
			return std::nullopt;  // no 3D yet: looked at again next time
		}
		std::scoped_lock lock(g_lock);
		g_known[id] = { fingerprint, penis };
		return penis;
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_known.clear();
	}
}
