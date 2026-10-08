#include "SceneRegistry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace SceneRegistry
{
	namespace
	{
		constexpr auto SCENE_FOLDER = "Data/F4SE/Plugins/4Stim/Scenes";

		std::mutex                             g_lock;
		bool                                   g_loaded = false;
		std::unordered_map<std::string, Scene> g_scenes;  // key: lowercase id; only used while loading

		// The loaded scenes, handed out as shared pointers so a scene stays
		// valid for whoever holds it even if ReloadScenes runs meanwhile
		// (Papyrus calls arrive on several threads).
		std::unordered_map<std::string, std::shared_ptr<const Scene>> g_published;

		// Sequences: raw while loading (checked once all scenes are in), then
		// published like the scenes.
		struct RawSequence
		{
			Sequence       sequence;
			nlohmann::json entries;
		};
		std::vector<RawSequence>                                         g_rawSequences;
		std::unordered_map<std::string, std::shared_ptr<const Sequence>> g_sequences;

		std::string Lower(std::string_view a_str)
		{
			std::string out(a_str);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		// a_idle is the Idle's ID within its own plugin, e.g. "0xAB8" or
		// "0x000AB8" (the load-order prefix is ignored, so full IDs copied
		// from xEdit also work).
		RE::TESIdleForm* ResolveIdle(const std::string& a_plugin, const std::string& a_idle, std::string& a_error)
		{
			std::uint32_t id = 0;
			try {
				id = static_cast<std::uint32_t>(std::stoul(a_idle, nullptr, 0));
			} catch (...) {
				a_error = "idle \"" + a_idle + "\" is not a valid form ID (expected hex like \"0xAB8\")";
				return nullptr;
			}

			const auto handler = RE::TESDataHandler::GetSingleton();
			if (!handler) {
				a_error = "game data isn't loaded yet";
				return nullptr;
			}

			// Regular plugins use the low 24 bits; light (ESL) plugins the low 12.
			auto idle = handler->LookupForm<RE::TESIdleForm>(id & 0x00FFFFFF, a_plugin);
			if (!idle) {
				idle = handler->LookupForm<RE::TESIdleForm>(id & 0x00000FFF, a_plugin);
			}
			if (!idle) {
				a_error = "no Idle " + a_idle + " in \"" + a_plugin + "\" (plugin not loaded, wrong ID, or the record isn't an Idle)";
			}
			return idle;
		}

		void LoadFile(const std::filesystem::path& a_path, int& a_loaded, int& a_skipped)
		{
			const auto file = a_path.filename().string();

			nlohmann::json root;
			try {
				std::ifstream in(a_path);
				root = nlohmann::json::parse(in, nullptr, true, true);  // comments allowed
			} catch (const std::exception& e) {
				REX::WARN("Scenes: {}: not valid JSON: {}", file, e.what());
				++a_skipped;
				return;
			}

			const auto filePlugin = root.value("plugin", std::string{});
			const auto scenes = root.find("scenes");
			const auto sequences = root.find("sequences");
			const bool hasSequences = sequences != root.end() && sequences->is_array();
			if ((scenes == root.end() || !scenes->is_array()) && !hasSequences) {
				REX::WARN("Scenes: {}: missing a \"scenes\" (or \"sequences\") array", file);
				++a_skipped;
				return;
			}

			if (hasSequences) {
				for (const auto& entry : *sequences) {
					const auto id = entry.is_object() ? entry.value("id", std::string{}) : std::string{};
					const auto list = entry.is_object() ? entry.find("scenes") : entry.end();
					if (id.empty() || list == entry.end() || !list->is_array() || list->empty()) {
						REX::WARN("Scenes: {}: a sequence needs an \"id\" and a non-empty \"scenes\" list, skipped", file);
						continue;
					}
					RawSequence raw;
					raw.sequence.id = id;
					raw.sequence.name = entry.value("name", id);
					raw.sequence.sourceFile = file;
					if (const auto tags = entry.find("tags"); tags != entry.end() && tags->is_array()) {
						for (const auto& tag : *tags) {
							if (tag.is_string()) {
								raw.sequence.tags.push_back(tag.get<std::string>());
							}
						}
					}
					raw.entries = *list;
					g_rawSequences.push_back(std::move(raw));
				}
			}
			if (scenes == root.end() || !scenes->is_array()) {
				return;
			}

			for (const auto& entry : *scenes) {
				const auto id = entry.value("id", std::string{});
				if (id.empty()) {
					REX::WARN("Scenes: {}: a scene has no \"id\", skipped", file);
					++a_skipped;
					continue;
				}

				Scene scene;
				scene.id = id;
				scene.name = entry.value("name", id);
				scene.icon = entry.value("icon", std::string{});
				scene.destination = entry.value("destination", std::string{});
				scene.furniture = Lower(entry.value("furniture", std::string{}));
				if (scene.furniture == "none") {
					scene.furniture.clear();
				}
				if (const auto off = entry.find("furnitureOffset"); off != entry.end()) {
					if (off->is_array() && (off->size() == 3 || off->size() == 4) && std::ranges::all_of(*off, [](const nlohmann::json& v) { return v.is_number(); })) {
						for (std::size_t i = 0; i < off->size(); ++i) {
							scene.furnitureOffset[i] = (*off)[i].get<float>();
						}
					} else {
						REX::WARN("Scenes: {}: scene \"{}\" \"furnitureOffset\" must be [x, y, z] or [x, y, z, degrees], ignored", file, id);
					}
				}
				if (const auto length = entry.find("length"); length != entry.end()) {
					if (length->is_number() && length->get<float>() > 0.0F) {
						scene.length = length->get<float>();
					} else {
						REX::WARN("Scenes: {}: scene \"{}\" \"length\" must be a positive number of seconds, ignored", file, id);
					}
				}
				scene.sourceFile = file;
				if (const auto tags = entry.find("tags"); tags != entry.end() && tags->is_array()) {
					for (const auto& tag : *tags) {
						if (tag.is_string()) {
							scene.tags.push_back(tag.get<std::string>());
						}
					}
				}

				const auto actors = entry.find("actors");
				if (actors == entry.end() || !actors->is_array() || actors->empty()) {
					REX::WARN("Scenes: {}: scene \"{}\" has no \"actors\", skipped", file, id);
					++a_skipped;
					continue;
				}
				for (const auto& a : *actors) {
					const auto sex = Lower(a.is_object() ? a.value("sex", std::string{ "any" }) : std::string{ "any" });
					SceneActor role;
					if (sex == "male" || sex == "m") {
						role.sex = Sex::kMale;
					} else if (sex == "female" || sex == "f") {
						role.sex = Sex::kFemale;
					} else if (sex != "any" && !sex.empty()) {
						REX::WARN("Scenes: {}: scene \"{}\": unknown \"sex\" \"{}\" (male, female or any), taken as any", file, id, sex);
					}
					scene.actors.push_back(role);
				}
				const auto roles = scene.actors.size();

				// Idles: either a "speeds" list ({"idles": [one per role]}), or the
				// single-speed shorthand with an "idle" on each actor.
				bool ok = true;
				auto resolve = [&](const nlohmann::json& a_idle, const std::string& a_plugin, std::size_t a_speed, std::size_t a_role) -> RE::TESIdleForm* {
					const auto idleID = a_idle.is_string() ? a_idle.get<std::string>() : std::string{};
					if (a_plugin.empty() || idleID.empty()) {
						REX::WARN("Scenes: {}: scene \"{}\" speed {} role {} needs an idle ID and a \"plugin\" (on the actor or the file)", file, id, a_speed + 1, a_role);
						return nullptr;
					}
					std::string error;
					const auto  idle = ResolveIdle(a_plugin, idleID, error);
					if (!idle) {
						REX::WARN("Scenes: {}: scene \"{}\" speed {} role {}: {}", file, id, a_speed + 1, a_role, error);
					}
					return idle;
				};

				if (const auto speeds = entry.find("speeds"); speeds != entry.end()) {
					if (!speeds->is_array() || speeds->empty()) {
						REX::WARN("Scenes: {}: scene \"{}\" has an empty \"speeds\" list, skipped", file, id);
						ok = false;
					}
					for (std::size_t sp = 0; ok && sp < speeds->size(); ++sp) {
						const auto& speed = (*speeds)[sp];
						const auto  idles = speed.find("idles");
						if (idles == speed.end() || !idles->is_array() || idles->size() != roles) {
							REX::WARN("Scenes: {}: scene \"{}\" speed {} needs \"idles\" with one entry per actor ({})", file, id, sp + 1, roles);
							ok = false;
							break;
						}
						const auto plugin = speed.value("plugin", filePlugin);
						std::vector<RE::TESIdleForm*> set;
						for (std::size_t role = 0; role < roles; ++role) {
							const auto idle = resolve((*idles)[role], plugin, sp, role);
							if (!idle) {
								ok = false;
								break;
							}
							set.push_back(idle);
						}
						scene.speeds.push_back(std::move(set));
					}
				} else {
					std::vector<RE::TESIdleForm*> set;
					for (std::size_t role = 0; ok && role < roles; ++role) {
						const auto& a = (*actors)[role];
						const auto  idle = resolve(a.value("idle", nlohmann::json{}), a.value("plugin", filePlugin), 0, role);
						ok = idle != nullptr;
						set.push_back(idle);
					}
					scene.speeds.push_back(std::move(set));
				}
				if (!ok) {
					++a_skipped;
					continue;
				}

				if (const auto navs = entry.find("navigations"); navs != entry.end() && navs->is_array()) {
					for (const auto& n : *navs) {
						const auto to = n.value("to", std::string{});
						if (!to.empty()) {
							scene.navigations.push_back({ to, n.value("label", to), n.value("icon", std::string{}) });
						}
					}
				}

				const auto key = Lower(id);
				if (const auto existing = g_scenes.find(key); existing != g_scenes.end()) {
					REX::WARN("Scenes: {}: scene \"{}\" replaces the one from {}", file, id, existing->second.sourceFile);
				}
				g_scenes[key] = std::move(scene);
				++a_loaded;
			}
		}

		// Checks the raw sequences against the loaded scenes. Returns the
		// number kept.
		int BuildSequences(std::unordered_map<std::string, std::shared_ptr<const Sequence>>& a_out)
		{
			int kept = 0;
			for (auto& raw : g_rawSequences) {
				auto&       seq = raw.sequence;
				std::string problem;
				int         index = 0;
				for (const auto& e : raw.entries) {
					++index;
					const auto sceneID = e.is_object() ? e.value("id", std::string{}) : e.is_string() ? e.get<std::string>() : std::string{};
					const auto it = g_scenes.find(Lower(sceneID));
					if (it == g_scenes.end()) {
						problem = std::format("entry {}: no scene \"{}\"", index, sceneID);
						break;
					}
					const auto& scene = it->second;
					if (!seq.entries.empty() && scene.furniture != seq.furniture) {
						problem = std::format("entry {}: \"{}\" is played on other furniture than the scenes before it", index, sceneID);
						break;
					}
					if (seq.entries.empty()) {
						seq.actorCount = scene.actors.size();
						seq.actors = scene.actors;
						seq.furniture = scene.furniture;
					} else if (scene.actors.size() != seq.actorCount) {
						problem = std::format("entry {}: \"{}\" has {} actor(s), the sequence {}", index, sceneID, scene.actors.size(), seq.actorCount);
						break;
					} else {
						bool clash = false;
						for (std::size_t role = 0; role < seq.actors.size(); ++role) {
							auto&      have = seq.actors[role].sex;
							const auto want = scene.actors[role].sex;
							if (have == Sex::kAny) {
								have = want;
							} else if (want != Sex::kAny && want != have) {
								clash = true;
							}
						}
						if (clash) {
							problem = std::format("entry {}: \"{}\" asks for a different sex in a role than the scenes before it", index, sceneID);
							break;
						}
					}
					float duration = scene.length;
					if (e.is_object() && e.contains("duration")) {
						if (!e["duration"].is_number() || e["duration"].get<float>() <= 0.0F) {
							problem = std::format("entry {}: \"duration\" must be a positive number of seconds", index);
							break;
						}
						duration = e["duration"].get<float>();
					}
					if (duration <= 0.0F) {
						problem = std::format("entry {}: needs a \"duration\" (scene \"{}\" has no \"length\")", index, sceneID);
						break;
					}
					int speed = e.is_object() ? e.value("speed", 1) - 1 : 0;
					speed = std::clamp(speed, 0, static_cast<int>(scene.speeds.size()) - 1);
					seq.entries.push_back({ scene.id, duration, speed });
				}
				if (!problem.empty()) {
					REX::WARN("Scenes: {}: sequence \"{}\" skipped: {}", seq.sourceFile, seq.id, problem);
					continue;
				}
				const auto key = Lower(seq.id);
				if (const auto existing = a_out.find(key); existing != a_out.end()) {
					REX::WARN("Scenes: {}: sequence \"{}\" replaces the one from {}", seq.sourceFile, seq.id, existing->second->sourceFile);
				}
				a_out[key] = std::make_shared<const Sequence>(std::move(seq));
				++kept;
			}
			g_rawSequences.clear();
			return kept;
		}

		void Publish()
		{
			std::unordered_map<std::string, std::shared_ptr<const Scene>> published;
			for (auto& [key, scene] : g_scenes) {
				published.emplace(key, std::make_shared<const Scene>(std::move(scene)));
			}
			g_scenes.clear();
			g_published = std::move(published);
		}

		int LoadLocked()
		{
			g_scenes.clear();
			g_rawSequences.clear();
			g_loaded = true;

			std::error_code ec;
			if (!std::filesystem::is_directory(SCENE_FOLDER, ec)) {
				REX::INFO("Scenes: no {} folder, no scenes loaded", SCENE_FOLDER);
				g_sequences.clear();
				Publish();  // drops any scenes from before
				return 0;
			}

			std::vector<std::filesystem::path> files;
			for (const auto& e : std::filesystem::directory_iterator(SCENE_FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					files.push_back(e.path());
				}
			}
			std::ranges::sort(files);  // deterministic: later files win on duplicate IDs

			int loaded = 0;
			int skipped = 0;
			for (const auto& f : files) {
				LoadFile(f, loaded, skipped);
			}

			// Navigation links must point at a loaded scene with the same
			// number of actors (roles carry over when navigating).
			int dropped = 0;
			for (auto& [key, scene] : g_scenes) {
				std::erase_if(scene.navigations, [&](const Navigation& a_nav) {
					const auto dest = g_scenes.find(Lower(a_nav.to));
					const bool good = dest != g_scenes.end() && dest->second.actors.size() == scene.actors.size();
					if (!good) {
						REX::WARN("Scenes: {}: scene \"{}\" navigation to \"{}\" dropped (no such scene, or a different number of actors)",
							scene.sourceFile, scene.id, a_nav.to);
						++dropped;
					}
					return !good;
				});
			}
			// A transition moves on to its destination by itself, so it needs
			// a real destination with the same actors, and a length.
			int brokenTransitions = 0;
			for (auto& [key, scene] : g_scenes) {
				if (!scene.IsTransition()) {
					continue;
				}
				const auto dest = g_scenes.find(Lower(scene.destination));
				std::string problem;
				if (dest == g_scenes.end()) {
					problem = "no such scene";
				} else if (dest->second.actors.size() != scene.actors.size()) {
					problem = "a different number of actors";
				} else if (scene.length <= 0.0F) {
					problem = "the scene has no \"length\"";
				}
				if (!problem.empty()) {
					REX::WARN("Scenes: {}: transition \"{}\" -> \"{}\" ignored ({}); it plays like a normal scene",
						scene.sourceFile, scene.id, scene.destination, problem);
					scene.destination.clear();
					++brokenTransitions;
				} else {
					scene.destination = dest->second.id;  // the ID's real spelling
				}
			}

			std::unordered_map<std::string, std::shared_ptr<const Sequence>> sequences;
			const int sequenceCount = BuildSequences(sequences);
			g_sequences = std::move(sequences);

			REX::INFO("Scenes: loaded {} scene(s) and {} sequence(s) from {} file(s), {} skipped, {} navigation(s) dropped, {} transition(s) ignored (see warnings above)",
				loaded, sequenceCount, files.size(), skipped, dropped, brokenTransitions);
			Publish();
			return loaded;
		}
	}

	int Reload()
	{
		std::scoped_lock lock(g_lock);
		return LoadLocked();
	}

	std::shared_ptr<const Scene> Find(std::string_view a_id)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto it = g_published.find(Lower(a_id));
		return it != g_published.end() ? it->second : nullptr;
	}

	std::shared_ptr<const Scene> Settled(std::shared_ptr<const Scene> a_scene)
	{
		for (int hop = 0; a_scene && a_scene->IsTransition() && hop < 8; ++hop) {
			auto next = Find(a_scene->destination);
			if (!next) {
				break;
			}
			a_scene = std::move(next);
		}
		return a_scene;
	}

	std::shared_ptr<const Sequence> FindSequence(std::string_view a_id)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto it = g_sequences.find(Lower(a_id));
		return it != g_sequences.end() ? it->second : nullptr;
	}

	Sex SexOf(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return Sex::kAny;
		}
		switch (a_actor->GetSex()) {
		case RE::SEX::kMale:
			return Sex::kMale;
		case RE::SEX::kFemale:
			return Sex::kFemale;
		default:
			return Sex::kAny;
		}
	}

	bool Fits(std::span<const SceneActor> a_roles, std::span<const Sex> a_actors)
	{
		if (a_roles.size() != a_actors.size()) {
			return false;
		}
		for (std::size_t i = 0; i < a_roles.size(); ++i) {
			const auto want = a_roles[i].sex;
			const auto have = a_actors[i];
			if (want != Sex::kAny && have != Sex::kAny && want != have) {
				return false;
			}
		}
		return true;
	}

	std::vector<std::size_t> AssignRoles(std::span<const SceneActor> a_roles, std::span<const Sex> a_actors)
	{
		if (a_roles.size() != a_actors.size()) {
			return {};
		}
		std::vector<std::size_t> order(a_actors.size());
		for (std::size_t i = 0; i < order.size(); ++i) {
			order[i] = i;
		}
		// Every order, starting with the actors' own (two actors: two orders).
		do {
			std::vector<Sex> ordered;
			for (const auto index : order) {
				ordered.push_back(a_actors[index]);
			}
			if (Fits(a_roles, ordered)) {
				return order;
			}
		} while (std::ranges::next_permutation(order).found);
		return {};
	}

	namespace
	{
		bool Passes(std::span<const SceneActor> a_roles, const ListFilter& a_filter)
		{
			if (a_filter.sexes.empty()) {
				return true;
			}
			return a_filter.fixedOrder ? Fits(a_roles, a_filter.sexes) : !AssignRoles(a_roles, a_filter.sexes).empty();
		}
	}

	std::vector<SceneSummary> ListSequences(std::size_t a_actorCount, const ListFilter& a_filter)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		std::vector<SceneSummary> out;
		for (const auto& [key, ptr] : g_sequences) {
			if (ptr->actorCount != a_actorCount || !Passes(ptr->actors, a_filter)) {
				continue;
			}
			std::string tags;
			for (const auto& tag : ptr->tags) {
				tags += tags.empty() ? tag : ", " + tag;
			}
			out.push_back({ ptr->id, ptr->name, std::move(tags), ptr->furniture });
		}
		std::ranges::sort(out, [](const SceneSummary& a, const SceneSummary& b) { return Lower(a.name) < Lower(b.name); });
		return out;
	}

	std::vector<SceneSummary> List(std::size_t a_actorCount, const ListFilter& a_filter)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		std::vector<SceneSummary> out;
		for (const auto& [key, ptr] : g_published) {
			const auto& scene = *ptr;
			if (scene.actors.size() != a_actorCount || !Passes(scene.actors, a_filter)) {
				continue;
			}
			std::string tags;
			for (const auto& tag : scene.tags) {
				tags += tags.empty() ? tag : ", " + tag;
			}
			out.push_back({ scene.id, scene.name, std::move(tags), scene.furniture });
		}
		std::ranges::sort(out, [](const SceneSummary& a, const SceneSummary& b) { return Lower(a.name) < Lower(b.name); });
		return out;
	}
}
