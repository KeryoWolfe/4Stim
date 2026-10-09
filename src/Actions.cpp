#include "Actions.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace Actions
{
	namespace
	{
		constexpr auto ACTION_FOLDER = "Data/F4SE/Plugins/4Stim/Actions";

		std::mutex g_lock;
		bool       g_loaded = false;
		// key: lowercase id or alias -> the type (aliases share the pointer)
		std::unordered_map<std::string, std::shared_ptr<const Type>> g_types;
		std::vector<std::string>                                     g_ids;

		std::string Lower(std::string_view a_str)
		{
			std::string out(a_str);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::vector<std::string> LowerList(const nlohmann::json& a_obj, const char* a_key)
		{
			std::vector<std::string> out;
			if (const auto it = a_obj.find(a_key); it != a_obj.end() && it->is_array()) {
				for (const auto& v : *it) {
					if (v.is_string()) {
						out.push_back(Lower(v.get<std::string>()));
					}
				}
			}
			return out;
		}

		Side ParseSide(const nlohmann::json& a_action, const char* a_key)
		{
			Side side;
			const auto it = a_action.find(a_key);
			if (it == a_action.end() || !it->is_object()) {
				return side;
			}
			if (const auto v = it->find("stimulation"); v != it->end() && v->is_number()) {
				side.stimulation = v->get<float>();
			}
			if (const auto v = it->find("maxStimulation"); v != it->end() && v->is_number()) {
				side.maxStimulation = v->get<float>();
			}
			side.requirements = LowerList(*it, "requirements");
			return side;
		}

		// "vaginalsex" -> "Vaginalsex"; files should give a "name".
		std::string DefaultName(const std::string& a_id)
		{
			auto name = a_id;
			if (!name.empty()) {
				name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
			}
			return name;
		}

		void LoadFile(const std::filesystem::path& a_path, std::unordered_map<std::string, std::shared_ptr<const Type>>& a_out,
			std::vector<std::string>& a_ids, int& a_loaded)
		{
			const auto     file = a_path.filename().string();
			nlohmann::json root;
			try {
				std::ifstream in(a_path);
				root = nlohmann::json::parse(in, nullptr, true, true);
			} catch (const std::exception& e) {
				REX::WARN("Actions: {}: not valid JSON: {}", file, e.what());
				return;
			}
			const auto list = root.find("actions");
			if (list == root.end() || !list->is_array()) {
				REX::WARN("Actions: {}: missing an \"actions\" array", file);
				return;
			}
			for (const auto& entry : *list) {
				const auto id = entry.is_object() ? Lower(entry.value("id", std::string{})) : std::string{};
				if (id.empty()) {
					REX::WARN("Actions: {}: an action has no \"id\", skipped", file);
					continue;
				}
				Type type;
				type.id = id;
				type.name = entry.value("name", DefaultName(id));
				type.tags = LowerList(entry, "tags");
				type.actor = ParseSide(entry, "actor");
				type.target = ParseSide(entry, "target");
				type.performer = ParseSide(entry, "performer");
				type.sourceFile = file;

				if (const auto existing = a_out.find(id); existing != a_out.end()) {
					if (existing->second->id == id) {
						REX::WARN("Actions: {}: action \"{}\" replaces the one from {}", file, id, existing->second->sourceFile);
					}
				} else {
					a_ids.push_back(id);
				}
				const auto ptr = std::make_shared<const Type>(std::move(type));
				a_out[id] = ptr;
				for (const auto& alias : LowerList(entry, "aliases")) {
					const auto taken = a_out.find(alias);
					if (taken != a_out.end() && taken->second->id == alias) {
						REX::WARN("Actions: {}: alias \"{}\" of \"{}\" is another action's id, ignored", file, alias, id);
						continue;
					}
					a_out[alias] = ptr;
				}
				++a_loaded;
			}
		}

		void LoadLocked()
		{
			g_loaded = true;
			std::unordered_map<std::string, std::shared_ptr<const Type>> types;
			std::vector<std::string>                                     ids;

			std::error_code ec;
			if (!std::filesystem::is_directory(ACTION_FOLDER, ec)) {
				REX::INFO("Actions: no {} folder, no actions loaded", ACTION_FOLDER);
			} else {
				std::vector<std::filesystem::path> files;
				for (const auto& e : std::filesystem::directory_iterator(ACTION_FOLDER, ec)) {
					if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
						files.push_back(e.path());
					}
				}
				std::ranges::sort(files);  // later files win
				int loaded = 0;
				for (const auto& f : files) {
					LoadFile(f, types, ids, loaded);
				}
				REX::INFO("Actions: loaded {} action type(s) from {} file(s)", ids.size(), files.size());
			}
			std::ranges::sort(ids);
			g_types = std::move(types);
			g_ids = std::move(ids);
		}
	}

	bool Type::HasTag(std::string_view a_tag) const
	{
		const auto tag = Lower(a_tag);
		return std::ranges::find(tags, tag) != tags.end();
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}

	std::shared_ptr<const Type> Find(std::string_view a_idOrAlias)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto it = g_types.find(Lower(a_idOrAlias));
		return it != g_types.end() ? it->second : nullptr;
	}

	std::vector<std::string> List()
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		return g_ids;
	}

	bool Provides(SceneRegistry::Sex a_sex, std::string_view a_requirement)
	{
		using SceneRegistry::Sex;
		const auto req = Lower(a_requirement);
		if (req == "hand" || req == "mouth" || req == "foot" || req == "anus" || req == "nipple") {
			return true;
		}
		if (a_sex == Sex::kAny) {
			return true;
		}
		if (req == "penis" || req == "testicles") {
			return a_sex == Sex::kMale;  // strap-ons will add the penis
		}
		if (req == "vagina" || req == "breast") {
			return a_sex == Sex::kFemale;
		}
		return false;
	}
}
