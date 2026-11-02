#include "Translations.h"
#include "compat/Fallbacks.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <queue>
#include <stack>
#include <unordered_map>

namespace Translations
{
	namespace
	{
		constexpr auto FOLDER = "Data/Interface/Translations/";

		std::mutex                                   g_lock;
		std::unordered_map<std::string, std::string> g_text;   // key as written -> text
		std::vector<std::pair<std::string, std::string>> g_order;  // file order
		std::string                                  g_language = "en";

		void AppendUTF8(std::string& a_out, std::uint32_t a_cp)
		{
			if (a_cp < 0x80) {
				a_out += static_cast<char>(a_cp);
			} else if (a_cp < 0x800) {
				a_out += static_cast<char>(0xC0 | (a_cp >> 6));
				a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
			} else if (a_cp < 0x10000) {
				a_out += static_cast<char>(0xE0 | (a_cp >> 12));
				a_out += static_cast<char>(0x80 | ((a_cp >> 6) & 0x3F));
				a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
			} else {
				a_out += static_cast<char>(0xF0 | (a_cp >> 18));
				a_out += static_cast<char>(0x80 | ((a_cp >> 12) & 0x3F));
				a_out += static_cast<char>(0x80 | ((a_cp >> 6) & 0x3F));
				a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
			}
		}

		// The file as UTF-8: Fallout 4's translation files are UTF-16 LE with a
		// byte order mark; a UTF-8 file (with or without one) works too.
		std::string ReadText(const std::filesystem::path& a_path)
		{
			std::ifstream in(a_path, std::ios::binary);
			const std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
				std::string out;
				for (std::size_t i = 2; i + 1 < raw.size(); i += 2) {
					std::uint32_t unit = static_cast<unsigned char>(raw[i]) | (static_cast<unsigned char>(raw[i + 1]) << 8);
					if (unit >= 0xD800 && unit < 0xDC00 && i + 3 < raw.size()) {
						const std::uint32_t low = static_cast<unsigned char>(raw[i + 2]) | (static_cast<unsigned char>(raw[i + 3]) << 8);
						unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
						i += 2;
					}
					AppendUTF8(out, unit);
				}
				return out;
			}
			if (raw.size() >= 3 && raw.compare(0, 3, "\xEF\xBB\xBF") == 0) {
				return raw.substr(3);
			}
			return raw;
		}

		std::string GameLanguage()
		{
			if (const auto ini = RE::INISettingCollection::GetSingleton()) {
				if (const auto setting = ini->GetSetting("sLanguage:General"); setting && setting->GetType() == RE::Setting::SETTING_TYPE::kString) {
					std::string language(setting->GetString());
					std::ranges::transform(language, language.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (!language.empty()) {
						return language;
					}
				}
			}
			return "en";
		}

		// ---- OStim's LocaleManager (Localize) ----
		std::mutex                                   g_localeLock;
		bool                                         g_localeLoaded = false;
		std::unordered_map<std::string, std::string> g_localeEnglish;  // sanitized key -> text
		std::unordered_map<std::string, std::string> g_localeLocal;    // the game's language's

		std::string LowerCopy(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		// Skyrim's sLanguage names for Fallout 4's codes (OStim packs come
		// with "_ENGLISH.txt" files).
		std::string SkyrimLanguage(const std::string& a_code)
		{
			static const std::unordered_map<std::string, std::string> names{
				{ "en", "english" }, { "de", "german" }, { "fr", "french" }, { "es", "spanish" }, { "it", "italian" },
				{ "pl", "polish" }, { "ru", "russian" }, { "ja", "japanese" }, { "cn", "chinese" }
			};
			const auto it = names.find(a_code);
			return it != names.end() ? it->second : std::string{};
		}

		// OStim's SanitizeKey: the contents of each outermost {...} removed
		// ("$a{1}" -> "$a{}"). nullopt on a '}' with no '{'.
		std::optional<std::string> SanitizeKey(std::string a_key)
		{
			std::stack<std::size_t> stack;
			for (std::size_t pos = 0; pos < a_key.size(); ++pos) {
				if (a_key[pos] == '{') {
					stack.push(pos);
				} else if (a_key[pos] == '}') {
					if (stack.empty()) {
						return std::nullopt;
					}
					if (stack.size() == 1) {
						const auto last = stack.top();
						stack.pop();
						const auto off = last + 1;
						if (pos > off) {
							a_key.erase(off, pos - off);
						}
						pos = off;
					} else {
						stack.pop();
					}
				}
			}
			if (!a_key.empty() && a_key.back() == '\r') {
				a_key.pop_back();
			}
			return a_key;
		}

		// OStim's ReadFromFile: "$key<tab>text" lines, the key sanitized; the
		// first file to have a key keeps it.
		void ReadLocaleFile(const std::filesystem::path& a_path, std::unordered_map<std::string, std::string>& a_map)
		{
			const auto  text = ReadText(a_path);
			std::size_t start = 0;
			while (start < text.size()) {
				auto end = text.find('\n', start);
				if (end == std::string::npos) {
					end = text.size();
				}
				std::string line = text.substr(start, end - start);
				start = end + 1;
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				if (line.empty() || line[0] != '$') {
					continue;
				}
				const auto tab = line.find('\t');
				if (tab == std::string::npos || tab + 1 >= line.size()) {
					continue;
				}
				auto key = line.substr(0, tab);
				if (auto sanitized = SanitizeKey(key)) {
					key = std::move(*sanitized);
				}
				a_map.emplace(std::move(key), line.substr(tab + 1));
			}
		}

		// Every "*_<one of a_suffixes>.txt" in the folder, in name order.
		std::size_t ReadLocaleFiles(const std::vector<std::string>& a_suffixes, std::unordered_map<std::string, std::string>& a_map)
		{
			std::error_code ec;
			if (!std::filesystem::is_directory(FOLDER, ec)) {
				return 0;
			}
			std::vector<std::filesystem::path> files;
			for (const auto& entry : std::filesystem::directory_iterator(FOLDER, ec)) {
				const auto name = LowerCopy(entry.path().filename().string());
				for (const auto& suffix : a_suffixes) {
					const auto ending = "_" + suffix + ".txt";
					if (!suffix.empty() && name.size() > ending.size() && name.ends_with(ending)) {
						files.push_back(entry.path());
						break;
					}
				}
			}
			std::ranges::sort(files);
			for (const auto& file : files) {
				ReadLocaleFile(file, a_map);
			}
			return files.size();
		}

		// g_localeLock held.
		void LoadLocaleLocked()
		{
			g_localeEnglish.clear();
			g_localeLocal.clear();
			const auto language = GameLanguage();
			std::size_t files = ReadLocaleFiles({ "en", "english" }, g_localeEnglish);
			if (language != "en") {
				files += ReadLocaleFiles({ language, SkyrimLanguage(language) }, g_localeLocal);
			}
			g_localeLoaded = true;
			REX::INFO("Translations: {} scene text(s) in English, {} in \"{}\", from {} file(s)", g_localeEnglish.size(), g_localeLocal.size(), language, files);
		}

		std::optional<std::string> FindLocalization(const std::string& a_key)
		{
			if (const auto it = g_localeLocal.find(a_key); it != g_localeLocal.end()) {
				return it->second;
			}
			if (const auto it = g_localeEnglish.find(a_key); it != g_localeEnglish.end()) {
				return it->second;
			}
			return std::nullopt;
		}

		std::string LocalizeLocked(const std::string& a_key);

		// OStim's GetNestedLocalizations: each outermost {...}'s contents,
		// translated. false on an empty {} or a '}' with no '{'.
		bool GetNested(const std::string& a_key, std::queue<std::string>& a_queue)
		{
			std::stack<std::size_t> stack;
			for (std::size_t pos = 0; pos < a_key.size(); ++pos) {
				if (a_key[pos] == '{') {
					stack.push(pos);
				} else if (a_key[pos] == '}') {
					if (stack.empty()) {
						return false;
					}
					if (stack.size() == 1) {
						const auto last = stack.top();
						stack.pop();
						const auto off = last + 1;
						if (pos == off) {
							return false;  // nothing to put in the text's {}
						}
						a_queue.push(LocalizeLocked(a_key.substr(off, pos - off)));
					} else {
						stack.pop();
					}
				}
			}
			return true;
		}

		// OStim's InsertLocalizations: each innermost {} in the text gets the
		// next nested translation ("{{}}" -> "{1}").
		void InsertNested(std::string& a_text, std::queue<std::string>& a_queue)
		{
			std::stack<std::size_t> stack;
			for (std::size_t pos = 0; pos < a_text.size(); ++pos) {
				if (a_text[pos] == '{') {
					stack.push(pos);
				} else if (a_text[pos] == '}') {
					if (stack.empty() || a_queue.empty()) {
						return;
					}
					const auto beg = stack.top();
					stack.pop();
					a_text.replace(beg, pos - beg + 1, a_queue.front());
					a_queue.pop();
					pos = beg;
				}
			}
		}

		// OStim's GetLocalizationInternal.
		std::string LocalizeLocked(const std::string& a_key)
		{
			if (a_key.empty() || a_key[0] != '$') {
				return a_key;
			}
			const auto sanitized = SanitizeKey(a_key);
			if (!sanitized) {
				return a_key;
			}
			auto text = FindLocalization(*sanitized);
			if (!text) {
				return a_key;
			}
			std::queue<std::string> nested;
			if (GetNested(a_key, nested)) {
				InsertNested(*text, nested);
			}
			return *text;
		}

		// Reads one file into the maps (later keys replace earlier ones).
		std::size_t ReadFile(const std::filesystem::path& a_path)
		{
			std::size_t count = 0;
			const auto  text = ReadText(a_path);
			std::size_t start = 0;
			while (start < text.size()) {
				auto end = text.find('\n', start);
				if (end == std::string::npos) {
					end = text.size();
				}
				std::string line = text.substr(start, end - start);
				start = end + 1;
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				const auto tab = line.find('\t');
				if (line.empty() || line[0] != '$' || tab == std::string::npos) {
					continue;
				}
				auto key = line.substr(0, tab);
				auto value = line.substr(tab + 1);
				if (!g_text.contains(key)) {
					g_order.emplace_back(key, value);
				} else {
					std::ranges::find_if(g_order, [&](const auto& a_entry) { return a_entry.first == key; })->second = value;
				}
				g_text[std::move(key)] = std::move(value);
				++count;
			}
			return count;
		}
	}

	void Load()
	{
		std::scoped_lock lock(g_lock);
		g_text.clear();
		g_order.clear();
		g_language = GameLanguage();
		{
			// The scene packs' texts too (Localize), the next time one is asked for.
			std::scoped_lock localeLock(g_localeLock);
			g_localeLoaded = false;
		}
		// The English first (every key), then the game's language over it.
		const std::filesystem::path english = std::string(FOLDER) + "4Stim_en.txt";
		const std::filesystem::path local = std::string(FOLDER) + "4Stim_" + g_language + ".txt";
		std::error_code ec;
		std::size_t     count = std::filesystem::exists(english, ec) ? ReadFile(english) : 0;
		if (g_language != "en" && std::filesystem::exists(local, ec)) {
			count = ReadFile(local);
			REX::INFO("Translations: {} text(s) in {} (language \"{}\")", count, local.filename().string(), g_language);
		} else {
			REX::INFO("Translations: {} text(s) in 4Stim_en.txt (language \"{}\"{})", count, g_language,
				g_language != "en" ? ", no 4Stim_" + g_language + ".txt" : std::string{});
		}
	}

	std::string Localize(std::string_view a_text)
	{
		if (a_text.empty() || a_text[0] != '$') {
			return std::string(a_text);
		}
		std::scoped_lock lock(g_localeLock);
		if (!g_localeLoaded) {
			LoadLocaleLocked();
		}
		return LocalizeLocked(std::string(a_text));
	}

	void AddToGame()
	{
		// Fallout 4 reads Interface\Translations\<plugin>_<language>.txt only
		// for a plugin of that name, and 4Stim is an F4SE plugin with no
		// 4Stim.esp, so MCM showed the bare "$4Stim_..." keys. Hand the file
		// to the game's Scaleform translator ourselves: the game's language's,
		// else the English. (One file only: whether the translator lets a
		// second file's keys replace the first's isn't known, so a
		// translation should carry every key, as make-translations.py writes.)
		const auto manager = RE::BSScaleformManager::GetSingleton();
		const auto translator = manager && manager->loader ? manager->GetTranslator() : nullptr;
		if (!translator) {
			REX::WARN("Translations: the game's translator isn't up yet, MCM labels stay untranslated");
			return;
		}
		const auto      language = Language();
		std::error_code ec;
		std::string     name = "4Stim_" + language + ".txt";
		if (!std::filesystem::exists(std::string(FOLDER) + name, ec)) {
			name = "4Stim_en.txt";
		}
		if (!Compat::CanAddTranslations()) {
			// 1.10.163: the Runtime Database has no
			// BSScaleformTranslator::AddTranslations there (docs/OLDGEN.md).
			// As F4SE's own translation loader does, put the texts (Load's:
			// the game's language's, over the English) in the translator's
			// table directly.
			auto widen = [](const std::string& a_text) {
				return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(a_text.data()), a_text.size())).wstring();
			};
			std::size_t added = 0;
			for (const auto& [key, text] : Entries()) {
				const auto wideKey = widen(key);
				const auto wideText = widen(text);
				if (translator->translator.translationMap.emplace(RE::BSFixedStringWCS(wideKey.c_str()), RE::BSFixedStringWCS(wideText.c_str())).second) {
					++added;
				}
			}
			REX::INFO("Translations: {} text(s) added to the game's translator", added);
		} else if (std::filesystem::exists(std::string(FOLDER) + name, ec)) {
			const auto file = "Interface\\Translations\\" + name;
			translator->AddTranslationsFile(file.c_str());
			REX::INFO("Translations: {} handed to the game", name);
		}
		translator->Release();  // GetTranslator added a reference
	}

	std::string Language()
	{
		std::scoped_lock lock(g_lock);
		return g_language;
	}

	std::string Get(std::string_view a_key, std::string_view a_english)
	{
		std::scoped_lock lock(g_lock);
		const auto       it = g_text.find(std::string(a_key));
		return it != g_text.end() ? it->second : std::string(a_english);
	}

	std::vector<std::pair<std::string, std::string>> Entries()
	{
		std::scoped_lock lock(g_lock);
		return g_order;
	}
}
