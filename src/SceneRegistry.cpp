#include "SceneRegistry.h"

#include "Actions.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace SceneRegistry
{
	namespace
	{
		constexpr auto SCENE_FOLDER = "Data/F4SE/Plugins/4Stim/Scenes";        // OStim: SKSE/Plugins/OStim/scenes
		constexpr auto SEQUENCE_FOLDER = "Data/F4SE/Plugins/4Stim/Sequences";  // OStim: SKSE/Plugins/OStim/sequences

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

		// A navigation as read, attached to its origin scene once every scene
		// is in: OStim's "origin" can link another scene to this one.
		struct RawNavigation
		{
			std::string origin;  // scene it's offered in
			Navigation  nav;
			std::string file;
		};
		std::vector<RawNavigation> g_rawNavigations;

		// Idles by editor ID (lowercase), for OStim's "animation" speeds.
		// Built on first use in each load.
		std::unordered_map<std::string, RE::TESIdleForm*> g_idlesByEditorID;
		bool                                              g_idlesIndexed = false;

		RE::TESIdleForm* IdleByEditorID(const std::string& a_editorID)
		{
			if (!g_idlesIndexed) {
				g_idlesIndexed = true;
				g_idlesByEditorID.clear();
				if (const auto handler = RE::TESDataHandler::GetSingleton()) {
					for (const auto idle : handler->GetFormArray<RE::TESIdleForm>()) {
						if (idle && !idle->formEditorID.empty()) {
							g_idlesByEditorID[Lower(idle->formEditorID.c_str())] = idle;
						}
					}
				}
				REX::INFO("Scenes: {} Idle(s) with an editor ID, for OStim-style \"animation\" names", g_idlesByEditorID.size());
			}
			const auto it = g_idlesByEditorID.find(Lower(a_editorID));
			return it != g_idlesByEditorID.end() ? it->second : nullptr;
		}

		std::vector<std::string> StringList(const nlohmann::json& a_obj, const char* a_key, bool a_lower)
		{
			std::vector<std::string> out;
			if (const auto it = a_obj.find(a_key); it != a_obj.end() && it->is_array()) {
				for (const auto& v : *it) {
					if (v.is_string()) {
						auto value = a_lower ? Lower(v.get<std::string>()) : v.get<std::string>();
						if (std::ranges::find(out, value) == out.end()) {
							out.push_back(std::move(value));
						}
					}
				}
			}
			return out;
		}

		// OStim's position object: {"x", "y", "z", "r"}, all optional.
		Position ReadPosition(const nlohmann::json& a_value)
		{
			Position pos;
			if (!a_value.is_object()) {
				return pos;
			}
			auto number = [&](const char* a_key) {
				const auto it = a_value.find(a_key);
				return it != a_value.end() && it->is_number() ? it->get<float>() : 0.0F;
			};
			pos.x = number("x");
			pos.y = number("y");
			pos.z = number("z");
			pos.r = number("r");
			return pos;
		}

		// A navigation object: 4Stim's {"to", "label", "icon"} or OStim's
		// {"destination", "origin", "description", "icon", "border",
		// "priority", "noWarnings"}. a_self is the scene it's written in.
		std::optional<RawNavigation> ReadNavigation(const nlohmann::json& a_value, const std::string& a_self, const std::string& a_file)
		{
			if (!a_value.is_object()) {
				return std::nullopt;
			}
			RawNavigation raw;
			raw.file = a_file;
			raw.origin = a_value.value("origin", a_self);
			raw.nav.to = a_value.value("to", a_value.value("destination", a_self));
			if (Lower(raw.origin) == Lower(raw.nav.to)) {
				return std::nullopt;  // neither an origin nor a destination
			}
			raw.nav.label = a_value.value("label", a_value.value("description", std::string{}));
			raw.nav.icon = a_value.value("icon", std::string{});
			raw.nav.border = a_value.value("border", std::string{ "ffffff" });
			if (const auto it = a_value.find("priority"); it != a_value.end() && it->is_number_integer()) {
				raw.nav.priority = it->get<int>();
			}
			raw.nav.noWarnings = a_value.value("noWarnings", false);
			return raw;
		}

		// One scene from its JSON object. a_ostim: from an OStim-style file
		// (one scene per file, id = file name), whose tags OStim lowercases.
		void ParseScene(const nlohmann::json& entry, const std::string& id, const std::string& file, const std::string& filePlugin, bool a_ostim,
			int& a_loaded, int& a_skipped)
		{
			Scene scene;
			scene.id = id;
			scene.name = entry.value("name", id);
			scene.icon = entry.value("icon", std::string{});
			scene.modpack = entry.value("modpack", std::string{});
			scene.fadeOnEntry = entry.value("fadeOnEntry", false);
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
			// OStim's scene "offset" moves every actor (added to each one's own).
			Position sceneOffset;
			if (const auto off = entry.find("offset"); off != entry.end()) {
				sceneOffset = ReadPosition(*off);
			}
			if (const auto length = entry.find("length"); length != entry.end()) {
				if (length->is_number() && length->get<float>() > 0.0F) {
					scene.length = length->get<float>();
				} else {
					REX::WARN("Scenes: {}: scene \"{}\" \"length\" must be a positive number of seconds, ignored", file, id);
				}
			}
			scene.sourceFile = file;
			scene.noRandomSelection = entry.value("noRandomSelection", false);
			scene.noStrip = entry.value("noStrip", false);
			if (const auto at = entry.find("dressAt"); at != entry.end() && at->is_number()) {
				scene.dressAt = std::max(at->get<float>(), 0.0F);
			}
			if (const auto ds = entry.find("defaultSpeed"); ds != entry.end()) {
				if (ds->is_number_integer() && ds->get<int>() >= 0) {
					scene.defaultSpeed = ds->get<int>();
				} else {
					REX::WARN("Scenes: {}: scene \"{}\" \"defaultSpeed\" must be a speed index (0 = slowest), ignored", file, id);
				}
			}
			scene.tags = StringList(entry, "tags", a_ostim);
			if (const auto auto_ = entry.find("autoTransitions"); auto_ != entry.end() && auto_->is_object()) {
				for (const auto& [event, dest] : auto_->items()) {
					if (dest.is_string() && !dest.get<std::string>().empty()) {
						scene.autoTransitions.emplace_back(Lower(event), dest.get<std::string>());
					}
				}
			}

			const auto actors = entry.find("actors");
			if (actors == entry.end() || !actors->is_array() || actors->empty()) {
				REX::WARN("Scenes: {}: scene \"{}\" has no \"actors\", skipped", file, id);
				++a_skipped;
				return;
			}
			std::vector<int> animationIndex;  // per role: the "_n" of OStim's per-actor animation name
			for (const auto& a : *actors) {
				// "sex", or OStim's "intendedSex".
				auto sexValue = std::string{ "any" };
				if (a.is_object()) {
					sexValue = a.value("sex", a.value("intendedSex", std::string{ "any" }));
				}
				const auto sex = Lower(sexValue);
				SceneActor role;
				int        index = static_cast<int>(scene.actors.size());
				if (a.is_object()) {
					role.tags = StringList(a, "tags", true);
					role.requirements = StringList(a, "requirements", true);
					// "autoTransitions": {"climax": id, ...}; "climax": id is short for the climax one.
					if (const auto auto_ = a.find("autoTransitions"); auto_ != a.end() && auto_->is_object()) {
						for (const auto& [event, dest] : auto_->items()) {
							if (dest.is_string() && !dest.get<std::string>().empty()) {
								role.autoTransitions.emplace_back(Lower(event), dest.get<std::string>());
							}
						}
					}
					if (const auto climax = a.value("climax", std::string{}); !climax.empty() && role.AutoTransition("climax").empty()) {
						role.autoTransitions.emplace_back("climax", climax);
					}
					if (const auto off = a.find("offset"); off != a.end()) {
						role.offset = ReadPosition(*off);
					}
					for (const auto key : { "penisBend", "sosBend", "tngBend" }) {
						if (const auto bend = a.find(key); bend != a.end() && bend->is_number()) {
							role.penisBend = bend->get<float>();
							break;
						}
					}
					if (const auto ai = a.find("animationIndex"); ai != a.end() && ai->is_number_integer()) {
						index = ai->get<int>();
					}
				}
				role.offset.x += sceneOffset.x;
				role.offset.y += sceneOffset.y;
				role.offset.z += sceneOffset.z;
				role.offset.r += sceneOffset.r;
				if (sex == "male" || sex == "m") {
					role.sex = Sex::kMale;
				} else if (sex == "female" || sex == "f") {
					role.sex = Sex::kFemale;
				} else if (sex != "any" && sex != "agender" && !sex.empty()) {
					REX::WARN("Scenes: {}: scene \"{}\": unknown \"sex\" \"{}\" (male, female or any), taken as any", file, id, sex);
				}
				scene.actors.push_back(role);
				animationIndex.push_back(index);
			}
			const auto roles = scene.actors.size();
			if (const auto list = entry.find("undress"); list != entry.end()) {
				for (const auto& step : list->is_array() ? *list : nlohmann::json::array()) {
					SceneUndress undress;
					const auto   role = step.is_object() ? step.find("actor") : step.end();
					const auto   slots = step.is_object() ? step.find("slots") : step.end();
					if (role == step.end() || !role->is_number_integer() || role->get<int>() < 0 || static_cast<std::size_t>(role->get<int>()) >= scene.actors.size() ||
						slots == step.end() || !slots->is_array()) {
						REX::WARN("Scenes: {}: scene \"{}\" has an \"undress\" step without a valid \"actor\" role and \"slots\" list, ignored", file, id);
						continue;
					}
					undress.actor = static_cast<std::size_t>(role->get<int>());
					for (const auto& slot : *slots) {
						if (slot.is_number_integer() && slot.get<int>() >= 30 && slot.get<int>() <= 61) {
							undress.slots.push_back(slot.get<int>());
						}
					}
					undress.at = std::max(step.value("at", 0.0F), 0.0F);
					scene.undress.push_back(std::move(undress));
				}
			}

			// Idles: a "speeds" list, each speed either 4Stim's {"idles": [one
			// per role]} or OStim's {"animation": name}, whose Idles are the
			// ones with editor ID name_0, name_1... (one per role); or the
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
					SpeedInfo   info;
					if (const auto v = speed.find("playbackSpeed"); v != speed.end() && v->is_number()) {
						info.playbackSpeed = v->get<float>();
					}
					if (const auto v = speed.find("displaySpeed"); v != speed.end() && v->is_number()) {
						info.displaySpeed = v->get<float>();
					}
					std::vector<RE::TESIdleForm*> set;
					if (const auto animation = speed.find("animation"); animation != speed.end() && animation->is_string()) {
						const auto name = animation->get<std::string>();
						for (std::size_t role = 0; role < roles; ++role) {
							const auto editorID = name + "_" + std::to_string(animationIndex[role]);
							const auto idle = IdleByEditorID(editorID);
							if (!idle) {
								REX::WARN("Scenes: {}: scene \"{}\" speed {} role {}: no Idle with the editor ID \"{}\" (OStim's \"animation\" names each actor's Idle name_0, name_1...)",
									file, id, sp + 1, role, editorID);
								ok = false;
								break;
							}
							set.push_back(idle);
						}
					} else {
						const auto idles = speed.find("idles");
						if (idles == speed.end() || !idles->is_array() || idles->size() != roles) {
							REX::WARN("Scenes: {}: scene \"{}\" speed {} needs \"idles\" with one entry per actor ({}), or an OStim \"animation\"", file, id, sp + 1, roles);
							ok = false;
							break;
						}
						const auto plugin = speed.value("plugin", filePlugin);
						for (std::size_t role = 0; role < roles; ++role) {
							const auto idle = resolve((*idles)[role], plugin, sp, role);
							if (!idle) {
								ok = false;
								break;
							}
							set.push_back(idle);
						}
					}
					scene.speeds.push_back(std::move(set));
					scene.speedInfo.push_back(info);
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
				scene.speedInfo.push_back({});
			}
			if (!ok) {
				++a_skipped;
				return;
			}

			// Actions: who does what to whom. Each role collects what its
			// side of its actions needs (Fits checks it).
			if (const auto acts = entry.find("actions"); acts != entry.end() && acts->is_array()) {
				for (const auto& a : *acts) {
					const auto typeID = a.is_object() ? a.value("type", std::string{}) : std::string{};
					auto       type = Actions::Find(typeID);
					if (!type) {
						REX::WARN("Scenes: {}: scene \"{}\": unknown action \"{}\" ignored (not in any Actions file)", file, id, typeID);
						continue;
					}
					auto role = [&](const char* a_key, int a_default) {
						const auto v = a.find(a_key);
						return v != a.end() && v->is_number_integer() ? v->get<int>() : a_default;
					};
					const int actor = role("actor", 0);
					const int target = role("target", actor);
					const int performer = role("performer", actor);
					const auto inRange = [&](int a_role) { return a_role >= 0 && static_cast<std::size_t>(a_role) < roles; };
					if (!inRange(actor) || !inRange(target) || !inRange(performer)) {
						REX::WARN("Scenes: {}: scene \"{}\": action \"{}\" names a role the scene doesn't have (0 to {}), ignored", file, id, typeID, roles - 1);
						continue;
					}
					auto need = [&](int a_role, const Actions::Side& a_side) {
						auto& reqs = scene.actors[static_cast<std::size_t>(a_role)].requirements;
						for (const auto& r : a_side.requirements) {
							if (std::ranges::find(reqs, r) == reqs.end()) {
								reqs.push_back(r);
							}
						}
					};
					need(actor, type->actor);
					need(target, type->target);
					need(performer, type->performer);
					scene.actions.push_back({ std::move(type), static_cast<std::size_t>(actor), static_cast<std::size_t>(target), static_cast<std::size_t>(performer) });
				}
			}

			// Navigations. As OStim: a transition's own "navigations" don't
			// count (it moves on to its destination); with an "origin" it's
			// offered in that scene, labelled by its "description".
			if (scene.IsTransition()) {
				if (const auto origin = entry.find("origin"); origin != entry.end() && origin->is_string()) {
					nlohmann::json nav = { { "origin", origin->get<std::string>() }, { "destination", id } };
					for (const auto key : { "description", "icon", "border", "priority", "noWarnings" }) {
						if (const auto it = entry.find(key); it != entry.end()) {
							nav[key] = *it;
						}
					}
					if (auto raw = ReadNavigation(nav, id, file)) {
						g_rawNavigations.push_back(std::move(*raw));
					}
				}
			} else if (const auto navs = entry.find("navigations"); navs != entry.end() && navs->is_array()) {
				for (const auto& n : *navs) {
					if (auto raw = ReadNavigation(n, id, file)) {
						g_rawNavigations.push_back(std::move(*raw));
					} else {
						REX::WARN("Scenes: {}: scene \"{}\" has a navigation with neither a destination (\"to\") nor an origin, ignored", file, id);
					}
				}
			}

			// OStim's automatic tags.
			auto addTag = [&](const char* a_tag) {
				if (std::ranges::none_of(scene.tags, [&](const std::string& t) { return Lower(t) == a_tag; })) {
					scene.tags.emplace_back(a_tag);
				}
			};
			if (scene.IsTransition()) {
				addTag("transition");
			}
			if (scene.actors.size() >= 2) {
				if (std::ranges::all_of(scene.actors, [](const SceneActor& r) { return r.sex == Sex::kMale; })) {
					addTag("gay");
				} else if (std::ranges::all_of(scene.actors, [](const SceneActor& r) { return r.sex == Sex::kFemale; })) {
					addTag("lesbian");
				}
			}

			scene.defaultSpeed = std::min(scene.defaultSpeed, static_cast<int>(scene.speeds.size()) - 1);

			const auto key = Lower(id);
			if (const auto existing = g_scenes.find(key); existing != g_scenes.end()) {
				REX::WARN("Scenes: {}: scene \"{}\" replaces the one from {}", file, id, existing->second.sourceFile);
			}
			g_scenes[key] = std::move(scene);
			++a_loaded;
		}

		// A sequence's entries and tags, as read (checked once every scene is in).
		void AddRawSequence(const nlohmann::json& a_entry, const std::string& a_id, const std::string& a_file)
		{
			const auto list = a_entry.is_object() ? a_entry.find("scenes") : a_entry.end();
			if (a_id.empty() || list == a_entry.end() || !list->is_array() || list->empty()) {
				REX::WARN("Scenes: {}: a sequence needs an id and a non-empty \"scenes\" list, skipped", a_file);
				return;
			}
			RawSequence raw;
			raw.sequence.id = a_id;
			raw.sequence.name = a_entry.value("name", a_id);
			raw.sequence.sourceFile = a_file;
			raw.sequence.tags = StringList(a_entry, "tags", false);
			raw.entries = *list;
			g_rawSequences.push_back(std::move(raw));
		}

		std::optional<nlohmann::json> ReadJSON(const std::filesystem::path& a_path, const char* a_what)
		{
			try {
				std::ifstream in(a_path);
				return nlohmann::json::parse(in, nullptr, true, true);  // comments allowed
			} catch (const std::exception& e) {
				REX::WARN("{}: {}: not valid JSON: {}", a_what, a_path.filename().string(), e.what());
				return std::nullopt;
			}
		}

		// A file under Scenes\: 4Stim's pack layout ("scenes" / "sequences"
		// lists), or OStim's one-scene-per-file layout (id = file name).
		void LoadFile(const std::filesystem::path& a_path, int& a_loaded, int& a_skipped)
		{
			const auto file = a_path.filename().string();
			const auto parsed = ReadJSON(a_path, "Scenes");
			if (!parsed) {
				++a_skipped;
				return;
			}
			const auto& root = *parsed;
			if (!root.is_object()) {
				REX::WARN("Scenes: {}: not a JSON object, skipped", file);
				++a_skipped;
				return;
			}

			const auto scenes = root.find("scenes");
			const auto sequences = root.find("sequences");
			const bool pack = (scenes != root.end() && scenes->is_array()) || (sequences != root.end() && sequences->is_array());
			if (!pack) {
				// OStim's layout: this file is one scene.
				ParseScene(root, a_path.stem().string(), file, root.value("plugin", std::string{}), true, a_loaded, a_skipped);
				return;
			}

			const auto filePlugin = root.value("plugin", std::string{});
			if (sequences != root.end() && sequences->is_array()) {
				for (const auto& entry : *sequences) {
					AddRawSequence(entry, entry.is_object() ? entry.value("id", std::string{}) : std::string{}, file);
				}
			}
			if (scenes == root.end() || !scenes->is_array()) {
				return;
			}
			for (const auto& entry : *scenes) {
				const auto id = entry.is_object() ? entry.value("id", std::string{}) : std::string{};
				if (id.empty()) {
					REX::WARN("Scenes: {}: a scene has no \"id\", skipped", file);
					++a_skipped;
					continue;
				}
				ParseScene(entry, id, file, filePlugin, false, a_loaded, a_skipped);
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
			Actions::Reload();  // scenes name their actions
			g_scenes.clear();
			g_rawSequences.clear();
			g_rawNavigations.clear();
			g_loaded = true;

			std::error_code ec;
			if (!std::filesystem::is_directory(SCENE_FOLDER, ec)) {
				REX::INFO("Scenes: no {} folder, no scenes loaded", SCENE_FOLDER);
				g_sequences.clear();
				Publish();  // drops any scenes from before
				return 0;
			}

			// Every .json under Scenes\ and its subfolders (OStim keeps one
			// scene per file in subfolders of its scenes folder).
			std::vector<std::filesystem::path> files;
			for (const auto& e : std::filesystem::recursive_directory_iterator(SCENE_FOLDER, ec)) {
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
			// OStim's sequences folder: one sequence per file, id = file name.
			for (const auto& e : std::filesystem::directory_iterator(SEQUENCE_FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					if (const auto root = ReadJSON(e.path(), "Sequences")) {
						AddRawSequence(*root, e.path().stem().string(), e.path().filename().string());
					}
				}
			}

			// Navigation links go to their origin scene, and must point at a
			// loaded scene with the same number of actors (roles carry over
			// when navigating). Then each scene's are sorted by priority, as
			// OStim does (stable: file order otherwise).
			int dropped = 0;
			for (auto& raw : g_rawNavigations) {
				const auto origin = g_scenes.find(Lower(raw.origin));
				const auto dest = g_scenes.find(Lower(raw.nav.to));
				if (origin == g_scenes.end() || dest == g_scenes.end() || dest->second.actors.size() != origin->second.actors.size()) {
					REX::WARN("Scenes: {}: navigation \"{}\" -> \"{}\" dropped (no such scene, or a different number of actors)", raw.file, raw.origin, raw.nav.to);
					++dropped;
					continue;
				}
				raw.nav.to = dest->second.id;
				if (raw.nav.label.empty()) {
					raw.nav.label = dest->second.name;
				}
				origin->second.navigations.push_back(std::move(raw.nav));
			}
			g_rawNavigations.clear();
			for (auto& [key, scene] : g_scenes) {
				std::ranges::stable_sort(scene.navigations, {}, &Navigation::priority);
			}
			g_idlesByEditorID.clear();
			g_idlesIndexed = false;
			// Auto transitions (climax scenes...): a loaded scene with the same actors.
			for (auto& [key, scene] : g_scenes) {
				std::erase_if(scene.autoTransitions, [&](auto& a_entry) {
					const auto dest = g_scenes.find(Lower(a_entry.second));
					if (dest == g_scenes.end() || dest->second.actors.size() != scene.actors.size()) {
						REX::WARN("Scenes: {}: scene \"{}\" {} scene \"{}\" dropped (no such scene, or a different number of actors)",
							scene.sourceFile, scene.id, a_entry.first, a_entry.second);
						return true;
					}
					a_entry.second = dest->second.id;
					return false;
				});
				for (auto& role : scene.actors) {
					std::erase_if(role.autoTransitions, [&](auto& a_entry) {
						const auto dest = g_scenes.find(Lower(a_entry.second));
						if (dest == g_scenes.end() || dest->second.actors.size() != scene.actors.size()) {
							REX::WARN("Scenes: {}: scene \"{}\" {} scene \"{}\" dropped (no such scene, or a different number of actors)",
								scene.sourceFile, scene.id, a_entry.first, a_entry.second);
							return true;
						}
						a_entry.second = dest->second.id;
						return false;
					});
				}
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

			const auto withActions = std::ranges::count_if(g_scenes, [](const auto& a_entry) { return !a_entry.second.actions.empty(); });
			REX::INFO("Scenes: loaded {} scene(s) and {} sequence(s) from {} file(s), {} skipped, {} navigation(s) dropped, {} transition(s) ignored (see warnings above); {} scene(s) have actions",
				loaded, sequenceCount, files.size(), skipped, dropped, brokenTransitions, withActions);
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
			for (const auto& req : a_roles[i].requirements) {
				if (!Actions::Provides(have, req)) {
					return false;
				}
			}
		}
		return true;
	}

	bool SceneActor::HasTag(std::string_view a_tag) const
	{
		return std::ranges::find(tags, Lower(a_tag)) != tags.end();
	}

	std::string SceneActor::AutoTransition(std::string_view a_event) const
	{
		const auto event = Lower(a_event);
		const auto it = std::ranges::find_if(autoTransitions, [&](const auto& a_entry) { return a_entry.first == event; });
		return it != autoTransitions.end() ? it->second : std::string{};
	}

	std::string Scene::AutoTransition(std::string_view a_event) const
	{
		const auto event = Lower(a_event);
		const auto it = std::ranges::find_if(autoTransitions, [&](const auto& a_entry) { return a_entry.first == event; });
		return it != autoTransitions.end() ? it->second : std::string{};
	}

	bool Scene::HasAction(std::string_view a_type) const
	{
		const auto type = Actions::Find(a_type);
		return type && std::ranges::any_of(actions, [&](const SceneAction& a) { return a.type->id == type->id; });
	}

	bool Scene::HasActionTag(std::string_view a_tag) const
	{
		return std::ranges::any_of(actions, [&](const SceneAction& a) { return a.type->HasTag(a_tag); });
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
			std::string names;
			std::vector<const Actions::Type*> seen;
			for (const auto& action : scene.actions) {
				if (std::ranges::find(seen, action.type.get()) == seen.end()) {
					seen.push_back(action.type.get());
					names += names.empty() ? action.type->name : ", " + action.type->name;
				}
			}
			out.push_back({ scene.id, scene.name, std::move(tags), scene.furniture, std::move(names), scene.modpack });
		}
		std::ranges::sort(out, [](const SceneSummary& a, const SceneSummary& b) { return Lower(a.name) < Lower(b.name); });
		return out;
	}
}
