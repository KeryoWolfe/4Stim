#include "MenuPages.h"

#include "Core/Core.h"
#include "MCM.h"
#include "Translations.h"

// The framework's header includes <windows.h>: after every game header (the
// precompiled ones), and without windows.h's min / max macros.
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
// Its UTF-8 helpers use std::wstring_convert, deprecated since C++17 (C4996),
// and it declares ImGuiTextFilter as a struct, then defines it as a class (C4099).
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4996 4099)
#endif
#include "F4SEMenuFramework.h"
#ifdef _MSC_VER
#	pragma warning(pop)
#endif

#include <array>
#include <fstream>

namespace MenuPages
{
	namespace
	{
		using namespace ImGuiMCP;

		// One control of the MCM's config, as it's drawn here.
		struct Control
		{
			std::string              type;     // section, text, switcher, slider, dropdown, keyinput, button
			std::string              text;     // "$4stim_..." (translated when drawn)
			std::string              help;
			std::string              key;      // MCM's setting name ("b4S_SetEndOnOrgasm"); "" for none
			std::string              section;  // MCM's section ("General")
			std::string              source;   // ModSettingBool / Int / Float / String
			float                    min = 0.0F;
			float                    max = 100.0F;
			float                    step = 1.0F;
			std::vector<std::string> options;   // a dropdown's choices
			std::string              function;  // a button's (FourStimMCM's function)
		};

		struct Page
		{
			std::string          name;  // "$4stim_page_general"
			std::vector<Control> controls;
		};

		std::vector<Page> g_pages;

		// The settings as the MCM shows them, read again when MCM's file of
		// changes was written since (checked twice a second, not every
		// frame), or after a change here.
		std::unordered_map<std::string, std::string> g_values;
		std::filesystem::file_time_type             g_valuesTime{};
		std::chrono::steady_clock::time_point        g_checked{};
		bool                                         g_stale = true;
		std::unordered_map<std::string, float>       g_dragging;  // sliders being dragged: their value till released

		void Refresh()
		{
			const auto now = std::chrono::steady_clock::now();
			if (!g_stale && now - g_checked < std::chrono::milliseconds(500)) {
				return;
			}
			g_checked = now;
			if (const auto time = MCM::SettingsTime(); g_stale || time != g_valuesTime) {
				g_values = MCM::Snapshot();
				g_valuesTime = time;
				g_stale = false;
			}
		}

		std::string Value(const std::string& a_section, const std::string& a_key)
		{
			const auto it = g_values.find(Core::LowerCopy(a_section) + "|" + Core::LowerCopy(a_key));
			return it != g_values.end() ? it->second : std::string{};
		}

		std::string T(const std::string& a_text)
		{
			return !a_text.empty() && a_text.front() == '$' ? Translations::Get(a_text, a_text.substr(1)) : a_text;
		}

		// MCM's config: its pages and their controls (docs/MCM.md).
		bool Load()
		{
			std::ifstream in("Data/MCM/Config/4Stim/config.json");
			if (!in) {
				REX::WARN("Menu Framework: no Data/MCM/Config/4Stim/config.json, so no settings pages");
				return false;
			}
			const auto json = nlohmann::json::parse(in, nullptr, false, true);
			const auto pages = json.is_object() ? json.find("pages") : json.end();
			if (pages == json.end() || !pages->is_array()) {
				REX::WARN("Menu Framework: 4Stim's MCM config has no pages");
				return false;
			}
			for (const auto& p : *pages) {
				Page page{ p.value("pageDisplayName", std::string{}), {} };
				const auto content = p.find("content");
				if (content == p.end() || !content->is_array()) {
					continue;
				}
				for (const auto& c : *content) {
					Control control;
					control.type = c.value("type", std::string{});
					control.text = c.value("text", std::string{});
					control.help = c.value("help", std::string{});
					// "b4S_SetEndOnOrgasm:General" -> the key and its section.
					const auto id = c.value("id", std::string{});
					if (const auto colon = id.find(':'); colon != std::string::npos) {
						control.key = id.substr(0, colon);
						control.section = id.substr(colon + 1);
					}
					if (const auto v = c.find("valueOptions"); v != c.end() && v->is_object()) {
						control.source = v->value("sourceType", std::string{});
						control.min = v->value("min", 0.0F);
						control.max = v->value("max", 100.0F);
						control.step = v->value("step", 1.0F);
						if (const auto o = v->find("options"); o != v->end() && o->is_array()) {
							for (const auto& option : *o) {
								if (option.is_string()) {
									control.options.push_back(option.get<std::string>());
								}
							}
						}
					}
					if (const auto a = c.find("action"); a != c.end() && a->is_object()) {
						control.function = a->value("function", std::string{});
					}
					page.controls.push_back(std::move(control));
				}
				g_pages.push_back(std::move(page));
			}
			return !g_pages.empty();
		}

		// A setting changed: written where the MCM writes it, then read
		// again, as the MCM's SettingChanged does (FourStimMCM.psc).
		void Change(const Control& a_control, const std::string& a_value)
		{
			if (MCM::Set(a_control.section, a_control.key, a_value)) {
				g_values[Core::LowerCopy(a_control.section) + "|" + Core::LowerCopy(a_control.key)] = a_value;
				g_stale = true;
				Core::ReloadSettings({});  // on the main thread, as the MCM's change
				REX::INFO("Menu Framework: {} = {}", a_control.key, a_value);
			} else {
				REX::WARN("Menu Framework: couldn't write {} to {}", a_control.key, MCM::SettingsPath().string());
			}
		}

		void Help(const Control& a_control)
		{
			if (!a_control.help.empty()) {
				SetItemTooltip("%s", T(a_control.help).c_str());
			}
		}

		void Draw(const Control& a_control, int a_index)
		{
			PushID(a_index);
			const auto label = T(a_control.text);
			const auto value = a_control.key.empty() ? std::string{} : Value(a_control.section, a_control.key);
			if (a_control.type == "section") {
				SeparatorText(label.c_str());
			} else if (a_control.type == "text") {
				TextWrapped("%s", label.c_str());
			} else if (a_control.type == "switcher") {
				bool on = value == "1" || value == "true";
				if (Checkbox(label.c_str(), &on)) {
					Change(a_control, on ? "1" : "0");
				}
				Help(a_control);
			} else if (a_control.type == "slider") {
				// Dragged: the value is held here and written once, on release.
				const auto id = a_control.section + "|" + a_control.key;
				auto       held = g_dragging.find(id);
				if (a_control.source == "ModSettingInt") {
					int number = held != g_dragging.end() ? static_cast<int>(held->second) : static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
					if (SliderInt(label.c_str(), &number, static_cast<int>(a_control.min), static_cast<int>(a_control.max))) {
						g_dragging[id] = static_cast<float>(number);
					}
				} else {
					float      number = held != g_dragging.end() ? held->second : std::strtof(value.c_str(), nullptr);
					const auto format = a_control.step >= 1.0F ? "%.0f" : a_control.step >= 0.1F ? "%.1f" : "%.2f";
					if (SliderFloat(label.c_str(), &number, a_control.min, a_control.max, format)) {
						g_dragging[id] = number;
					}
				}
				if (IsItemDeactivatedAfterEdit()) {
					if (const auto it = g_dragging.find(id); it != g_dragging.end()) {
						float number = it->second;
						g_dragging.erase(it);
						if (a_control.source == "ModSettingInt") {
							Change(a_control, std::to_string(static_cast<int>(number)));
						} else {
							if (a_control.step > 0.0F) {
								number = std::round(number / a_control.step) * a_control.step;
							}
							Change(a_control, std::format("{:.4f}", number));
						}
					}
				}
				Help(a_control);
			} else if (a_control.type == "dropdown") {
				int        index = static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
				const auto shown = index >= 0 && static_cast<std::size_t>(index) < a_control.options.size() ? T(a_control.options[static_cast<std::size_t>(index)]) : std::string{};
				if (BeginCombo(label.c_str(), shown.c_str())) {
					for (std::size_t i = 0; i < a_control.options.size(); ++i) {
						if (Selectable(T(a_control.options[i]).c_str(), static_cast<int>(i) == index)) {
							Change(a_control, std::to_string(i));
						}
					}
					EndCombo();
				}
				Help(a_control);
			} else if (a_control.type == "keyinput") {
				// Keys are set in the MCM or 4Stim.ini: a key press can't be
				// caught from here without taking the framework's own keys.
				const auto code = value.substr(0, value.find(','));
				Text("%s: %s", label.c_str(), code.empty() || code == "-1" ? "-" : std::format("key code {}", code).c_str());
				Help(a_control);
			} else if (a_control.type == "button") {
				if (Button(label.c_str())) {
					if (a_control.function == "ExportSettings") {
						Core::ExportSettings({});
					} else if (a_control.function == "ImportSettings") {
						Core::ImportSettings({});
					} else if (a_control.function == "UseIniSettings") {
						Core::UseIniSettings({});
					}
				}
				Help(a_control);
			}
			PopID();
		}

		void DrawPage(std::size_t a_page)
		{
			if (a_page >= g_pages.size()) {
				return;
			}
			Refresh();
			const auto& controls = g_pages[a_page].controls;
			for (std::size_t i = 0; i < controls.size(); ++i) {
				Draw(controls[i], static_cast<int>(i));
			}
		}

		// The framework calls a plain function per page.
		template <std::size_t N>
		void __stdcall Render()
		{
			DrawPage(N);
		}

		template <std::size_t... I>
		constexpr auto MakeRenderers(std::index_sequence<I...>)
		{
			return std::array<F4SEMenuFramework::Model::RenderFunction, sizeof...(I)>{ &Render<I>... };
		}

		constexpr std::size_t kMaxPages = 24;
		constexpr auto        kRenderers = MakeRenderers(std::make_index_sequence<kMaxPages>{});
	}

	void Register()
	{
		static bool done = false;
		if (done) {
			return;
		}
		done = true;
		if (!F4SEMenuFramework::IsInstalled()) {
			REX::INFO("Menu Framework: not installed, no settings pages there");
			return;
		}
		if (!Load()) {
			return;
		}
		if (g_pages.size() > kMaxPages) {
			REX::WARN("Menu Framework: only the first {} of {} pages", kMaxPages, g_pages.size());
			g_pages.resize(kMaxPages);
		}
		F4SEMenuFramework::SetSection("4Stim");
		for (std::size_t i = 0; i < g_pages.size(); ++i) {
			F4SEMenuFramework::AddSectionItem(std::format("{}/{}", Translations::Get("$4stim_menu_settings", "Settings"), T(g_pages[i].name)), kRenderers[i]);
		}
		REX::INFO("Menu Framework: {} settings page(s) added", g_pages.size());
	}
}
