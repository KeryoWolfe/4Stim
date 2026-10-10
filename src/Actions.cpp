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

		// OStim's strippingSlots are Skyrim's biped slots; these are the
		// Fallout 4 slots each one covers (docs/UNDRESS.md). Slots Skyrim
		// uses for things Fallout 4 doesn't have (shield, tail, back...)
		// map to nothing.
		std::vector<int> FalloutSlotsFor(int a_skyrimSlot)
		{
			switch (a_skyrimSlot) {
			case 30:  // head
			case 31:  // hair
			case 41:  // long hair
			case 42:  // circlet
				return { 46 };
			case 32:  // body
				return { 33, 36, 39, 40, 41, 44, 45 };
			case 33:  // hands
				return { 34, 35 };
			case 34:  // forearms
			case 57:  // shoulder
			case 58:  // arm (secondary)
			case 59:  // arm (primary)
				return { 37, 38, 42, 43 };
			case 35:  // amulet
			case 45:  // neck
				return { 50 };
			case 36:  // ring
				return { 51 };
			case 37:  // feet: Fallout 4 has no feet slot, shoes are part of the legs
			case 38:  // calves
			case 53:  // leg (primary)
			case 54:  // leg (secondary)
				return { 39, 40, 44, 45 };
			case 44:  // face / mouth
			case 55:  // face (alternate)
				return { 49 };
			case 46:  // chest (primary)
			case 56:  // chest (secondary)
				return { 33, 36, 41 };
			case 49:  // pelvis (primary)
			case 52:  // pelvis (secondary)
				return { 33, 36, 39, 40, 44, 45 };
			default:
				return {};
			}
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
			side.fullStrip = it->value("fullStrip", false);
			if (const auto v = it->find("undressSlots"); v != it->end() && v->is_array()) {
				for (const auto& slot : *v) {
					if (slot.is_number_integer() && slot.get<int>() >= 30 && slot.get<int>() <= 61) {
						side.undressSlots.push_back(slot.get<int>());
					}
				}
			} else if (const auto skyrim = it->find("strippingSlots"); skyrim != it->end() && skyrim->is_array()) {
				// OStim's field: Skyrim slots, mapped to Fallout 4's.
				for (const auto& slot : *skyrim) {
					if (!slot.is_number_integer()) {
						continue;
					}
					for (const int fo4 : FalloutSlotsFor(slot.get<int>())) {
						if (std::ranges::find(side.undressSlots, fo4) == side.undressSlots.end()) {
							side.undressSlots.push_back(fo4);
						}
					}
				}
			}
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

		void AddType(const nlohmann::json& entry, const std::string& id, const std::string& file,
			std::unordered_map<std::string, std::shared_ptr<const Type>>& a_out, std::vector<std::string>& a_ids, int& a_loaded);

		// A file under Actions\: 4Stim's layout (an "actions" list) or
		// OStim's (one action per file, its id the file name).
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
			if (!root.is_object()) {
				REX::WARN("Actions: {}: not a JSON object", file);
				return;
			}
			const auto list = root.find("actions");
			if (list == root.end() || !list->is_array()) {
				// OStim's layout: this file is one action, its id the file name.
				AddType(root, Lower(a_path.stem().string()), file, a_out, a_ids, a_loaded);
				return;
			}
			for (const auto& entry : *list) {
				const auto id = entry.is_object() ? Lower(entry.value("id", std::string{})) : std::string{};
				if (id.empty()) {
					REX::WARN("Actions: {}: an action has no \"id\", skipped", file);
					continue;
				}
				AddType(entry, id, file, a_out, a_ids, a_loaded);
			}
		}

		void AddType(const nlohmann::json& entry, const std::string& id, const std::string& file,
			std::unordered_map<std::string, std::shared_ptr<const Type>>& a_out, std::vector<std::string>& a_ids, int& a_loaded)
		{
			{
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
					if (alias == id) {
						continue;
					}
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
