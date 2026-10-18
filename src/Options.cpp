#include "Options.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>

#include <nlohmann/json.hpp>

#include "Bridge.h"
#include "EquipObjects.h"
#include "SceneRegistry.h"
#include "Sounds.h"
#include "Translations.h"
#include "Undress.h"

namespace Options
{
	namespace
	{
		constexpr auto FOLDER = "Data/F4SE/Plugins/4Stim/Options";  // OStim: SKSE\Plugins\OStim\options

		struct State
		{
			std::string name;
			std::string description;
			std::string icon;
			std::string border;
			std::string script;
			std::string function;
		};

		struct Option
		{
			std::string        name;
			std::string        page;
			std::string        repeat;     // "", "actor", "equipObject"
			std::string        equipType;
			std::string        stateOf;    // 4Stim's "state": muted, stripped, equipped:<type>
			std::vector<State> states;     // file order
		};

		struct Page
		{
			std::string name;
			std::string description;
			std::string icon;
			std::string border;
			std::string parent;  // "" = the top
			bool        perActor = false;
			// 4Stim's "sex": shown only for an actor of this sex (the page's
			// own actor, or the one of the page above): "female" / "male".
			SceneRegistry::Sex sex = SceneRegistry::Sex::kAny;
		};

		bool SexFits(const Page& a_page, std::uint32_t a_actor)
		{
			if (a_page.sex == SceneRegistry::Sex::kAny) {
				return true;
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actor);
			return actor && SceneRegistry::SexOf(actor) == a_page.sex;
		}

		std::mutex                         g_lock;
		bool                               g_loaded = false;
		std::vector<Page>                  g_pages;    // first file's definition of a name wins, as OStim
		std::vector<Option>                g_options;
		std::map<std::string, std::string> g_english;  // "$4Stim_..." -> English

		// Where the HUD is: the pages opened, each with the actor it's for
		// (-1: not a per-actor page), and whose scene it is.
		struct Step
		{
			std::string page;
			int         actor = -1;
		};
		std::vector<Step> g_path;
		std::uint32_t     g_pathScene = 0;

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		// OStim's icon paths ("OStim/symbols/x") are 4Stim's icon folder.
		std::string Icon(std::string a_path)
		{
			if (Lower(a_path).starts_with("ostim/")) {
				a_path = "4Stim/" + a_path.substr(6);
			}
			return a_path;
		}

		std::string Text(const std::string& a_text)
		{
			if (!a_text.starts_with("$")) {
				return a_text;
			}
			const auto english = g_english.find(a_text);
			return Translations::Get(a_text, english != g_english.end() ? english->second : a_text);
		}

		void Replace(std::string& a_text, std::string_view a_what, const std::string& a_with)
		{
			for (auto at = a_text.find(a_what); at != std::string::npos; at = a_text.find(a_what, at + a_with.size())) {
				a_text.replace(at, a_what.size(), a_with);
			}
		}

		// a_you: the player as "yourself" (in options), else by name (pages).
		std::string ActorName(std::uint32_t a_id, bool a_you = true)
		{
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (a_you && actor && actor == player) {
				return TR("$4Stim_Yourself", "yourself");
			}
			const char* name = actor ? actor->GetDisplayFullName() : nullptr;
			return name && *name ? name : std::format("{:08X}", a_id);
		}

		std::string String(const nlohmann::json& a_value, const char* a_key)
		{
			const auto it = a_value.find(a_key);
			return it != a_value.end() && it->is_string() ? it->get<std::string>() : std::string{};
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_pages.clear();
			g_options.clear();
			g_english.clear();
			std::vector<std::filesystem::path> files;
			std::error_code                    ec;
			for (const auto& e : std::filesystem::recursive_directory_iterator(FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					files.push_back(e.path());
				}
			}
			std::ranges::sort(files);
			for (const auto& path : files) {
				nlohmann::json root;
				try {
					std::ifstream in(path);
					root = nlohmann::json::parse(in, nullptr, true, true);
				} catch (const std::exception& ex) {
					REX::WARN("Options: {}: not valid JSON ({})", path.filename().string(), ex.what());
					continue;
				}
				if (const auto english = root.find("english"); english != root.end() && english->is_object()) {
					for (const auto& [key, text] : english->items()) {
						if (text.is_string()) {
							g_english[key] = text.get<std::string>();
						}
					}
				}
				for (const auto& p : root.value("pages", nlohmann::json::array())) {
					Page page;
					page.name = String(p, "name");
					if (page.name.empty()) {
						REX::WARN("Options: {}: a page without a name, skipped", path.filename().string());
						continue;
					}
					if (std::ranges::any_of(g_pages, [&](const Page& a) { return a.name == page.name; })) {
						continue;  // as OStim: defined already
					}
					page.description = String(p, "description");
					if (page.description.empty()) {
						page.description = page.name;
					}
					page.icon = Icon(String(p, "icon"));
					page.border = String(p, "border");
					page.parent = String(p, "parent");  // null or missing: the top
					page.perActor = Lower(String(p, "repeat")) == "actor";
					const auto sex = Lower(String(p, "sex"));
					page.sex = sex == "female" ? SceneRegistry::Sex::kFemale : sex == "male" ? SceneRegistry::Sex::kMale : SceneRegistry::Sex::kAny;
					g_pages.push_back(std::move(page));
				}
				for (const auto& o : root.value("options", nlohmann::json::array())) {
					Option option;
					option.name = String(o, "name");
					option.page = String(o, "page");
					if (option.name.empty() || option.page.empty()) {
						REX::WARN("Options: {}: an option without a name or page, skipped", path.filename().string());
						continue;
					}
					option.repeat = String(o, "repeat");
					option.equipType = Lower(String(o, "equipType"));
					option.stateOf = Lower(String(o, "state"));
					if (o.contains("stateFaction")) {
						REX::INFO("Options: {}: \"{}\": stateFaction isn't supported (4Stim's \"state\" is)", path.filename().string(), option.name);
					}
					if (const auto states = o.find("states"); states != o.end() && states->is_object()) {
						for (const auto& [name, s] : states->items()) {
							State state;
							state.name = name;
							state.description = String(s, "description");
							state.icon = Icon(String(s, "icon"));
							state.border = String(s, "border");
							const auto function = String(s, "function");
							if (const auto slash = function.find('/'); slash != std::string::npos) {
								state.script = function.substr(0, slash);
								state.function = function.substr(slash + 1);
							} else if (!function.empty()) {
								REX::WARN("Options: {}: \"{}\" state \"{}\": the function must be \"Script/Function\"", path.filename().string(), option.name, name);
							}
							option.states.push_back(std::move(state));
						}
					}
					if (option.states.empty()) {
						REX::WARN("Options: {}: \"{}\" has no states, skipped", path.filename().string(), option.name);
						continue;
					}
					g_options.push_back(std::move(option));
				}
			}
			REX::INFO("Options: {} page(s) and {} option(s) from {} file(s)", g_pages.size(), g_options.size(), files.size());
		}

		void EnsureLoaded()
		{
			if (!g_loaded) {
				LoadLocked();
			}
		}

		const Page* FindPage(const std::string& a_name)
		{
			const auto it = std::ranges::find_if(g_pages, [&](const Page& p) { return p.name == a_name; });
			return it != g_pages.end() ? &*it : nullptr;
		}

		// The state an option shows for an actor: 4Stim's "state", else the
		// first (as OStim without a state faction).
		const State& ShownState(const Option& a_option, std::uint32_t a_actor)
		{
			std::string wanted;
			if (a_actor && !a_option.stateOf.empty()) {
				bool on = false;
				if (a_option.stateOf == "muted") {
					on = Sounds::IsMuted(a_actor);
				} else if (a_option.stateOf == "stripped") {
					on = Undress::IsStripped(a_actor);
				} else if (a_option.stateOf.starts_with("equipped:")) {
					on = EquipObjects::IsEquipped(a_actor, a_option.stateOf.substr(9));
				}
				wanted = on ? "on" : "off";
			}
			const auto it = std::ranges::find_if(a_option.states, [&](const State& s) { return Lower(s.name) == wanted; });
			return it != a_option.states.end() ? *it : a_option.states.front();
		}

		std::string Describe(const State& a_state, std::uint32_t a_actor, const std::string& a_type)
		{
			auto text = Text(a_state.description.empty() ? a_state.name : a_state.description);
			if (a_actor) {
				Replace(text, "{actor}", ActorName(a_actor));
			}
			if (!a_type.empty()) {
				Replace(text, "{type}", a_type);
			}
			return text;
		}

		// "__opt:<option>|<actor>|<state>|<object>"
		std::string OptionID(std::size_t a_option, int a_actor, const std::string& a_state, const std::string& a_object)
		{
			return std::format("{}:{}|{}|{}|{}", OPTION_PREFIX, a_option, a_actor, a_state, a_object);
		}

		std::vector<std::string> Split(const std::string& a_text, char a_by)
		{
			std::vector<std::string> out;
			std::size_t              start = 0;
			for (auto at = a_text.find(a_by); at != std::string::npos; at = a_text.find(a_by, start)) {
				out.push_back(a_text.substr(start, at - start));
				start = at + 1;
			}
			out.push_back(a_text.substr(start));
			return out;
		}

		// A different scene (or none): back to the top.
		void CheckScene(const std::vector<std::uint32_t>& a_actors)
		{
			const auto scene = a_actors.empty() ? 0 : a_actors.front();
			if (scene != g_pathScene) {
				g_path.clear();
				g_pathScene = scene;
			}
		}
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
		g_path.clear();
	}

	std::vector<Entry> Rows(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		CheckScene(a_actors);
		std::vector<Entry> rows;
		const std::string  here = g_path.empty() ? std::string{} : g_path.back().page;
		// The actor this page is for (its own or a page above's).
		int pageActor = -1;
		for (const auto& step : g_path) {
			if (step.actor >= 0) {
				pageActor = step.actor;
			}
		}
		if (!g_path.empty()) {
			const auto parent = g_path.size() > 1 ? FindPage(g_path[g_path.size() - 2].page) : nullptr;
			rows.push_back({ std::string(OPTION_PREFIX) + ":back", TRF("$4Stim_OptionsBack", "Back to {}", parent ? Text(parent->description) : TR("$4Stim_TabUtility", "Utility")),
				"4Stim/symbols/return", "ff0000" });
		}
		// Pages under this one (one per actor where it repeats).
		for (const auto& page : g_pages) {
			if (page.parent != here) {
				continue;
			}
			if (page.perActor) {
				for (std::size_t i = 0; i < a_actors.size(); ++i) {
					if (!SexFits(page, a_actors[i])) {
						continue;
					}
					auto label = Text(page.description);
					Replace(label, "{actor}", ActorName(a_actors[i], false));
					// 4Stim's: "{sex}" in a per-actor page's icon is the actor's
					// "m" or "f" (4Stim/symbols/gender_{sex}); OStim shows the
					// same icon for every actor.
					auto icon = page.icon;
					if (icon.find("{sex}") != std::string::npos) {
						const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actors[i]);
						Replace(icon, "{sex}", actor && SceneRegistry::SexOf(actor) == SceneRegistry::Sex::kMale ? "m" : "f");
					}
					rows.push_back({ std::format("{}page:{}|{}", OPTION_PREFIX, page.name, i), label, icon, page.border });
				}
			} else {
				if (pageActor >= 0 && static_cast<std::size_t>(pageActor) < a_actors.size() && !SexFits(page, a_actors[pageActor])) {
					continue;
				}
				auto label = Text(page.description);
				if (pageActor >= 0 && static_cast<std::size_t>(pageActor) < a_actors.size()) {
					Replace(label, "{actor}", ActorName(a_actors[pageActor]));
				}
				rows.push_back({ std::format("{}page:{}|-1", OPTION_PREFIX, page.name), label, page.icon, page.border });
			}
		}
		if (here.empty()) {
			return rows;  // OStim's options sit in pages
		}
		for (std::size_t index = 0; index < g_options.size(); ++index) {
			const auto& option = g_options[index];
			if (option.page != here) {
				continue;
			}
			const auto repeat = Lower(option.repeat);
			auto       add = [&](int a_actor) {
				const std::uint32_t id = a_actor >= 0 && static_cast<std::size_t>(a_actor) < a_actors.size() ? a_actors[a_actor] : 0;
				if (repeat == "equipobject" && !option.equipType.empty()) {
					// OStim's: the "toggle" option, then one per object of the type.
					const auto toggle = std::ranges::find_if(option.states, [](const State& s) { return Lower(s.name) == "toggle"; });
					const auto object = std::ranges::find_if(option.states, [](const State& s) { return Lower(s.name) == "object"; });
					if (toggle != option.states.end()) {
						rows.push_back({ OptionID(index, a_actor, toggle->name, {}), Describe(*toggle, id, {}), toggle->icon, toggle->border });
					}
					if (object != option.states.end()) {
						const auto chosen = id ? EquipObjects::Chosen(id, option.equipType) : std::string{};
						for (const auto& [objectID, name] : EquipObjects::ObjectsOf(option.equipType)) {
							auto label = Describe(*object, id, name);
							if (objectID == chosen) {
								label += "  *";  // the one they wear
							}
							rows.push_back({ OptionID(index, a_actor, object->name, objectID), label, object->icon, object->border });
						}
					}
					return;
				}
				const auto& state = ShownState(option, id);
				rows.push_back({ OptionID(index, a_actor, state.name, {}), Describe(state, id, {}), state.icon, state.border });
			};
			if (repeat == "actor" && pageActor < 0) {
				for (std::size_t i = 0; i < a_actors.size(); ++i) {
					add(static_cast<int>(i));
				}
			} else {
				add(pageActor);
			}
		}
		return rows;
	}

	bool InPage(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		CheckScene(a_actors);
		return !g_path.empty();
	}

	void Pick(const std::string& a_id, const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		CheckScene(a_actors);
		const auto rest = a_id.substr(OPTION_PREFIX.size());
		REX::INFO("Options: picked \"{}\"", a_id);
		if (rest == ":back") {
			if (!g_path.empty()) {
				g_path.pop_back();
			}
			return;
		}
		if (rest.starts_with("page:")) {
			const auto parts = Split(rest.substr(5), '|');
			if (parts.size() == 2 && FindPage(parts[0])) {
				g_path.push_back({ parts[0], std::stoi(parts[1]) });
			}
			return;
		}
		if (!rest.starts_with(":")) {
			return;
		}
		const auto parts = Split(rest.substr(1), '|');
		std::size_t index = 0;
		int         actorIndex = -1;
		try {
			if (parts.size() != 4) {
				throw std::invalid_argument("parts");
			}
			index = static_cast<std::size_t>(std::stoul(parts[0]));
			actorIndex = std::stoi(parts[1]);
		} catch (const std::exception&) {
			REX::WARN("Options: \"{}\" isn't an option row", a_id);
			return;
		}
		if (index >= g_options.size()) {
			REX::WARN("Options: option {} doesn't exist (options reloaded?)", index);
			return;
		}
		const auto& option = g_options[index];
		const auto  state = std::ranges::find_if(option.states, [&](const State& s) { return s.name == parts[2]; });
		if (state == option.states.end() || state->function.empty()) {
			REX::WARN("Options: \"{}\" has no state \"{}\" with a function", option.name, parts[2]);
			return;
		}
		const auto vm = FourStim::GetVM();
		if (!vm) {
			return;
		}
		// As OStim's ExecuteOption: (state), (state, actor), (state, actor, object).
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const auto actor = actorIndex >= 0 && static_cast<std::size_t>(actorIndex) < a_actors.size() ? RE::TESForm::GetFormByID<RE::Actor>(a_actors[actorIndex]) : nullptr;
		REX::INFO("Options: \"{}\" ({}) -> {}.{} for {:08X}{}", option.name, state->name, state->script, state->function, actor ? actor->GetFormID() : 0,
			parts[3].empty() ? std::string{} : " (\"" + parts[3] + "\")");
		if (actor && !parts[3].empty()) {
			vm->DispatchStaticCall(state->script, state->function, callback, std::string(state->name), actor, std::string(parts[3]));
		} else if (actor) {
			vm->DispatchStaticCall(state->script, state->function, callback, std::string(state->name), actor);
		} else {
			vm->DispatchStaticCall(state->script, state->function, callback, std::string(state->name));
		}
	}
}
