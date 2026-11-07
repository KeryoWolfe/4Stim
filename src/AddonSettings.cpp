#include "AddonSettings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace AddonSettings
{
	namespace
	{
		constexpr auto FOLDER = "Data/F4SE/Plugins/4Stim/Settings";  // OStim: SKSE\Plugins\OStim\settings
		constexpr auto CONFIG = "Data/MCM/Config/4Stim/config.json";
		constexpr auto PAGE_NAME = "$4stim_page_addons";  // OStim's $ostim_page_addons, "Add-Ons"

		using json = nlohmann::ordered_json;

		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		// OStim's form fields ({"mod": "MyMod.esp", "formid": "0x800"}) as
		// MCM's sourceForm ("MyMod.esp|800": the form's own ID in its plugin,
		// in hex). Empty if they're not usable.
		std::string SourceForm(const json& a_variable)
		{
			if (!a_variable.is_object() || !a_variable.contains("mod") || !a_variable["mod"].is_string() || !a_variable.contains("formid")) {
				return {};
			}
			std::uint32_t id = 0;
			try {
				const auto& formid = a_variable["formid"];
				if (formid.is_string()) {
					id = static_cast<std::uint32_t>(std::stoul(formid.get<std::string>(), nullptr, 16));
				} else if (formid.is_number_unsigned() || formid.is_number_integer()) {
					id = formid.get<std::uint32_t>();
				} else {
					return {};
				}
			} catch (...) {
				return {};
			}
			const auto mod = a_variable["mod"].get<std::string>();
			// A light plugin's forms are 12 bits; others 24.
			const auto ext = Lower(std::filesystem::path(mod).extension().string());
			id &= ext == ".esl" ? 0xFFFu : 0xFFFFFFu;
			return mod.empty() || id == 0 ? std::string{} : std::format("{}|{:X}", mod, id);
		}

		float Number(const json& a_setting, const char* a_key, float a_default)
		{
			const auto it = a_setting.find(a_key);
			return it != a_setting.end() && it->is_number() ? it->get<float>() : a_default;
		}

		// One of OStim's settings as an MCM control, or null if it's not
		// usable (logged, as OStim's warnings).
		json Control(const json& a_setting, const std::string& a_file, std::size_t a_index)
		{
			const auto where = std::format("{}, setting {}", a_file, a_index);
			if (!a_setting.is_object()) {
				REX::WARN("Add-on settings: {} is malformed", where);
				return nullptr;
			}
			const auto name = a_setting.value("name", std::string{});
			auto       type = Lower(a_setting.value("type", std::string{}));
			if (name.empty() || type.empty()) {
				REX::WARN("Add-on settings: {} has no \"name\" or \"type\"", where);
				return nullptr;
			}
			const auto form = SourceForm(a_setting.value("variable", json::object()));
			if (form.empty()) {
				REX::WARN("Add-on settings: {} (\"{}\"): its \"variable\" isn't a form (\"mod\" and \"formid\")", where, name);
				return nullptr;
			}
			json control = json::object();
			control["text"] = name;
			if (const auto tooltip = a_setting.value("tooltip", std::string{}); !tooltip.empty()) {
				control["help"] = tooltip;
			}
			json options = json::object();
			if (type == "toggle") {
				control["type"] = "switcher";
				options["defaultValue"] = a_setting.value("defaultValue", false) ? 1 : 0;
			} else if (type == "slider") {
				control["type"] = "slider";
				options["min"] = Number(a_setting, "minValue", 0.0F);
				options["max"] = Number(a_setting, "maxValue", 0.0F);
				options["step"] = Number(a_setting, "valueStep", 1.0F);
				options["defaultValue"] = Number(a_setting, "defaultValue", 0.0F);
			} else if (type == "dropdown") {
				control["type"] = "dropdown";
				json list = json::array();
				if (const auto it = a_setting.find("options"); it != a_setting.end() && it->is_array()) {
					for (const auto& option : *it) {
						if (option.is_string()) {
							list.push_back(option);
						}
					}
				}
				if (list.empty()) {
					REX::WARN("Add-on settings: {} (\"{}\"): a dropdown with no \"options\"", where, name);
					return nullptr;
				}
				options["options"] = std::move(list);
				options["defaultValue"] = a_setting.value("defaultIndex", 0);
			} else if (type == "keymap") {
				// OStim's keymap: the key's code in the global. MCM's key input
				// gives Fallout 4's key codes (the add-on reads the global, as
				// with OStim; Skyrim's codes from an OStim add-on differ).
				control["type"] = "keyinput";
				options["allowModifierKeys"] = false;
				options["defaultValue"] = a_setting.value("defaultKey", 0);
			} else {
				REX::WARN("Add-on settings: {} (\"{}\"): unknown type \"{}\"", where, name, type);
				return nullptr;
			}
			options["sourceType"] = "GlobalValue";
			options["sourceForm"] = form;
			control["valueOptions"] = std::move(options);
			return control;
		}

		bool IsAddonPage(const json& a_page)
		{
			return a_page.is_object() && a_page.value("pageDisplayName", std::string{}) == PAGE_NAME;
		}
	}

	std::size_t WritePage()
	{
		std::error_code ec;
		if (!std::filesystem::exists(CONFIG, ec)) {
			return 0;  // no MCM page installed
		}
		// The add-ons' groups, by file name (OStim's order: the folder's).
		std::vector<std::filesystem::path> files;
		for (const auto& e : std::filesystem::directory_iterator(FOLDER, ec)) {
			if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
				files.push_back(e.path());
			}
		}
		std::ranges::sort(files);
		json        content = json::array();
		std::size_t count = 0;
		for (const auto& path : files) {
			const auto file = path.filename().string();
			try {
				std::ifstream in(path);
				const auto    root = json::parse(in, nullptr, true, true);
				const auto    group = root.is_object() ? root.value("name", std::string{}) : std::string{};
				if (group.empty()) {
					REX::WARN("Add-on settings: {} has no \"name\"", file);
					continue;
				}
				const auto settings = root.find("settings");
				if (settings == root.end() || !settings->is_array()) {
					REX::WARN("Add-on settings: {} has no \"settings\" list", file);
					continue;
				}
				json controls = json::array();
				for (std::size_t i = 0; i < settings->size(); ++i) {
					if (auto control = Control((*settings)[i], file, i); !control.is_null()) {
						controls.push_back(std::move(control));
					}
				}
				if (controls.empty()) {
					continue;
				}
				content.push_back({ { "type", "section" }, { "text", group } });
				for (auto& control : controls) {
					content.push_back(std::move(control));
				}
				count += controls.size();
				REX::INFO("Add-on settings: {} ({}): {} setting(s)", group, file, controls.size());
			} catch (const std::exception& ex) {
				REX::WARN("Add-on settings: {} couldn't be read ({})", file, ex.what());
			}
		}

		// The config, with the page put in (or taken out); written only when
		// that changes it.
		std::string before;
		json        config;
		try {
			std::ifstream     in(CONFIG, std::ios::binary);
			std::stringstream text;
			text << in.rdbuf();
			before = text.str();
			config = json::parse(before);
		} catch (const std::exception& ex) {
			REX::WARN("Add-on settings: couldn't read {} ({}); no Add-Ons page", CONFIG, ex.what());
			return count;
		}
		if (!config.is_object() || !config.contains("pages") || !config["pages"].is_array()) {
			REX::WARN("Add-on settings: {} has no pages; no Add-Ons page", CONFIG);
			return count;
		}
		auto& pages = config["pages"];
		bool  hadPage = false;
		for (auto it = pages.begin(); it != pages.end();) {
			if (IsAddonPage(*it)) {
				it = pages.erase(it);
				hadPage = true;
			} else {
				++it;
			}
		}
		if (count == 0 && !hadPage) {
			return 0;  // nothing to add or take out: the file isn't touched
		}
		if (count > 0) {
			json page = json::object();
			page["pageDisplayName"] = PAGE_NAME;
			page["content"] = std::move(content);
			// OStim's Addons page comes after its others; 4Stim's About page stays last.
			auto at = pages.end();
			if (!pages.empty() && pages.back().value("pageDisplayName", std::string{}) == "$4stim_page_about") {
				at = pages.end() - 1;
			}
			pages.insert(at, std::move(page));
		}
		auto after = config.dump(1, '\t');
		after += '\n';
		if (after == before) {
			return count;
		}
		// make-mcm.py writes "\n" line ends, as dump does; a copy saved with
		// "\r\n" differs only in those, so compare without them.
		std::string plain = before;
		std::erase(plain, '\r');
		if (plain == after) {
			return count;
		}
		// In place, not through a temporary file: under Mod Organizer a
		// renamed file would land in its overwrite folder instead of 4Stim's.
		{
			std::ofstream out(CONFIG, std::ios::binary | std::ios::trunc);
			out << after;
			if (!out) {
				REX::WARN("Add-on settings: couldn't write {}", CONFIG);
				return count;
			}
		}
		REX::INFO("Add-on settings: {} {}", count > 0 ? std::format("Add-Ons page written to {} ({} setting(s))", CONFIG, count) : std::string("Add-Ons page taken out of"),
			count > 0 ? std::string{} : CONFIG);
		return count;
	}
}
