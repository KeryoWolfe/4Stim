#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Translations of 4Stim's own text (docs/TRANSLATIONS.md): the picker, the
// HUD, notifications, and the MCM page. One file per language, as Fallout 4
// names them: Data\Interface\Translations\4Stim_<language>.txt (en, de, fr,
// es, it, pl, ptbr, ru, ja, cn...), UTF-16 with "$key<tab>text" lines.
// 4Stim reads it for the text it builds (names and numbers filled in),
// falling back to the English in the code, and hands it to the game
// (AddToGame) for MCM's "$4Stim_..." labels.
namespace Translations
{
	// Reads the file for the game's language (sLanguage), else the English
	// one. Again on FourStim.ReloadScenes().
	void Load();

	// Adds the file(s) to the game's Scaleform translator, which reads such
	// files only for a plugin named 4Stim (there's no 4Stim.esp). Once, after
	// Load, before any menu shows the texts.
	void AddToGame();

	// The game's language code ("en"...).
	std::string Language();

	// The text for a_key ("$4Stim_..."), or a_english.
	std::string Get(std::string_view a_key, std::string_view a_english);

	// Every translation loaded (for the HUD's and picker's own labels).
	std::vector<std::pair<std::string, std::string>> Entries();

	// Get, then std::format's {} filled in (the translation's, or if it
	// doesn't fit the arguments, the English).
	template <class... Args>
	std::string Format(std::string_view a_key, std::string_view a_english, const Args&... a_args)
	{
		const auto text = Get(a_key, a_english);
		try {
			return std::vformat(text, std::make_format_args(a_args...));
		} catch (const std::exception&) {
			try {
				return std::vformat(a_english, std::make_format_args(a_args...));
			} catch (const std::exception&) {
				return std::string(a_english);
			}
		}
	}
}

// Shorthands: TR("$4Stim_Key", "English"), TRF("$4Stim_Key", "English {}", value).
// tools/make-translations.py collects them into 4Stim_en.txt.
#define TR(a_key, a_english) Translations::Get(a_key, a_english)
#define TRF(a_key, a_english, ...) Translations::Format(a_key, a_english, __VA_ARGS__)
