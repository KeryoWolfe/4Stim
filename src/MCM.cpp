#include "MCM.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"

namespace MCM
{
	namespace
	{
		constexpr auto BASE = "Data/F4SE/Plugins/4Stim.ini";
		constexpr auto DEFAULTS = "Data/MCM/Config/4Stim/settings.ini";
		constexpr auto USER = "Data/MCM/Settings/4Stim.ini";

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::string Trim(std::string_view a_text)
		{
			const auto b = a_text.find_first_not_of(" \t\r");
			const auto e = a_text.find_last_not_of(" \t\r");
			return b == std::string_view::npos ? std::string{} : std::string(a_text.substr(b, e - b + 1));
		}

		// The settings that are a choice of words: MCM's dropdown index for them.
		struct Choice
		{
			std::string_view              key;  // 4Stim.ini's, lowercase
			std::vector<std::string_view> options;
		};
		const std::array<Choice, 5> CHOICES{ {
			{ "stargetmode", { "Crosshair", "Proximity" } },
			{ "shudcontrols", { "Tabs", "Columns" } },
			{ "stheme", { "Color", "PipBoy" } },
			{ "smalelight", { "nolight", "facelight", "facelightdim", "crotchlight", "crotchlightdim", "asslight", "asslightdim" } },
			{ "sfemalelight", { "nolight", "facelight", "facelightdim", "crotchlight", "crotchlightdim", "asslight", "asslightdim" } },
		} };

		const Choice* FindChoice(std::string_view a_iniKey)
		{
			const auto key = Lower(a_iniKey);
			const auto it = std::ranges::find_if(CHOICES, [&](const Choice& c) { return c.key == key; });
			return it != CHOICES.end() ? &*it : nullptr;
		}

		// An INI file, in order: sections of key / value pairs.
		struct Ini
		{
			std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> sections;

			static Ini Read(const std::filesystem::path& a_path)
			{
				Ini           ini;
				std::ifstream in(a_path);
				std::string   line;
				std::string   section;
				while (std::getline(in, line)) {
					line = Trim(line);
					if (line.empty() || line[0] == ';' || line[0] == '#') {
						continue;
					}
					if (line.front() == '[' && line.back() == ']') {
						section = line.substr(1, line.size() - 2);
						continue;
					}
					const auto eq = line.find('=');
					if (eq == std::string::npos) {
						continue;
					}
					auto value = Trim(line.substr(eq + 1));
					if (const auto c = value.find(';'); c != std::string::npos) {
						value = Trim(value.substr(0, c));
					}
					ini.Set(section, Trim(line.substr(0, eq)), value);
				}
				return ini;
			}

			std::vector<std::pair<std::string, std::string>>* Section(std::string_view a_name, bool a_create)
			{
				const auto name = Lower(a_name);
				for (auto& [section, entries] : sections) {
					if (Lower(section) == name) {
						return &entries;
					}
				}
				if (!a_create) {
					return nullptr;
				}
				sections.emplace_back(std::string(a_name), std::vector<std::pair<std::string, std::string>>{});
				return &sections.back().second;
			}

			std::optional<std::string> Get(std::string_view a_section, std::string_view a_key)
			{
				const auto entries = Section(a_section, false);
				const auto key = Lower(a_key);
				if (entries) {
					for (const auto& [k, v] : *entries) {
						if (Lower(k) == key) {
							return v;
						}
					}
				}
				return std::nullopt;
			}

			void Set(std::string_view a_section, std::string_view a_key, std::string_view a_value)
			{
				auto&      entries = *Section(a_section, true);
				const auto key = Lower(a_key);
				for (auto& [k, v] : entries) {
					if (Lower(k) == key) {
						v = a_value;
						return;
					}
				}
				entries.emplace_back(std::string(a_key), std::string(a_value));
			}

			void Erase(std::string_view a_section, std::string_view a_key)
			{
				if (const auto entries = Section(a_section, false)) {
					const auto key = Lower(a_key);
					std::erase_if(*entries, [&](const auto& a_entry) { return Lower(a_entry.first) == key; });
				}
			}

			bool Write(const std::filesystem::path& a_path) const
			{
				std::error_code ec;
				std::filesystem::create_directories(a_path.parent_path(), ec);
				std::ofstream out(a_path, std::ios::trunc);
				if (!out) {
					return false;
				}
				for (const auto& [section, entries] : sections) {
					if (entries.empty()) {
						continue;
					}
					out << '[' << section << "]\n";
					for (const auto& [k, v] : entries) {
						out << k << '=' << v << '\n';
					}
					out << '\n';
				}
				return true;
			}
		};

		// The 4Stim.ini key MCM's a_mcmKey stands for (as written), or "" if
		// it isn't a 4Stim one.
		std::string IniKeyOf(std::string_view a_mcmKey)
		{
			if (a_mcmKey.size() > 4 && std::string_view("bifs").find(a_mcmKey[0]) != std::string_view::npos && Lower(a_mcmKey.substr(1, 3)) == "4s_") {
				return std::string(a_mcmKey.substr(4));
			}
			return std::string(a_mcmKey);
		}

		// The same value? Numbers compare as numbers (0x4E = 78, 1.0 = 1).
		bool SameValue(const std::string& a_a, const std::string& a_b)
		{
			auto number = [](const std::string& a_text) -> std::optional<double> {
				try {
					std::size_t used = 0;
					const double value = a_text.starts_with("0x") || a_text.starts_with("0X") ? static_cast<double>(std::stoll(a_text, &used, 16)) : std::stod(a_text, &used);
					return used == a_text.size() ? std::optional<double>(value) : std::nullopt;
				} catch (...) {
					return std::nullopt;
				}
			};
			const auto x = number(a_a), y = number(a_b);
			if (x && y) {
				return std::fabs(*x - *y) < 1e-6;
			}
			return Lower(a_a) == Lower(a_b);
		}

		// a_iniValue as MCM's file holds it for a_mcmKey (a choice: its index).
		std::string ToMCMValue(std::string_view a_mcmKey, const std::string& a_iniValue)
		{
			if (const auto choice = a_mcmKey.starts_with("i4S_") || a_mcmKey.starts_with("i4s_") ? FindChoice(IniKeyOf(a_mcmKey)) : nullptr) {
				const auto value = Lower(a_iniValue);
				for (std::size_t i = 0; i < choice->options.size(); ++i) {
					if (Lower(choice->options[i]) == value) {
						return std::to_string(i);
					}
				}
				return "0";
			}
			// Booleans as MCM writes them.
			if (a_mcmKey.starts_with("b")) {
				return SameValue(a_iniValue, "0") || Lower(a_iniValue) == "false" ? "0" : "1";
			}
			return a_iniValue;
		}

		std::filesystem::path ExportPath()
		{
			wchar_t*   buffer = nullptr;
			const auto result = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(buffer));
			std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> known(buffer, REX::W32::CoTaskMemFree);
			if (!known || result != 0) {
				return "Data/F4SE/Plugins/4Stim/mcmsettings.json";
			}
			auto                  folder = F4SE::GetSaveFolderName();
			std::filesystem::path path = known.get();
			path /= "My Games";
			path /= folder.empty() ? std::string("Fallout4") : std::string(folder);
			path /= "4Stim";
			path /= "mcmsettings.json";
			return path;
		}

		// Copies 4Stim.ini's values into a_user for every setting MCM shows:
		// set where they differ from MCM's defaults, dropped where they don't.
		std::size_t CopyBase(Ini& a_user)
		{
			auto        base = Ini::Read(BASE);
			auto        defaults = Ini::Read(DEFAULTS);
			std::size_t changed = 0;
			for (const auto& [section, entries] : defaults.sections) {
				for (const auto& [mcmKey, defaultValue] : entries) {
					const auto iniValue = base.Get(section, IniKeyOf(mcmKey));
					if (!iniValue) {
						continue;
					}
					const auto value = ToMCMValue(mcmKey, *iniValue);
					if (SameValue(value, defaultValue)) {
						a_user.Erase(section, mcmKey);
					} else {
						a_user.Set(section, mcmKey, value);
						++changed;
					}
				}
			}
			return changed;
		}
	}

	std::filesystem::path SettingsPath()
	{
		return USER;
	}

	bool Installed()
	{
		std::error_code ec;
		return std::filesystem::exists(DEFAULTS, ec);
	}

	std::optional<std::pair<std::string, std::string>> ToIni(std::string_view a_mcmKey, std::string_view a_value)
	{
		const auto iniKey = Lower(IniKeyOf(a_mcmKey));
		if (iniKey.empty()) {
			return std::nullopt;
		}
		if (const auto choice = Lower(a_mcmKey.substr(0, 4)) == "i4s_" ? FindChoice(iniKey) : nullptr) {
			try {
				const auto index = static_cast<std::size_t>(std::stoul(std::string(a_value)));
				return std::pair{ iniKey, std::string(choice->options[std::min(index, choice->options.size() - 1)]) };
			} catch (...) {
				return std::nullopt;
			}
		}
		return std::pair{ iniKey, std::string(a_value) };
	}

	void Sync()
	{
		if (!Installed()) {
			return;
		}
		std::error_code ec;
		const auto      baseTime = std::filesystem::last_write_time(BASE, ec);
		if (ec) {
			return;
		}
		const bool userExists = std::filesystem::exists(USER, ec);
		const auto userTime = userExists ? std::filesystem::last_write_time(USER, ec) : std::filesystem::file_time_type::min();
		if (userExists && !ec && userTime >= baseTime) {
			REX::INFO("MCM: its settings are newer than 4Stim.ini: they stay on top");
			return;
		}
		auto       user = Ini::Read(USER);
		const auto changed = CopyBase(user);
		if (user.Write(USER)) {
			REX::INFO("MCM: 4Stim.ini is newer: its values copied into the MCM's ({} differ from the MCM's defaults)", changed);
		} else {
			REX::WARN("MCM: couldn't write {}", USER);
		}
	}

	bool Export()
	{
		auto base = Ini::Read(BASE);
		auto user = Ini::Read(USER);
		for (const auto& [section, entries] : user.sections) {
			for (const auto& [mcmKey, value] : entries) {
				if (const auto ini = ToIni(mcmKey, value)) {
					// Back under the name 4Stim.ini uses (the case it's written in).
					auto name = IniKeyOf(mcmKey);
					base.Set(section, name, ini->second);
				}
			}
		}
		nlohmann::json out = nlohmann::json::object();
		for (const auto& [section, entries] : base.sections) {
			for (const auto& [key, value] : entries) {
				out[section][key] = value;
			}
		}
		const auto      path = ExportPath();
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream file(path, std::ios::trunc);
		if (!file) {
			REX::WARN("MCM: couldn't write {}", path.string());
			return false;
		}
		file << out.dump(2) << '\n';
		REX::INFO("MCM: settings exported to {}", path.string());
		return true;
	}

	bool Import()
	{
		const auto path = ExportPath();
		std::ifstream in(path);
		if (!in) {
			REX::WARN("MCM: no {} to import", path.string());
			return false;
		}
		nlohmann::json json;
		try {
			json = nlohmann::json::parse(in, nullptr, true, true);
		} catch (const std::exception& e) {
			REX::WARN("MCM: {} isn't valid JSON ({})", path.string(), e.what());
			return false;
		}
		// Each value under MCM's name for it where MCM shows it, else under
		// 4Stim.ini's (4Stim reads both from MCM's file).
		auto defaults = Ini::Read(DEFAULTS);
		auto user = Ini::Read(USER);
		std::size_t count = 0;
		for (const auto& [section, entries] : json.items()) {
			if (!entries.is_object()) {
				continue;
			}
			for (const auto& [key, value] : entries.items()) {
				const auto text = value.is_string() ? value.get<std::string>() : value.dump();
				std::string mcmKey = key;
				if (const auto list = defaults.Section(section, false)) {
					for (const auto& [name, unused] : *list) {
						if (Lower(IniKeyOf(name)) == Lower(key)) {
							mcmKey = name;
							break;
						}
					}
				}
				user.Set(section, mcmKey, ToMCMValue(mcmKey, text));
				++count;
			}
		}
		if (!user.Write(USER)) {
			REX::WARN("MCM: couldn't write {}", USER);
			return false;
		}
		REX::INFO("MCM: {} setting(s) imported from {}", count, path.string());
		return true;
	}

	void UseIni()
	{
		Ini user;
		CopyBase(user);
		if (user.Write(USER)) {
			REX::INFO("MCM: back to 4Stim.ini's values");
		}
	}
}
