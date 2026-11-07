#include "MCM.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <fstream>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
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
		const std::array<Choice, 20> CHOICES{ {
			{ "stargetmode", { "Crosshair", "Proximity" } },
			{ "shudcontrols", { "Tabs", "Columns" } },
			{ "stheme", { "Color", "PipBoy" } },
			{ "smalelight", { "nolight", "facelight", "facelightdim", "crotchlight", "crotchlightdim", "asslight", "asslightdim" } },
			{ "sfemalelight", { "nolight", "facelight", "facelightdim", "crotchlight", "crotchlightdim", "asslight", "asslightdim" } },
			{ "splayerlight", { "default", "nolight", "facelight", "facelightdim", "crotchlight", "crotchlightdim", "asslight", "asslightdim" } },
			{ "spadscheme", { "Modern", "Classic" } },
			{ "stoysynchronization", { "player", "male", "female", "scene", "individual" } },
			{ "stoyscaling", { "none", "rate", "value", "speed", "individual" } },
			// Controller buttons ([Controller]).
			{ "spadhotkey", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadnpcscene", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadspeedup", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadspeeddown", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadautomode", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadpullout", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadend", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadsearch", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadalign", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadfreecam", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
			{ "spadhideui", { "None", "LB", "RB", "LT", "RT", "X", "Y", "LS", "RS", "Back" } },
		} };

		// OStim's settings in milliseconds (4Stim.ini has them so, as OStim):
		// the MCM shows them in seconds, as "f4S_<name>". Lowercase.
		const std::array<std::string_view, 10> MILLISECONDS{
			"setexpressiondurationmin",
			"setexpressiondurationmax",
			"moanintervalmin",
			"moanintervalmax",
			"npcsceneduration",
			"excitementdecaygraceperiod",
			"automodeanimdurationmin",
			"automodeanimdurationmax",
			"autospeedcontrolintervalmin",
			"autospeedcontrolintervalmax",
		};

		bool InMilliseconds(std::string_view a_iniKey)
		{
			const auto key = Lower(a_iniKey);
			return std::ranges::find(MILLISECONDS, key) != MILLISECONDS.end();
		}

		std::string FormatNumber(double a_value)
		{
			auto text = std::format("{:.3f}", a_value);
			while (text.ends_with('0')) {
				text.pop_back();
			}
			if (text.ends_with('.')) {
				text.pop_back();
			}
			return text;
		}

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
			// Milliseconds in 4Stim.ini, seconds in the MCM.
			if ((a_mcmKey.starts_with("f4S_") || a_mcmKey.starts_with("f4s_")) && InMilliseconds(IniKeyOf(a_mcmKey))) {
				try {
					return FormatNumber(std::stod(a_iniValue) / 1000.0);
				} catch (...) {
					return a_iniValue;
				}
			}
			// Booleans as MCM writes them.
			if (a_mcmKey.starts_with("b")) {
				return SameValue(a_iniValue, "0") || Lower(a_iniValue) == "false" ? "0" : "1";
			}
			// A key: MCM's key input holds "keycode,modifiers" ("38,0").
			if (a_mcmKey.starts_with("s4S_") || a_mcmKey.starts_with("s4s_")) {
				try {
					return std::to_string(std::stoll(a_iniValue, nullptr, 0)) + ",0";
				} catch (...) {
					return "0,0";
				}
			}
			// Whole numbers in decimal (4Stim.ini may write hex).
			if (a_mcmKey.starts_with("i") && (a_iniValue.starts_with("0x") || a_iniValue.starts_with("0X"))) {
				try {
					return std::to_string(std::stoll(a_iniValue, nullptr, 16));
				} catch (...) {
				}
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
		// A key from MCM's key input: "keycode,modifiers" -> the keycode.
		if (Lower(a_mcmKey.substr(0, std::min<std::size_t>(4, a_mcmKey.size()))) == "s4s_") {
			const auto text = std::string(a_value);
			try {
				return std::pair{ iniKey, std::to_string(std::stoll(text.substr(0, text.find(',')))) };
			} catch (...) {
				return std::nullopt;
			}
		}
		if (InMilliseconds(iniKey)) {
			const auto prefix = Lower(a_mcmKey.substr(0, std::min<std::size_t>(4, a_mcmKey.size())));
			if (prefix == "i4s_") {
				return std::nullopt;  // an older MCM page's milliseconds: its seconds one replaced it
			}
			if (prefix == "f4s_") {
				try {
					return std::pair{ iniKey, std::to_string(std::llround(std::stod(std::string(a_value)) * 1000.0)) };
				} catch (...) {
					return std::nullopt;
				}
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

	namespace
	{
		constexpr auto EXPORT_MARK = "4StimExport";  // 4Stim's own files (OStim's have no such key)

		// A value as the JSON OStim writes (numbers as numbers; its globals
		// are floats): 4Stim.ini's hex key codes as their number, the rest
		// as text.
		nlohmann::json ExportValue(const std::string& a_value)
		{
			try {
				std::size_t used = 0;
				if (a_value.starts_with("0x") || a_value.starts_with("0X")) {
					const auto number = std::stoll(a_value, &used, 16);
					if (used == a_value.size()) {
						return number;
					}
				} else if (!a_value.empty()) {
					const auto number = std::stod(a_value, &used);
					if (used == a_value.size()) {
						return number == std::floor(number) && std::fabs(number) < 1e15 ? nlohmann::json(static_cast<std::int64_t>(number)) : nlohmann::json(number);
					}
				}
			} catch (...) {
			}
			return a_value;
		}

		std::string ImportText(const nlohmann::json& a_value)
		{
			if (a_value.is_string()) {
				return a_value.get<std::string>();
			}
			if (a_value.is_boolean()) {
				return a_value.get<bool>() ? "1" : "0";
			}
			if (a_value.is_number()) {
				const auto number = a_value.get<double>();
				return number == std::floor(number) && std::fabs(number) < 1e15 ? std::to_string(static_cast<std::int64_t>(number)) : FormatNumber(number);
			}
			return a_value.dump();
		}
	}

	// OStim's mcmsettings.json layout (MCMTable::exportSettings): one flat
	// object, each setting under its name. 4Stim.ini uses OStim's names
	// wherever OStim has the setting, so they're OStim's keys; 4Stim's own
	// settings sit alongside under theirs. Plus "4StimExport", so an import
	// can tell 4Stim's file from OStim's.
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
		nlohmann::ordered_json out = nlohmann::ordered_json::object();
		out[EXPORT_MARK] = 1;
		for (const auto& [section, entries] : base.sections) {
			for (const auto& [key, value] : entries) {
				out[key] = ExportValue(value);
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
		REX::INFO("MCM: {} setting(s) exported to {}", out.size() - 1, path.string());
		return true;
	}

	// Reads OStim's flat layout (4Stim's export, or OStim's own file copied
	// over) and 4Stim's older one ({ "<ini section>": { ... } }). From an
	// OStim file, keys (Skyrim's DirectX scan codes, not Fallout 4's key
	// codes) and dropdowns (OStim's own numbering) are left out, and so is
	// what 4Stim has no setting for; the log says how many. Not carried, as
	// OStim's "actorData": the per-actor choices (they're in the save).
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
		if (!json.is_object()) {
			REX::WARN("MCM: {} isn't a settings file", path.string());
			return false;
		}
		auto defaults = Ini::Read(DEFAULTS);
		auto base = Ini::Read(BASE);
		auto user = Ini::Read(USER);
		// Where a_key goes: MCM's name for it and its section where MCM
		// shows it, else 4Stim.ini's section; nullopt if 4Stim has no such
		// setting.
		auto place = [&](const std::string& a_key, const std::string& a_section) -> std::optional<std::pair<std::string, std::string>> {
			const auto key = Lower(a_key);
			for (const auto& [section, entries] : defaults.sections) {
				if (!a_section.empty() && Lower(section) != Lower(a_section)) {
					continue;
				}
				for (const auto& [name, unused] : entries) {
					if (Lower(IniKeyOf(name)) == key) {
						return std::pair{ section, name };
					}
				}
			}
			for (const auto& [section, entries] : base.sections) {
				for (const auto& [name, unused] : entries) {
					if (Lower(name) == key) {
						return std::pair{ a_section.empty() ? section : a_section, name };
					}
				}
			}
			if (!a_section.empty()) {
				return std::pair{ a_section, a_key };  // 4Stim's older layout: as it was written
			}
			return std::nullopt;
		};
		bool sectioned = false;  // 4Stim's older layout
		for (auto it = json.begin(); it != json.end(); ++it) {
			sectioned = sectioned || it->is_object();
		}
		const bool  ostim = !json.contains(EXPORT_MARK) && !sectioned;
		std::size_t count = 0, unknown = 0, keys = 0;
		auto take = [&](const std::string& a_key, const nlohmann::json& a_value, const std::string& a_section) {
			if (a_value.is_object() || a_value.is_array() || a_value.is_null()) {
				return;
			}
			const auto where = place(a_key, a_section);
			if (!where) {
				++unknown;
				return;
			}
			const auto& [section, mcmKey] = *where;
			const auto prefix = Lower(mcmKey.substr(0, std::min<std::size_t>(4, mcmKey.size())));
			if (ostim && (prefix == "s4s_" || prefix == "i4s_")) {
				++keys;
				return;
			}
			user.Set(section, mcmKey, ToMCMValue(mcmKey, ImportText(a_value)));
			++count;
		};
		for (const auto& [key, value] : json.items()) {
			if (key == EXPORT_MARK) {
				continue;
			}
			if (value.is_object()) {
				for (const auto& [inner, innerValue] : value.items()) {
					take(inner, innerValue, key);
				}
			} else {
				take(key, value, {});
			}
		}
		if (count == 0) {
			REX::WARN("MCM: nothing in {} is a 4Stim setting: nothing imported", path.string());
			return false;
		}
		if (!user.Write(USER)) {
			REX::WARN("MCM: couldn't write {}", USER);
			return false;
		}
		REX::INFO("MCM: {} setting(s) imported from {}{}{}", count, path.string(), ostim ? " (OStim's file)" : "",
			unknown + keys > 0 ? std::format("; left out: {} 4Stim doesn't have, {} key(s) and dropdown(s) from OStim", unknown, keys) : std::string{});
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
