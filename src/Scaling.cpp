#include "Scaling.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include "Undress.h"

#include "RE/N/NiFloatExtraData.h"

#include <nlohmann/json.hpp>

namespace Scaling
{
	namespace
	{
		Config g_config;

		constexpr auto HHS_FOLDER = "Data/F4SE/Plugins/HHS";

		std::mutex                             g_lock;
		std::unordered_map<std::string, float> g_heights;  // lowercase mesh path -> High Heels System height (0 = no file)

		// The High Heels System's .json files (Data\F4SE\Plugins\HHS\*.json):
		// heights by mesh path, and by armor form and sex.
		struct FormHeight
		{
			int   gender = 2;  // 0 male, 1 female, 2 (or 3) both
			float value = 0.0F;
		};
		bool                                                      g_jsonLoaded = false;
		std::unordered_map<std::string, float>                    g_jsonMeshes;  // lowercase mesh path (relative to Meshes\)
		std::unordered_map<std::uint32_t, std::vector<FormHeight>> g_jsonForms;  // full form ID

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::string MeshKey(std::string_view a_model)
		{
			auto key = Lower(a_model);
			std::ranges::replace(key, '/', '\\');
			if (key.starts_with("meshes\\")) {
				key = key.substr(7);
			}
			return key;
		}

		// "Height=<value>" in a High Heels System .txt; 0 if none.
		float ReadTxt(const std::filesystem::path& a_path)
		{
			std::ifstream in(a_path);
			std::string   line;
			while (in && std::getline(in, line)) {
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				auto name = Lower(line.substr(0, eq));
				std::erase_if(name, [](unsigned char c) { return std::isspace(c); });
				if (name == "height") {
					auto value = line.substr(eq + 1);
					std::ranges::replace(value, ',', '.');  // "a number with comma" in some files
					try {
						return std::max(std::stof(value), 0.0F);
					} catch (...) {
						return 0.0F;
					}
				}
			}
			return 0.0F;
		}

		// The High Heels System's .json files, as its docs give them:
		// { "<group>": [ {"key": "<mesh path under Meshes>", "value": h} ],
		//   "<Plugin.esp>": [ {"formid": "00800", "gender": 0-3, "value": h} ] }
		// Locked by the caller.
		void LoadJSONLocked()
		{
			g_jsonLoaded = true;
			g_jsonMeshes.clear();
			g_jsonForms.clear();
			const auto      handler = RE::TESDataHandler::GetSingleton();
			std::error_code ec;
			for (const auto& e : std::filesystem::directory_iterator(HHS_FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					for (const auto& [group, list] : root.items()) {
						if (!list.is_array()) {
							continue;
						}
						for (const auto& entry : list) {
							if (!entry.is_object() || !entry.contains("value") || !entry["value"].is_number()) {
								continue;
							}
							const float value = entry["value"].get<float>();
							if (const auto key = entry.find("key"); key != entry.end() && key->is_string()) {
								g_jsonMeshes[MeshKey(key->get<std::string>())] = value;
							} else if (const auto formID = entry.find("formid"); formID != entry.end() && formID->is_string() && handler) {
								const auto local = static_cast<std::uint32_t>(std::stoul(formID->get<std::string>(), nullptr, 16));
								auto       form = handler->LookupForm(local & 0x00FFFFFF, group);
								if (!form) {
									form = handler->LookupForm(local & 0x00000FFF, group);
								}
								if (form) {
									g_jsonForms[form->GetFormID()].push_back({ entry.value("gender", 2), value });
								}
							}
						}
					}
				} catch (const std::exception& ex) {
					REX::WARN("Heels: {}: couldn't be read ({})", e.path().filename().string(), ex.what());
				}
			}
			if (!g_jsonMeshes.empty() || !g_jsonForms.empty()) {
				REX::INFO("Heels: {} mesh(es) and {} armor(s) from the High Heels System's .json files", g_jsonMeshes.size(), g_jsonForms.size());
			}
		}

		// a_model's height (the mesh path as the armor addon gives it,
		// relative to Meshes\): from the High Heels System's .json files, else
		// its .txt with "Height=<units>", next to the mesh
		// (Data\Meshes\...\Shoes.nif -> Shoes.txt) or by the mesh's file name
		// in Data\F4SE\Plugins\HHS\, as the High Heels System's docs give them.
		float HeightFor(const std::string& a_model)
		{
			const auto key = MeshKey(a_model);
			{
				std::scoped_lock lock(g_lock);
				if (!g_jsonLoaded) {
					LoadJSONLocked();
				}
				if (const auto it = g_jsonMeshes.find(key); it != g_jsonMeshes.end()) {
					return it->second;
				}
				if (const auto it = g_heights.find(key); it != g_heights.end()) {
					return it->second;
				}
			}
			std::filesystem::path txt = std::filesystem::path("Data\\Meshes") / key;
			txt.replace_extension(".txt");
			float height = ReadTxt(txt);
			if (height <= 0.0F) {
				height = ReadTxt(std::filesystem::path(HHS_FOLDER) / txt.filename());
			}
			if (height > 0.0F) {
				REX::INFO("Heels: {} -> height {}", a_model, height);
			}
			std::scoped_lock lock(g_lock);
			g_heights[key] = height;
			return height;
		}

		// An armor's height by form ID from the .json files (for a_sex:
		// 0 male, 1 female), or -1 if it has none.
		float HeightForForm(std::uint32_t a_formID, int a_sex)
		{
			std::scoped_lock lock(g_lock);
			if (!g_jsonLoaded) {
				LoadJSONLocked();
			}
			const auto it = g_jsonForms.find(a_formID);
			if (it == g_jsonForms.end()) {
				return -1.0F;
			}
			for (const auto& entry : it->second) {
				if (entry.gender == a_sex || entry.gender >= 2) {
					return entry.value;
				}
			}
			return -1.0F;
		}
		// The High Heels System's mesh extra data: an NiFloatExtraData named
		// "HHS" on a shape (or node) of the shoe's mesh, holding the height
		// (set in NifSkope or Outfit Studio, as HHS's docs show). Read from the
		// actor's loaded third-person model, where worn armor is attached; the
		// highest one counts. 0 if none, or the model isn't loaded.
		float ExtraDataHeight(const RE::NiAVObject* a_object, int a_depth)
		{
			if (!a_object) {
				return 0.0F;
			}
			static const RE::BSFixedString key("HHS");
			float                          best = 0.0F;
			if (const auto extra = netimmerse_cast<RE::NiFloatExtraData*>(a_object->GetExtraData(key))) {
				best = std::max(best, extra->GetValue());
			}
			if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
				for (const auto& child : node->children) {
					best = std::max(best, ExtraDataHeight(child.get(), a_depth - 1));
				}
			}
			return best;
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	float NPCHeight(RE::Actor* a_actor)
	{
		const auto base = a_actor ? a_actor->GetNPC() : nullptr;
		return base && base->height > 0.0F ? base->height : 1.0F;
	}

	float HeelHeight(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return 0.0F;
		}
		const std::size_t sex = a_actor->GetSex() == RE::SEX::kFemale ? 1 : 0;
		float             best = 0.0F;
		for (const auto id : Undress::WornItems(a_actor)) {
			const auto armor = RE::TESForm::GetFormByID<RE::TESObjectARMO>(id);
			if (!armor) {
				continue;
			}
			if (const float byForm = HeightForForm(id, static_cast<int>(sex)); byForm >= 0.0F) {
				best = std::max(best, byForm);
				continue;
			}
			for (const auto& entry : armor->modelArray) {
				const auto addon = entry.armorAddon;
				if (!addon) {
					continue;
				}
				// The .json files may name the armor addon instead of the armor.
				if (const float byForm = HeightForForm(addon->GetFormID(), static_cast<int>(sex)); byForm >= 0.0F) {
					best = std::max(best, byForm);
					continue;
				}
				const char* model = addon->bipedModel[sex].GetModel();
				if (!model || !*model) {
					model = addon->bipedModel[1 - sex].GetModel();
				}
				if (model && *model) {
					best = std::max(best, HeightFor(model));
				}
			}
		}
		// No .json or .txt height: the mesh's own "HHS" extra data.
		if (best <= 0.0F) {
			best = ExtraDataHeight(a_actor->Get3D(false), 64);
			if (best > 0.0F) {
				REX::INFO("Heels: {:08X} -> height {} (mesh extra data)", a_actor->GetFormID(), best);
			}
		}
		return best;
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		g_heights.clear();
		g_jsonLoaded = false;
	}
}
