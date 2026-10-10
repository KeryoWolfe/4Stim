#include "Alignment.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

#include <nlohmann/json.hpp>

namespace Alignment
{
	namespace
	{
		constexpr auto FILE_PATH = "Data/F4SE/Plugins/4Stim/Alignment.json";

		std::mutex                                       g_lock;
		bool                                             g_loaded = false;
		std::map<std::string, std::map<std::size_t, Offset>> g_offsets;  // lowercase scene id -> role -> offset

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_offsets.clear();
			std::ifstream in(FILE_PATH);
			if (!in) {
				return;  // nothing aligned yet
			}
			try {
				const auto root = nlohmann::json::parse(in, nullptr, true, true);
				const auto scenes = root.find("scenes");
				if (scenes == root.end() || !scenes->is_object()) {
					return;
				}
				std::size_t count = 0;
				for (const auto& [id, roles] : scenes->items()) {
					if (!roles.is_object()) {
						continue;
					}
					for (const auto& [role, value] : roles.items()) {
						if (!value.is_object()) {
							continue;
						}
						Offset offset;
						offset.x = value.value("x", 0.0F);
						offset.y = value.value("y", 0.0F);
						offset.z = value.value("z", 0.0F);
						offset.rot = value.value("rot", 0.0F);
						offset.scale = std::clamp(value.value("scale", 1.0F), 0.1F, 10.0F);
						g_offsets[Lower(id)][static_cast<std::size_t>(std::stoul(role))] = offset;
						++count;
					}
				}
				REX::INFO("Alignment: {} offset(s) for {} scene(s)", count, g_offsets.size());
			} catch (const std::exception& e) {
				REX::WARN("Alignment: {}: couldn't be read ({}); starting with no offsets, and it'll be rewritten on the next change", FILE_PATH, e.what());
				g_offsets.clear();
			}
		}

		void SaveLocked()
		{
			nlohmann::json scenes = nlohmann::json::object();
			for (const auto& [id, roles] : g_offsets) {
				nlohmann::json out = nlohmann::json::object();
				for (const auto& [role, o] : roles) {
					if (!o.IsZero()) {
						out[std::to_string(role)] = { { "x", o.x }, { "y", o.y }, { "z", o.z }, { "rot", o.rot }, { "scale", o.scale } };
					}
				}
				if (!out.empty()) {
					scenes[id] = std::move(out);
				}
			}
			nlohmann::json root = {
				{ "_comment", "4Stim alignment, set in game from the HUD's Align tab (docs/ALIGNMENT.md). Per scene id and role: x right, y forward, z up (game units), rot degrees, scale." },
				{ "scenes", std::move(scenes) }
			};
			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(FILE_PATH).parent_path(), ec);
			const auto temp = std::string(FILE_PATH) + ".tmp";
			{
				std::ofstream out(temp, std::ios::trunc);
				if (!out) {
					REX::WARN("Alignment: couldn't write {}", temp);
					return;
				}
				out << root.dump(1, '\t') << '\n';
			}
			std::filesystem::rename(temp, FILE_PATH, ec);
			if (ec) {
				REX::WARN("Alignment: couldn't replace {} ({})", FILE_PATH, ec.message());
			}
		}
	}

	Offset Get(std::string_view a_sceneID, std::size_t a_role)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto scene = g_offsets.find(Lower(a_sceneID));
		if (scene == g_offsets.end()) {
			return {};
		}
		const auto role = scene->second.find(a_role);
		return role != scene->second.end() ? role->second : Offset{};
	}

	void Set(std::string_view a_sceneID, std::size_t a_role, const Offset& a_offset)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		g_offsets[Lower(a_sceneID)][a_role] = a_offset;
		SaveLocked();
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}
}
