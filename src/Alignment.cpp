#include "Alignment.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>

#include <nlohmann/json.hpp>

#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"

namespace Alignment
{
	namespace
	{
		// 4Stim's file before it took OStim's layout: per scene and role,
		// for everyone. Read once and converted (see Convert).
		constexpr auto OLD_FILE_PATH = "Data/F4SE/Plugins/4Stim/Alignment.json";

		Config g_config;

		std::mutex g_lock;
		bool       g_loaded = false;
		// actor set key -> lowercase scene id -> role -> offset
		std::map<std::string, std::map<std::string, std::map<std::size_t, Offset>>> g_offsets;
		// lowercase scene id -> the id as written (OStim writes scene ids as they are)
		std::map<std::string, std::string> g_sceneNames;

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		// Documents\My Games\Fallout4\4Stim\alignment.json, as OStim keeps
		// its alignment.json in Documents\My Games\Skyrim Special Edition\OStim.
		std::filesystem::path FilePath()
		{
			wchar_t*   buffer = nullptr;
			const auto result = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(buffer));
			std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> known(buffer, REX::W32::CoTaskMemFree);
			if (!known || result != 0) {
				REX::WARN("Alignment: couldn't find the Documents folder; using Data\\F4SE\\Plugins\\4Stim\\alignment.json");
				return "Data/F4SE/Plugins/4Stim/alignment.json";
			}
			auto folder = F4SE::GetSaveFolderName();
			std::filesystem::path path = known.get();
			path /= "My Games";
			path /= folder.empty() ? std::string("Fallout4") : std::string(folder);
			path /= "4Stim";
			path /= "alignment.json";
			return path;
		}

		Offset ReadOffset(const nlohmann::json& a_value)
		{
			Offset offset;
			auto number = [&](const char* a_key, float a_default) {
				const auto it = a_value.find(a_key);
				return it != a_value.end() && it->is_number() ? it->get<float>() : a_default;
			};
			offset.x = number("offsetX", 0.0F);
			offset.y = number("offsetY", 0.0F);
			offset.z = number("offsetZ", 0.0F);
			offset.rot = number("rotation", 0.0F);
			offset.scale = std::clamp(number("scale", 1.0F), 0.1F, 10.0F);
			offset.bend = number("sosBend", number("penisBend", 0.0F));
			return offset;
		}

		void SaveLocked()
		{
			nlohmann::json root = nlohmann::json::object();
			for (const auto& [key, scenes] : g_offsets) {
				nlohmann::json keyOut = nlohmann::json::object();
				for (const auto& [lowerID, roles] : scenes) {
					nlohmann::json sceneOut = nlohmann::json::object();
					for (const auto& [role, o] : roles) {
						if (!o.IsZero()) {
							sceneOut[std::to_string(role)] = { { "offsetX", o.x }, { "offsetY", o.y }, { "offsetZ", o.z }, { "scale", o.scale },
								{ "rotation", o.rot }, { "sosBend", o.bend } };
						}
					}
					if (!sceneOut.empty()) {
						const auto name = g_sceneNames.find(lowerID);
						keyOut[name != g_sceneNames.end() ? name->second : lowerID] = std::move(sceneOut);
					}
				}
				if (!keyOut.empty()) {
					root[key] = std::move(keyOut);
				}
			}
			const auto      path = FilePath();
			std::error_code ec;
			std::filesystem::create_directories(path.parent_path(), ec);
			auto temp = path;
			temp += ".tmp";
			{
				std::ofstream out(temp, std::ios::trunc);
				if (!out) {
					REX::WARN("Alignment: couldn't write {}", temp.string());
					return;
				}
				out << root.dump(2) << '\n';  // as OStim writes it
			}
			std::filesystem::rename(temp, path, ec);
			if (ec) {
				REX::WARN("Alignment: couldn't replace {} ({})", path.string(), ec.message());
			}
		}

		// Every actor set key a scene's roles can have from their intended
		// sexes ("any" roles: both), at height 100 and no heels: where the
		// old one-for-everyone values go.
		std::vector<std::string> KeysForScene(std::string_view a_sceneID)
		{
			const auto scene = SceneRegistry::Find(a_sceneID);
			if (!scene) {
				return {};
			}
			std::vector<std::vector<ActorInfo>> sets{ {} };
			for (const auto& role : scene->actors) {
				std::vector<std::vector<ActorInfo>> next;
				for (const auto& set : sets) {
					const bool any = role.sex == SceneRegistry::Sex::kAny;
					for (const auto sex : { SceneRegistry::Sex::kMale, SceneRegistry::Sex::kFemale }) {
						if (any || role.sex == sex) {
							auto extended = set;
							extended.push_back({ sex, 1.0F, 0.0F });
							next.push_back(std::move(extended));
						}
					}
				}
				sets = std::move(next);
			}
			std::vector<std::string> keys;
			for (const auto& set : sets) {
				if (auto key = KeyFor(set); std::ranges::find(keys, key) == keys.end()) {
					keys.push_back(std::move(key));
				}
			}
			return keys;
		}

		// The old Data\F4SE\Plugins\4Stim\Alignment.json ({"scenes": {id:
		// {role: {x, y, z, rot, scale}}}}), one value for everyone: copied
		// to every actor set its scene's roles can have. Only when the new
		// file doesn't exist yet. Returns how many offsets came over.
		int Convert()
		{
			std::ifstream in(OLD_FILE_PATH);
			if (!in) {
				return 0;
			}
			int count = 0;
			try {
				const auto root = nlohmann::json::parse(in, nullptr, true, true);
				const auto scenes = root.find("scenes");
				if (scenes == root.end() || !scenes->is_object()) {
					return 0;
				}
				for (const auto& [id, roles] : scenes->items()) {
					const auto keys = KeysForScene(id);
					if (keys.empty() || !roles.is_object()) {
						REX::WARN("Alignment: old alignment for \"{}\" not converted (no such scene loaded)", id);
						continue;
					}
					const auto scene = SceneRegistry::Find(id);
					g_sceneNames[Lower(id)] = scene ? scene->id : id;
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
						for (const auto& key : keys) {
							g_offsets[key][Lower(id)][static_cast<std::size_t>(std::stoul(role))] = offset;
						}
						++count;
					}
				}
			} catch (const std::exception& e) {
				REX::WARN("Alignment: {} couldn't be converted ({})", OLD_FILE_PATH, e.what());
				return 0;
			}
			return count;
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_offsets.clear();
			g_sceneNames.clear();
			const auto    path = FilePath();
			std::ifstream in(path);
			if (!in) {
				if (const int converted = Convert(); converted > 0) {
					SaveLocked();
					REX::INFO("Alignment: converted {} offset(s) from {} to OStim's layout in {} (the old file is no longer read)", converted, OLD_FILE_PATH,
						path.string());
				}
				return;
			}
			try {
				const auto  root = nlohmann::json::parse(in, nullptr, true, true);
				std::size_t count = 0;
				for (const auto& [key, scenes] : root.items()) {
					if (!scenes.is_object()) {
						continue;
					}
					for (const auto& [id, roles] : scenes.items()) {
						if (!roles.is_object()) {
							continue;
						}
						g_sceneNames[Lower(id)] = id;
						for (const auto& [role, value] : roles.items()) {
							if (!value.is_object()) {
								continue;
							}
							g_offsets[key][Lower(id)][static_cast<std::size_t>(std::stoul(role))] = ReadOffset(value);
							++count;
						}
					}
				}
				REX::INFO("Alignment: {} offset(s) for {} actor set(s), from {}", count, g_offsets.size(), path.string());
			} catch (const std::exception& e) {
				REX::WARN("Alignment: {} couldn't be read ({}); starting with no offsets, and it'll be rewritten on the next change", path.string(), e.what());
				g_offsets.clear();
			}
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	std::string KeyFor(const std::vector<ActorInfo>& a_actors)
	{
		std::string key;
		for (const auto& actor : a_actors) {
			if (!key.empty()) {
				key += "&";
			}
			// OStim's ActorKey: an actor of unknown sex counts as male, as
			// OStim's isFemale check does.
			const char sex = !g_config.groupBySex ? 'N' : actor.sex == SceneRegistry::Sex::kFemale ? 'F' : 'M';
			const int  height = g_config.groupByHeight ? static_cast<int>(actor.scale * 100.0F) : 100;
			const int  heels = g_config.groupByHeels ? static_cast<int>(actor.heels * 100.0F) : 0;
			key += std::format("{}{}x{}", sex, height, heels);
		}
		return key;
	}

	Offset Get(std::string_view a_key, std::string_view a_sceneID, std::size_t a_role)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		const auto set = g_offsets.find(std::string(a_key));
		if (set == g_offsets.end()) {
			return {};
		}
		const auto scene = set->second.find(Lower(a_sceneID));
		if (scene == set->second.end()) {
			return {};
		}
		const auto role = scene->second.find(a_role);
		return role != scene->second.end() ? role->second : Offset{};
	}

	void Set(std::string_view a_key, std::string_view a_sceneID, std::size_t a_role, const Offset& a_offset)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		g_sceneNames[Lower(a_sceneID)] = std::string(a_sceneID);
		g_offsets[std::string(a_key)][Lower(a_sceneID)][a_role] = a_offset;
		SaveLocked();
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}
}
