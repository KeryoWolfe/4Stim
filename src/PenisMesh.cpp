#include "PenisMesh.h"

#include "EquipObjects.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace PenisMesh
{
	namespace
	{
		struct Known
		{
			std::size_t                fingerprint = 0;
			std::optional<bool>        penis;
			std::vector<std::uint32_t> items;  // worn items that give it (kept on by Undress)
		};

		struct Found
		{
			std::string                why;    // the mesh, for the log
			std::vector<std::uint32_t> items;  // worn items it's on (none: the body itself)
		};

		std::mutex                               g_lock;
		std::unordered_map<std::uint32_t, Known> g_known;
		std::unordered_set<std::uint32_t>        g_queued;      // looks waiting for the main thread
		std::atomic<std::thread::id>             g_mainThread;  // set from a task, read anywhere
		std::vector<std::string>                 g_names{ "penis", "schlong" };  // not "genital": some female bodies have a "Genitals" mesh

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

		bool ContainsNoCase(std::string_view a_text, std::string_view a_part)
		{
			return !a_part.empty() && std::ranges::search(a_text, a_part, [](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			}).begin() != a_text.end();
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

		// A penis mesh: skinned to ZeX's Penis_* bones, or (older mods, from
		// before ZeX's bones, skinned to the pelvis: Bag of Dicks' and
		// Futanari FEV's "Schlong") named like one (sPenisMeshNames). Why, or "".
		std::string PenisIn(RE::NiAVObject* a_root, const std::unordered_set<const RE::NiAVObject*>& a_skip, const std::vector<std::string>& a_names)
		{
			std::string why;
			ForEachGeometry(a_root, 64, a_skip, [&](RE::BSGeometry* a_geometry) {
				if (!why.empty()) {
					return;
				}
				const std::string_view name = a_geometry->name.c_str() ? a_geometry->name.c_str() : "";
				if (const auto skin = a_geometry->skinInstance.get()) {
					for (const auto bone : skin->bones) {
						const char* bn = bone ? bone->name.c_str() : nullptr;
						if (bn && _strnicmp(bn, "Penis_", 6) == 0) {
							why = std::format("\"{}\" (skinned to {})", name, bn);
							return;
						}
					}
				}
				// The game names a worn piece's geometry after its armor
				// ("<addon> (id)[n]/<armor> (id)"), not the NIF's shape, so
				// its shader is looked at too: Fallout 4 keeps the material
				// file (.bgsm) as the shader property's name
				// ("Materials\Futa\penis.bgsm", "materials\Schlong\Schlong.BGSM").
				std::string_view material;
				for (const auto& property : a_geometry->properties) {
					if (const auto shader = netimmerse_cast<RE::BSShaderProperty*>(property.get()); shader && shader->name.c_str()) {
						material = shader->name.c_str();
					}
				}
				for (const auto& part : a_names) {
					if (ContainsNoCase(name, part)) {
						why = std::format("\"{}\" (named like one: \"{}\")", name, part);
						return;
					}
					if (ContainsNoCase(material, part)) {
						why = std::format("\"{}\" (its material is like one: \"{}\", \"{}\")", name, material, part);
						return;
					}
				}
			});
			return why;
		}

		// Main thread: what the actor wears, piece by piece (so it's known
		// which item gives it), then the rest of the 3D (the body where the
		// game puts it outside the biped). 4Stim's equip objects skipped.
		std::optional<Found> Look(RE::Actor* a_actor)
		{
			const auto root = a_actor->Get3D(false);
			if (!root) {
				return std::nullopt;
			}
			const auto names = [] {
				std::scoped_lock lock(g_lock);
				return g_names;
			}();
			std::unordered_set<const RE::NiAVObject*> skip;
			std::unordered_set<const RE::NiAVObject*> seen;  // a clone in several slots: looked at once
			Found found;
			if (const auto biped = a_actor->biped.get()) {
				for (const auto& object : biped->object) {
					const auto form = object.parent.object;
					const auto clone = object.partClone.get();
					if (!clone) {
						continue;
					}
					skip.insert(clone);  // the walk of the whole 3D below doesn't do these again
					if ((form && EquipObjects::IsObjectItem(form->GetFormID())) || !seen.insert(clone).second) {
						continue;
					}
					if (auto why = PenisIn(clone, {}, names); !why.empty()) {
						if (found.why.empty()) {
							const char* item = form ? form->GetFormEditorID() : nullptr;
							found.why = std::format("{} on {:08X}{}", why, form ? form->GetFormID() : 0, item && *item ? std::format(" ({})", item) : "");
						}
						if (form) {
							found.items.push_back(form->GetFormID());
						}
					}
				}
			}
			if (found.why.empty()) {
				found.why = PenisIn(root, skip, names);
			}
			REX::INFO("Penis from body: {:08X} {}{}", a_actor->GetFormID(), found.why.empty() ? "has none" : "has one: ", found.why);
			return found;
		}

		// On the main thread: the cached answer while nothing worn or the 3D
		// changed, else a new look.
		std::optional<bool> OnMain(RE::Actor* a_actor)
		{
			const auto id = a_actor->GetFormID();
			const auto fingerprint = Fingerprint(a_actor);
			{
				std::scoped_lock lock(g_lock);
				if (const auto it = g_known.find(id); it != g_known.end() && it->second.fingerprint == fingerprint) {
					return it->second.penis;
				}
			}
			auto found = Look(a_actor);
			if (!found) {
				return std::nullopt;  // no 3D yet: looked at again next time
			}
			const bool penis = !found->why.empty();
			std::scoped_lock lock(g_lock);
			g_known[id] = { fingerprint, penis, std::move(found->items) };
			return penis;
		}
	}

	void SetMainThread(std::thread::id a_id)
	{
		g_mainThread.store(a_id);
	}

	void SetMeshNames(std::vector<std::string> a_names)
	{
		std::scoped_lock lock(g_lock);
		g_names = std::move(a_names);
		g_known.clear();
	}

	std::optional<bool> FromBody(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return std::nullopt;
		}
		if (std::this_thread::get_id() == g_mainThread.load()) {
			return OnMain(a_actor);
		}
		const auto id = a_actor->GetFormID();
		std::scoped_lock lock(g_lock);
		if (const auto it = g_known.find(id); it != g_known.end()) {
			return it->second.penis;
		}
		if (g_queued.insert(id).second) {
			// F4SE's tasks run on the main thread: the look happens there,
			// straight away (never through FromBody again, which would queue
			// another task for ever if the main thread weren't known yet).
			F4SE::GetTaskInterface()->AddTask([id]() {
				{
					std::scoped_lock lock(g_lock);
					g_queued.erase(id);
				}
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
					OnMain(actor);
				}
			});
		}
		return std::nullopt;
	}

	bool GivesPenis(RE::Actor* a_actor, std::uint32_t a_item)
	{
		if (!a_actor) {
			return false;
		}
		if (std::this_thread::get_id() == g_mainThread.load()) {
			OnMain(a_actor);  // up to date with what's worn now
		}
		std::scoped_lock lock(g_lock);
		const auto it = g_known.find(a_actor->GetFormID());
		return it != g_known.end() && std::ranges::find(it->second.items, a_item) != it->second.items.end();
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_known.clear();
	}
}
