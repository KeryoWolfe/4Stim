#include "Scaling.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include "Undress.h"

namespace Scaling
{
	namespace
	{
		Config g_config;

		std::mutex                             g_lock;
		std::unordered_map<std::string, float> g_heights;  // lowercase mesh path -> High Heels System height (0 = no file)

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		// Fallout 4's High Heels System keeps a shoe's height in a .txt next
		// to its mesh (Data\Meshes\...\Shoes.nif -> Shoes.txt), as
		// "Height=<units>". a_model is the mesh path as the armor addon gives
		// it (relative to Meshes\).
		float HeightFor(const std::string& a_model)
		{
			auto key = Lower(a_model);
			std::ranges::replace(key, '/', '\\');
			{
				std::scoped_lock lock(g_lock);
				if (const auto it = g_heights.find(key); it != g_heights.end()) {
					return it->second;
				}
			}
			float height = 0.0F;
			std::string path = key;
			if (path.starts_with("meshes\\")) {
				path = path.substr(7);
			}
			if (const auto dot = path.rfind('.'); dot != std::string::npos) {
				path = path.substr(0, dot);
			}
			std::ifstream in("Data\\Meshes\\" + path + ".txt");
			std::string   line;
			while (in && std::getline(in, line)) {
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				auto name = Lower(line.substr(0, eq));
				std::erase_if(name, [](unsigned char c) { return std::isspace(c); });
				if (name == "height") {
					try {
						height = std::max(std::stof(line.substr(eq + 1)), 0.0F);
					} catch (...) {
					}
					break;
				}
			}
			if (height > 0.0F) {
				REX::INFO("Heels: {} -> height {}", a_model, height);
			}
			std::scoped_lock lock(g_lock);
			g_heights[key] = height;
			return height;
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
			for (const auto& entry : armor->modelArray) {
				const auto addon = entry.armorAddon;
				if (!addon) {
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
		return best;
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		g_heights.clear();
	}
}
