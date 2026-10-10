#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <array>
#include <cmath>
#include <set>

#include <nlohmann/json.hpp>

#include "Excitement.h"
#include "Undress.h"
#include "Bridge.h"
#include "HUD.h"
#include "SceneRegistry.h"

namespace HUD
{
	namespace
	{
		using json = nlohmann::json;
		using GValue = Scaleform::GFx::Value;

		constexpr auto THEME_DIR = "Data/Interface/4Stim/Themes"sv;
		constexpr auto UTILITY_DIR = "Data/F4SE/Plugins/4Stim/Utility"sv;
		constexpr auto ICON_DIR = "4Stim/Icons/"sv;  // icon paths are under Data\Interface\4Stim\Icons\ (HUD_API.md)
		constexpr auto END_ID = "__end__"sv;
		constexpr auto STOP_WATCHING_ID = "__stopwatching__"sv;
		// Built-in Utility entries: undress / dress one of the scene's actors.
		constexpr auto STRIP_PREFIX = "__strip:"sv;  // + the actor's form ID in hex
		constexpr auto DRESS_PREFIX = "__dress:"sv;  // main.cpp handles it in NavigateFocused

		// The built-in "Color" theme. Every theme is laid over this, so a
		// theme file only needs the fields it changes. Kept in step with
		// Data\Interface\4Stim\Themes\Color.json.
		constexpr auto DEFAULT_THEME = R"json({
			"name": "Color",
			"layout": { "anchor": "bottomLeft", "offsetX": 0, "offsetY": 0, "scale": 1.0, "opacity": 1.0 },
			"colors": {
				"text": "#F4EAF1",
				"textMuted": "#C3B2CE",
				"accent": "#E7567A",
				"separator": ["#6A7CAF", "#E7567A"],
				"ring": ["#E7567A", "#6A7CAF"],
				"disc": "#17121F",
				"tabBackground": "#16101EDB",
				"tabBorder": "#9A6E9ACC",
				"tabSelected": ["#6A7CAF", "#E7567A"],
				"tabSelectedText": "#FFFFFF",
				"navBackground": "#16101EE6",
				"navSelected": ["#E7567A", "#6A7CAF"],
				"navSelectedText": "#FFFFFF",
				"iconTile": "#08060CB3",
				"meterTrack": "#FFFFFF24",
				"meterMale": ["#5B80D8"],
				"meterFemale": ["#E7567A"],
				"meterOther": ["#9A6E9A"],
				"speed": ["#6A7CAF", "#E7567A"]
			},
			"show": { "tabs": true, "navigation": true, "logo": true, "actorMeters": true, "speedMeter": true },
			"logoMovie": "4Stim\\HUDLogo.swf",
			"tintLogo": true,
			"font": "$MAIN_Font"
		})json"sv;

		// Colors a theme's "tint" replaces, unless the theme sets them itself.
		constexpr std::array TINT_KEYS{ "text"sv, "textMuted"sv, "accent"sv, "separator"sv, "ring"sv, "tabBorder"sv, "tabSelected"sv,
			"navSelected"sv, "meterMale"sv, "meterFemale"sv, "meterOther"sv, "speed"sv };

		struct UtilityEntry
		{
			std::string      id;
			std::string      label;
			std::string      icon;
			std::string      script;
			std::string      function;
			std::vector<int> actorCounts;  // empty = all
			int              order = 1000;
		};

		std::mutex                g_configLock;
		std::string               g_themeName = "Color";
		bool                      g_enabled = true;
		json                      g_theme;
		std::vector<UtilityEntry> g_utility;

		std::atomic<bool> g_focused = false;

		// The Align tab: which role is being adjusted and the step size.
		constexpr std::array<float, 5> ALIGN_STEPS{ 0.5F, 1.0F, 2.0F, 5.0F, 10.0F };  // units / degrees; scale moves by step / 100
		std::size_t                    g_alignRole = 0;
		std::size_t                    g_alignStep = 1;
		std::atomic<bool> g_showRequested = false;

		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		// Theme and icon paths are written with either slash; Scaleform's
		// loader wants forward slashes.
		std::string Slashes(std::string a_path)
		{
			std::ranges::replace(a_path, '\\', '/');
			return a_path;
		}

		// An icon named in a scene or Utility file, as a path under
		// Data\Interface\ for the movie. An icon is a .dds (mounted through
		// F4SE's MountImage by the movie) or a .swf; a name without an
		// extension gets .dds:
		// "4Stim/positional/standup_f" -> "4Stim/Icons/4Stim/positional/standup_f.dds".
		std::string IconPath(const std::string& a_icon)
		{
			if (a_icon.empty()) {
				return {};
			}
			auto path = std::string(ICON_DIR) + Slashes(a_icon);
			const auto slash = path.find_last_of('/');
			if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos) {
				path += ".dds";
			}
			return path;
		}


		std::optional<json> ReadJson(const std::filesystem::path& a_path)
		{
			std::ifstream in(a_path);
			if (!in) {
				return std::nullopt;
			}
			try {
				return json::parse(in, nullptr, true, true);  // comments allowed
			} catch (const std::exception& e) {
				REX::WARN("HUD: {}: bad JSON ({})", a_path.filename().string(), e.what());
				return std::nullopt;
			}
		}

		json LoadTheme(const std::string& a_name)
		{
			auto theme = json::parse(DEFAULT_THEME);
			const auto path = std::filesystem::path(THEME_DIR) / (a_name + ".json");
			const auto file = ReadJson(path);
			if (!file || !file->is_object()) {
				if (Lower(a_name) != "color") {
					REX::WARN("HUD: theme \"{}\" not found or unreadable ({}), using the built-in Color theme", a_name, path.string());
				}
				return theme;
			}
			if (const auto tint = file->find("tint"); tint != file->end() && tint->is_string()) {
				auto& colors = theme["colors"];
				for (const auto key : TINT_KEYS) {
					auto& slot = colors[std::string(key)];
					slot = slot.is_array() ? json::array({ *tint }) : *tint;
				}
			}
			theme.merge_patch(*file);
			if (const auto logo = theme.find("logoMovie"); logo != theme.end() && logo->is_string()) {
				*logo = Slashes(logo->get<std::string>());
			}
			REX::INFO("HUD: theme \"{}\" loaded", a_name);
			return theme;
		}

		std::vector<UtilityEntry> LoadUtility()
		{
			std::vector<UtilityEntry> entries;
			std::error_code           ec;
			if (!std::filesystem::is_directory(UTILITY_DIR, ec)) {
				return entries;
			}
			std::vector<std::filesystem::path> files;
			for (const auto& item : std::filesystem::directory_iterator(UTILITY_DIR, ec)) {
				if (item.is_regular_file() && Lower(item.path().extension().string()) == ".json") {
					files.push_back(item.path());
				}
			}
			std::ranges::sort(files);  // a later file replaces an earlier one's entry with the same id

			for (const auto& path : files) {
				const auto file = ReadJson(path);
				const auto list = file && file->is_object() ? file->find("entries") : json::const_iterator{};
				if (!file || !file->is_object() || list == file->end() || !list->is_array()) {
					REX::WARN("HUD: {}: no \"entries\" list, skipped", path.filename().string());
					continue;
				}
				for (const auto& item : *list) {
					try {
						UtilityEntry entry;
						entry.id = item.value("id", std::string{});
						entry.label = item.value("label", std::string{});
						entry.icon = item.value("icon", std::string{});
						entry.script = item.value("script", std::string{});
						entry.function = item.value("function", std::string{});
						entry.order = item.value("order", 1000);
						if (const auto counts = item.find("actors"); counts != item.end() && counts->is_array()) {
							for (const auto& n : *counts) {
								entry.actorCounts.push_back(n.get<int>());
							}
						}
						if (entry.id.empty() || entry.label.empty() || entry.script.empty() || entry.function.empty()) {
							REX::WARN("HUD: {}: a Utility entry is missing id, label, script or function, skipped", path.filename().string());
							continue;
						}
						std::erase_if(entries, [&](const UtilityEntry& e) { return Lower(e.id) == Lower(entry.id); });
						entries.push_back(std::move(entry));
					} catch (const std::exception& e) {
						REX::WARN("HUD: {}: bad Utility entry ({}), skipped", path.filename().string(), e.what());
					}
				}
			}
			std::ranges::sort(entries, [](const UtilityEntry& a, const UtilityEntry& b) {
				return a.order != b.order ? a.order < b.order : Lower(a.label) < Lower(b.label);
			});
			REX::INFO("HUD: {} Utility entr{} loaded", entries.size(), entries.size() == 1 ? "y" : "ies");
			return entries;
		}

		// JSON to an AS3 value. Strings are copied by SetMember/PushBack, so
		// a_json only has to outlive this call.
		void ToValue(Scaleform::GFx::Movie& a_movie, const json& a_json, GValue& a_out)
		{
			switch (a_json.type()) {
			case json::value_t::object:
				a_movie.CreateObject(&a_out);
				for (const auto& [key, item] : a_json.items()) {
					GValue member;
					ToValue(a_movie, item, member);
					a_out.SetMember(key, member);
				}
				break;
			case json::value_t::array:
				a_movie.CreateArray(&a_out);
				for (const auto& item : a_json) {
					GValue element;
					ToValue(a_movie, item, element);
					a_out.PushBack(element);
				}
				break;
			case json::value_t::string:
				a_out = a_json.get_ref<const std::string&>().c_str();
				break;
			case json::value_t::boolean:
				a_out = a_json.get<bool>();
				break;
			case json::value_t::number_integer:
			case json::value_t::number_unsigned:
			case json::value_t::number_float:
				a_out = a_json.get<double>();
				break;
			default:
				a_out = nullptr;
				break;
			}
		}

		double ToNumber(const GValue& a_value)
		{
			if (a_value.IsNumber()) {
				return a_value.GetNumber();
			}
			if (a_value.IsInt()) {
				return a_value.GetInt();
			}
			if (a_value.IsUInt()) {
				return a_value.GetUInt();
			}
			return 0.0;
		}

		void RunUtility(const std::string& a_id)
		{
			const auto scene = FourStim::GetFocusedScene();
			const auto vm = FourStim::GetVM();
			if (!scene.Active() || !vm) {
				return;
			}
			// Built in: undress or dress someone in the scene by hand.
			if (a_id.starts_with(STRIP_PREFIX) || a_id.starts_with(DRESS_PREFIX)) {
				const bool strip = a_id.starts_with(STRIP_PREFIX);
				try {
					const auto id = static_cast<std::uint32_t>(std::stoul(a_id.substr(STRIP_PREFIX.size()), nullptr, 16));
					REX::INFO("HUD: {} {:08X} by hand", strip ? "undressing" : "dressing", id);
					if (strip) {
						Undress::StripAll(id, true);
					} else {
						Undress::Redress(id, true);
					}
				} catch (const std::exception&) {
					REX::WARN("HUD: bad Utility id \"{}\"", a_id);
				}
				return;
			}
			UtilityEntry entry;
			{
				std::scoped_lock lock(g_configLock);
				const auto       it = std::ranges::find_if(g_utility, [&](const UtilityEntry& e) { return e.id == a_id; });
				if (it == g_utility.end()) {
					REX::WARN("HUD: no Utility entry \"{}\"", a_id);
					return;
				}
				entry = *it;
			}
			REX::INFO("HUD: Utility \"{}\" -> {}.{}", entry.id, entry.script, entry.function);
			const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall(entry.script, entry.function, callback, FourStim::ResolveActors(scene.ActorIDs()), scene.sceneID);
		}

		class HUDMenu;
		HUDMenu* g_menu = nullptr;  // the open HUD, main thread only

		void ShowMenu(bool a_show);

		// Whether the HUD should be on screen right now.
		bool Wanted()
		{
			bool enabled;
			{
				std::scoped_lock lock(g_configLock);
				enabled = g_enabled;
			}
			return enabled && FourStim::GetFocusedScene().Active();
		}

		// While the HUD is focused, an input layer keeps the game from also
		// acting on the HUD's keys (A activates, B opens the Pip-Boy, the
		// d-pad is favorites, Esc the pause menu...). Looking and movement
		// stay on, so the free camera still works. Main thread only.
		RE::BSTSmartPointer<RE::BSInputEnableLayer> g_focusLayer;

		void SetFocusLayer(bool a_on)
		{
			const auto manager = RE::BSInputEnableManager::GetSingleton();
			if (!manager) {
				return;
			}
			using U = RE::UserEvents::USER_EVENT_FLAG;
			using O = RE::OtherInputEvents::OTHER_EVENT_FLAG;
			const auto user = static_cast<std::int32_t>(U::kActivate) | static_cast<std::int32_t>(U::kMenu) |
			                  static_cast<std::int32_t>(U::kFighting) | static_cast<std::int32_t>(U::kJumping) |
			                  static_cast<std::int32_t>(U::kVATS) | static_cast<std::int32_t>(U::kMainFour) |
			                  static_cast<std::int32_t>(U::kPOVSwitch) | static_cast<std::int32_t>(U::kSneaking);
			const auto other = static_cast<std::int32_t>(O::kFavorites) | static_cast<std::int32_t>(O::kActivation) |
			                   static_cast<std::int32_t>(O::kJournalTabs) | static_cast<std::int32_t>(O::kVATS) |
			                   static_cast<std::int32_t>(O::kPOVChange) | static_cast<std::int32_t>(O::kPipboyLight);
			if (a_on && !g_focusLayer) {
				if (!manager->AllocateNewLayer(g_focusLayer, "4Stim HUD") || !g_focusLayer) {
					REX::WARN("HUD: couldn't allocate an input layer for focus");
					return;
				}
				manager->EnableUserEvent(g_focusLayer->layerID, static_cast<U>(user), false, RE::UserEvents::SENDER_ID::kScript);
				manager->EnableOtherEvent(g_focusLayer->layerID, static_cast<O>(other), false, RE::UserEvents::SENDER_ID::kScript);
			} else if (!a_on && g_focusLayer) {
				manager->EnableUserEvent(g_focusLayer->layerID, U::kAll, true, RE::UserEvents::SENDER_ID::kScript);
				manager->EnableOtherEvent(g_focusLayer->layerID, O::kAll, true, RE::UserEvents::SENDER_ID::kScript);
				g_focusLayer.reset();
			}
		}

		class HUDMenu : public RE::IMenu
		{
		public:
			static RE::IMenu* Create(const RE::UIMessage&) { return new HUDMenu(); }

			HUDMenu()
			{
				using F = RE::UI_MENU_FLAGS;
				// No pause, cursor, menu context or modal flag: the HUD sits on
				// screen without taking the player's controls (the free camera
				// keeps working). Input reaches it only through HUD::HandleInput,
				// while focused. kAdvancesUnderPauseMenu keeps it updating while
				// another menu pauses the game, so it hears about the pause
				// (SetPaused) and the logo movie can stop itself.
				for (const auto f : { F::kAllowSaving, F::kAdvancesUnderPauseMenu, F::kRendersUnderPauseMenu }) {
					UpdateFlag(f, true);
				}
				depthPriority = RE::UI_DEPTH_PRIORITY::kHUD;
				inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kNone;
				g_menu = this;
				g_showRequested = false;
				if (!Wanted()) {
					ShowMenu(false);  // the scene ended while the menu was opening
				}

				const auto scaleform = RE::BSScaleformManager::GetSingleton();
				// The game's own LoadMovie (not CommonLib's LoadMovieEx copy of
				// it): F4SE hooks it to give the movie its Scaleform functions
				// (root.f4se), which the HUD needs to show .dds icons.
				bool loaded = scaleform && scaleform->LoadMovie(*this, uiMovie, MENU_NAME, "root1.Menu_mc");
				if (!loaded && scaleform) {
					REX::WARN("HUD: the game's LoadMovie failed; loading directly (no F4SE functions, so no .dds icons)");
					loaded = scaleform->LoadMovieEx(*this, "Interface/FourStimHUDMenu.swf"sv, ""sv);
				}
				if (!loaded) {
					REX::WARN("HUD: couldn't load Interface/FourStimHUDMenu.swf");
					return;
				}
				LinkMenuObject();
				if (menuObj.IsObject()) {
					_loaded = true;
					CheckApiVersion();
					SendAll();
					// The HUD takes the navigation keys as soon as it's up; the
					// hotkey (or Cancel) hands them back to the game.
					HUD::SetFocus(true);
				}
			}

			~HUDMenu() override
			{
				if (g_menu == this) {
					g_menu = nullptr;
				}
				g_focused = false;
				SetFocusLayer(false);
				// A scene started while this menu was closing: open a new one
				// (unless that's already been asked for).
				if (Wanted() && !g_showRequested.exchange(true)) {
					ShowMenu(true);
				}
			}

			// The engine can let go of the movie before this object is destroyed
			// (while the menu closes), so check the movie itself every time.
			[[nodiscard]] bool Loaded() const { return _loaded && uiMovie && menuObj.IsObject(); }

			// The HUD never takes input from the game's menu system; focused
			// input comes from HUD::HandleInput instead.
			bool ShouldHandleEvent(const RE::InputEvent*) override { return false; }

			void AdvanceMovie(float a_timeDelta, std::uint64_t a_time) override
			{
				RE::IMenu::AdvanceMovie(a_timeDelta, a_time);
				if (!Loaded()) {
					return;
				}

				const auto ui = RE::UI::GetSingleton();
				const bool paused = ui && ui->menuMode > 0;
				if (paused != _paused) {
					_paused = paused;
					if (paused && _heldDir != 0) {
						// The key's release won't reach the HUD while paused.
						GValue release[2]{ HeldName(), false };
						InvokeRequired("ProcessUserEvent", release, 2);
						_heldDir = 0;
					}
					GValue arg(paused);
					InvokeOptional("SetPaused", &arg, 1);
				}

				const auto rect = uiMovie->GetVisibleFrameRect();
				if (rect.x1 != _screen.x1 || rect.y1 != _screen.y1 || rect.x2 != _screen.x2 || rect.y2 != _screen.y2) {
					SendScreen();
				}

				RepeatHeldNavigation();
				UpdateMeters(a_timeDelta);
			}

			void MapCodeObjectFunctions() override
			{
				MapCodeMethodToASFunction("Ready", 0);
				MapCodeMethodToASFunction("Navigate", 1);
				MapCodeMethodToASFunction("EndScene", 2);
				MapCodeMethodToASFunction("ChangeSpeed", 3);
				MapCodeMethodToASFunction("RunUtility", 4);
				MapCodeMethodToASFunction("OpenSearch", 5);
				MapCodeMethodToASFunction("ReleaseFocus", 6);
				MapCodeMethodToASFunction("Log", 7);
				MapCodeMethodToASFunction("AlignAdjust", 8);   // (field, direction)
				MapCodeMethodToASFunction("AlignActor", 9);    // (direction)
				MapCodeMethodToASFunction("AlignStep", 10);    // (direction)
				MapCodeMethodToASFunction("AlignReset", 11);
				MapCodeMethodToASFunction("SetHorizontalRepeat", 12);  // (on)
			}

			void Call(const Params& a_params) override
			{
				const auto string0 = [&]() -> std::string {
					return a_params.argCount > 0 && a_params.args[0].IsString() ? a_params.args[0].GetString() : std::string{};
				};
				switch (reinterpret_cast<std::uintptr_t>(a_params.userData)) {
				case 0:  // Ready()
					REX::INFO("HUD: movie ready");
					CheckApiVersion();
					SendAll();
					break;
				case 1:  // Navigate(id)
					if (const auto id = string0(); !id.empty()) {
						FourStim::NavigateFocused(id);
					}
					break;
				case 2:  // EndScene()
					SetFocus(false);
					FourStim::EndFocusedScene();
					break;
				case 3:  // ChangeSpeed(delta)
					if (a_params.argCount > 0) {
						const auto delta = static_cast<int>(ToNumber(a_params.args[0]));
						if (delta != 0) {
							FourStim::ChangeFocusedSpeed(delta > 0 ? 1 : -1);
						}
					}
					break;
				case 4:  // RunUtility(id)
					if (const auto id = string0(); !id.empty()) {
						RunUtility(id);
					}
					break;
				case 5:  // OpenSearch()
					FourStim::OpenSearchForFocused();
					break;
				case 6:  // ReleaseFocus()
					SetFocus(false);
					break;
				case 7:  // Log(message)
					REX::INFO("HUD movie: {}", string0());
					break;
				case 8:  // AlignAdjust(field, direction)
					if (a_params.argCount > 1) {
						AlignAdjust(string0(), ToNumber(a_params.args[1]) < 0 ? -1.0F : 1.0F);
					}
					break;
				case 9:  // AlignActor(direction)
					if (const auto count = FourStim::GetFocusedScene().ActorIDs().size(); count > 0) {
						const int dir = a_params.argCount > 0 && ToNumber(a_params.args[0]) < 0 ? -1 : 1;
						g_alignRole = (g_alignRole + count + dir) % count;
						SendAlign();
					}
					break;
				case 10:  // AlignStep(direction)
					{
						const int dir = a_params.argCount > 0 && ToNumber(a_params.args[0]) < 0 ? -1 : 1;
						g_alignStep = (g_alignStep + ALIGN_STEPS.size() + dir) % ALIGN_STEPS.size();
						SendAlign();
					}
					break;
				case 11:  // AlignReset()
					FourStim::SetFocusedAlignment(g_alignRole, {});
					SendAlign();
					break;
				case 12:  // SetHorizontalRepeat(on)
					_horizontalRepeat = a_params.argCount > 0 && a_params.args[0].IsBool() && a_params.args[0].GetBool();
					break;
				default:
					break;
				}
			}

			// Everything the movie shows: sent when it opens, when it says it's
			// ready, and when the theme or Utility entries are reloaded.
			void SendAll()
			{
				if (!Loaded()) {
					return;
				}
				SendScreen();
				SendTheme();
				SendSceneData();
			}

			// The scene's data, sent whenever the focused scene changes (start,
			// navigation, speed). Not the theme: re-applying it rebuilds the
			// whole HUD.
			void SendSceneData()
			{
				if (!Loaded()) {
					return;
				}
				const auto focused = FourStim::GetFocusedScene();
				const auto scene = focused.Active() ? SceneRegistry::Find(focused.sceneID) : nullptr;

				SendScene(scene.get());
				SendActors(focused);
				SendSpeed(focused, scene.get());
				// During a transition, the options of where it's going.
				SendNavigation(focused, scene ? SceneRegistry::Settled(scene).get() : nullptr);
				SendUtility(focused);
				SendAlign();
				SendFocus();
			}

			// The Align tab's data (optional HUD API function SetAlign,
			// HUD_API.md): the role being adjusted, its name, its offset in
			// this scene and the step size.
			void SendAlign()
			{
				if (!Loaded()) {
					return;
				}
				const auto focused = FourStim::GetFocusedScene();
				const auto ids = focused.ActorIDs();
				if (g_alignRole >= ids.size()) {
					g_alignRole = 0;
				}
				Alignment::Offset offset;
				std::string       sceneID;
				const bool        ok = !ids.empty() && FourStim::GetFocusedAlignment(g_alignRole, offset, sceneID);
				std::string       name = "Partner";
				if (const auto actor = ok ? RE::TESForm::GetFormByID<RE::Actor>(ids[g_alignRole]) : nullptr) {
					if (actor == RE::PlayerCharacter::GetSingleton()) {
						name = "You";
					} else if (const auto n = actor->GetDisplayFullName(); n && *n) {
						name = n;
					}
				}
				GValue data;
				uiMovie->CreateObject(&data);
				data.SetMember("available"sv, GValue(ok));
				data.SetMember("role"sv, GValue(static_cast<double>(g_alignRole)));
				data.SetMember("roleCount"sv, GValue(static_cast<double>(ids.size())));
				data.SetMember("name"sv, GValue(name.c_str()));
				data.SetMember("x"sv, GValue(static_cast<double>(offset.x)));
				data.SetMember("y"sv, GValue(static_cast<double>(offset.y)));
				data.SetMember("z"sv, GValue(static_cast<double>(offset.z)));
				data.SetMember("rot"sv, GValue(static_cast<double>(offset.rot)));
				data.SetMember("scale"sv, GValue(static_cast<double>(offset.scale)));
				data.SetMember("step"sv, GValue(static_cast<double>(ALIGN_STEPS[g_alignStep])));
				InvokeOptional("SetAlign", &data, 1);
			}

			// One step of a field of the role being adjusted, a_dir +1 / -1.
			void AlignAdjust(const std::string& a_field, float a_dir)
			{
				Alignment::Offset offset;
				std::string       sceneID;
				if (!FourStim::GetFocusedAlignment(g_alignRole, offset, sceneID)) {
					return;
				}
				const float step = ALIGN_STEPS[g_alignStep] * a_dir;
				auto        snap = [](float a_value, float a_grid) { return std::round(a_value / a_grid) * a_grid; };
				if (a_field == "x") {
					offset.x = snap(offset.x + step, 0.1F);
				} else if (a_field == "y") {
					offset.y = snap(offset.y + step, 0.1F);
				} else if (a_field == "z") {
					offset.z = snap(offset.z + step, 0.1F);
				} else if (a_field == "rot") {
					offset.rot = std::fmod(snap(offset.rot + step, 0.1F) + 540.0F, 360.0F) - 180.0F;
				} else if (a_field == "scale") {
					offset.scale = std::clamp(snap(offset.scale + step / 100.0F, 0.005F), 0.5F, 2.0F);
				} else {
					return;
				}
				FourStim::SetFocusedAlignment(g_alignRole, offset);
				SendAlign();
			}

			void SendFocus()
			{
				GValue arg(g_focused.load());
				InvokeRequired("SetFocus", &arg, 1);
				_heldDir = 0;
			}

			// A navigation key went down (-1 up, +1 down) or up (0), for
			// hold-to-scroll.
			void SetHeld(int a_dir)
			{
				_heldDir = a_dir;
				_heldSince = std::chrono::steady_clock::now();
				_repeats = 0;
			}

			bool WantsHorizontalRepeat() const { return _horizontalRepeat; }

			// Invoke fails when the movie has no such function; that's logged
			// once per function rather than every frame.
			bool InvokeOptionalPublic(const char* a_name, const GValue* a_args, std::size_t a_count)
			{
				return InvokeOptional(a_name, a_args, a_count);
			}

			bool InvokeRequired(const char* a_name, const GValue* a_args, std::size_t a_count)
			{
				if (!Loaded()) {
					return false;
				}
				const bool ok = menuObj.Invoke(a_name, nullptr, a_args, a_count);
				if (!ok && _warnedMissing.insert(a_name).second) {
					REX::WARN("HUD: calling {} on the movie failed; does it have that function? (see HUD_API.md)", a_name);
				}
				return ok;
			}

		private:
			// An actor's excitement as a meter fill, 0 to 1, or -1 for none.
			static double MeterOf(std::uint32_t a_id)
			{
				if (!Excitement::Settings().enabled) {
					return -1.0;
				}
				const float value = Excitement::Get(a_id);
				return value < 0.0F ? -1.0 : static_cast<double>(value) / 100.0;
			}

			// Excitement changes all the time: the meters are refreshed ten
			// times a second, when a value moved.
			void UpdateMeters(float)
			{
				// Wall-clock time: the frame delta the game passes a HUD-depth
				// menu isn't reliable.
				const auto now = std::chrono::steady_clock::now();
				if (now - _meterLast < std::chrono::milliseconds(100)) {
					return;
				}
				_meterLast = now;
				const auto focused = FourStim::GetFocusedScene();
				if (!focused.Active()) {
					_lastMeters.clear();
					return;
				}
				std::vector<double> values;
				for (const auto id : focused.ActorIDs()) {
					values.push_back(MeterOf(id));
				}
				if (values == _lastMeters) {
					return;
				}
				_lastMeters = values;
				GValue list;
				uiMovie->CreateArray(&list);
				for (const auto v : values) {
					list.PushBack(GValue(v));
				}
				const bool ok = InvokeOptional("SetMeters", &list, 1);
				if (!_metersLogged) {
					_metersLogged = true;
					REX::INFO("HUD: meters {} ({} value(s), first {:.2f})", ok ? "updating" : "NOT updating: SetMeters failed", values.size(), values.empty() ? -1.0 : values[0]);
				}
			}

			std::chrono::steady_clock::time_point _meterLast{};
			bool                                  _metersLogged = false;
			std::vector<double>                   _lastMeters;
			bool                                  _loaded = false;
			bool                                  _paused = false;
			Scaleform::Render::Rect<float>        _screen{};
			int                                   _heldDir = 0;   // -1 up, +1 down, -2 left, +2 right, 0 none
			bool                                  _horizontalRepeat = false;  // the movie wants Left / Right repeated (Align tab)
			std::chrono::steady_clock::time_point _heldSince{};
			int                                   _repeats = 0;
			std::set<std::string>                 _warnedMissing;

			bool InvokeOptional(const char* a_name, const GValue* a_args, std::size_t a_count)
			{
				return Loaded() && menuObj.Invoke(a_name, nullptr, a_args, a_count);
			}

			// Finds the AS3 menu clip and attaches BGSCodeObj to it, trying the
			// same paths as the picker (Flex-built movies name their root
			// differently from the game's own).
			void LinkMenuObject()
			{
				constexpr std::array paths{ "root1.Menu_mc", "root.Menu_mc", "Menu_mc", "_root.Menu_mc", "root1", "root" };
				for (const auto path : paths) {
					GValue candidate;
					if (uiMovie->GetVariable(&candidate, path) && candidate.IsObject()) {
						menuObj = candidate;
						RegisterCodeObject(*uiMovie, menuObj);
						REX::INFO("HUD: menu clip found at \"{}\"", path);
						return;
					}
				}
				REX::WARN("HUD: couldn't find the menu clip (tried root1.Menu_mc, root.Menu_mc, Menu_mc, _root.Menu_mc, root1, root)");
			}

			void CheckApiVersion()
			{
				GValue result;
				if (!menuObj.Invoke("GetApiVersion", &result)) {
					REX::WARN("HUD: the movie has no GetApiVersion; it may not be a 4Stim HUD");
					return;
				}
				const auto version = static_cast<int>(ToNumber(result));
				if (version != API_VERSION) {
					REX::WARN("HUD: the movie was made for HUD API version {}, this 4Stim uses version {}. Parts of it may not work.",
						version, API_VERSION);
				}
			}

			void SendScreen()
			{
				_screen = uiMovie->GetVisibleFrameRect();
				GValue args[4]{ static_cast<double>(_screen.x1), static_cast<double>(_screen.y1),
					static_cast<double>(_screen.x2), static_cast<double>(_screen.y2) };
				InvokeRequired("SetScreen", args, 4);
			}

			void SendTheme()
			{
				json theme;
				{
					std::scoped_lock lock(g_configLock);
					theme = g_theme.is_object() ? g_theme : json::parse(DEFAULT_THEME);
				}
				GValue value;
				ToValue(*uiMovie, theme, value);
				InvokeRequired("SetTheme", &value, 1);
			}

			void SendScene(const SceneRegistry::Scene* a_scene)
			{
				GValue value, id, name, count, tags;
				uiMovie->CreateObject(&value);
				uiMovie->CreateArray(&tags);
				if (a_scene) {
					id = a_scene->id.c_str();
					name = a_scene->name.c_str();
					count = static_cast<double>(a_scene->actors.size());
					for (const auto& tag : a_scene->tags) {
						tags.PushBack(GValue(tag.c_str()));
					}
				} else {
					id = "";
					name = "";
					count = 0.0;
				}
				value.SetMember("id"sv, id);
				value.SetMember("name"sv, name);
				value.SetMember("actorCount"sv, count);
				value.SetMember("tags"sv, tags);
				InvokeRequired("SetScene", &value, 1);
			}

			void SendActors(const FourStim::FocusedScene& a_scene)
			{
				GValue list;
				uiMovie->CreateArray(&list);
				const auto ids = a_scene.ActorIDs();
				for (std::size_t role = 0; role < ids.size(); ++role) {
					const auto  actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
					std::string name = "Partner";
					const char* sex = "other";
					bool        player = false;
					if (actor) {
						if (const auto n = actor->GetDisplayFullName(); n && *n) {
							name = n;
						}
						const auto s = actor->GetSex();
						sex = s == RE::SEX::kMale ? "male" : s == RE::SEX::kFemale ? "female" : "other";
						player = actor == RE::PlayerCharacter::GetSingleton();
					}
					GValue entry;
					uiMovie->CreateObject(&entry);
					entry.SetMember("role"sv, GValue(static_cast<double>(role)));
					entry.SetMember("name"sv, GValue(name.c_str()));
					entry.SetMember("sex"sv, GValue(sex));
					entry.SetMember("isPlayer"sv, GValue(player));
					entry.SetMember("meter"sv, GValue(MeterOf(ids[role])));
					list.PushBack(entry);
				}
				InvokeRequired("SetActors", &list, 1);
			}

			void SendSpeed(const FourStim::FocusedScene& a_focused, const SceneRegistry::Scene* a_scene)
			{
				const int count = a_scene ? static_cast<int>(a_scene->speeds.size()) : 0;
				GValue    args[2]{ static_cast<double>(std::clamp(a_focused.speed, 0, std::max(count - 1, 0))), static_cast<double>(count) };
				InvokeRequired("SetSpeed", args, 2);
			}

			void AddEntry(GValue& a_list, const std::string& a_id, const std::string& a_label, const std::string& a_icon, const char* a_kind)
			{
				GValue entry;
				uiMovie->CreateObject(&entry);
				entry.SetMember("id"sv, GValue(a_id.c_str()));
				entry.SetMember("label"sv, GValue(a_label.c_str()));
				entry.SetMember("icon"sv, GValue(a_icon.c_str()));
				if (a_kind) {
					entry.SetMember("kind"sv, GValue(a_kind));
				}
				a_list.PushBack(entry);
			}

			void SendNavigation(const FourStim::FocusedScene& a_focused, const SceneRegistry::Scene* a_scene)
			{
				GValue list;
				uiMovie->CreateArray(&list);
				if (a_scene) {
					for (const auto& nav : a_scene->navigations) {
						const auto dest = SceneRegistry::Find(nav.to);
						if (dest && !FourStim::FocusedCanPlay(*dest)) {
							continue;
						}
						const auto icon = IconPath(!nav.icon.empty() ? nav.icon : dest ? dest->icon : std::string{});
						AddEntry(list, nav.to, FourStim::FormatLabel(nav.label, a_focused), icon, "scene");
					}
				}
				if (a_focused.Active()) {
					// Watching an NPC scene: leave it running and go back to the game.
					const auto player = RE::PlayerCharacter::GetSingleton();
					const auto ids = a_focused.ActorIDs();
					if (player && std::ranges::find(ids, player->GetFormID()) == ids.end()) {
						AddEntry(list, std::string(STOP_WATCHING_ID), "Stop watching", "", "end");
					}
					AddEntry(list, std::string(END_ID), "End scene", "", "end");
				}
				InvokeRequired("SetNavigation", &list, 1);
			}

			void SendUtility(const FourStim::FocusedScene& a_focused)
			{
				std::vector<UtilityEntry> entries;
				{
					std::scoped_lock lock(g_configLock);
					entries = g_utility;
				}
				const auto ids = a_focused.ActorIDs();
				const int  count = static_cast<int>(ids.size());
				GValue     list;
				uiMovie->CreateArray(&list);
				// Built in: "Undress <name>" / "Dress <name>" for each actor.
				if (Undress::Settings().enabled) {
					for (const auto id : ids) {
						const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
						if (!actor) {
							continue;
						}
						const auto  player = RE::PlayerCharacter::GetSingleton();
						const char* name = actor->GetDisplayFullName();
						const std::string who = actor == player ? std::string("yourself") : std::string(name && *name ? name : "them");
						const bool  stripped = Undress::IsStripped(id);
						AddEntry(list, std::format("{}{:X}", stripped ? DRESS_PREFIX : STRIP_PREFIX, id), (stripped ? "Dress " : "Undress ") + who,
							IconPath(stripped ? "4Stim/clothes/top.swf" : "4Stim/clothes/bra.swf"), nullptr);
					}
				}
				for (const auto& entry : entries) {
					if (!entry.actorCounts.empty() && std::ranges::find(entry.actorCounts, count) == entry.actorCounts.end()) {
						continue;
					}
					AddEntry(list, entry.id, FourStim::FormatLabel(entry.label, a_focused), IconPath(entry.icon), nullptr);
				}
				InvokeRequired("SetUtility", &list, 1);
			}

			// Hold-to-scroll while focused: the movie gets a fresh Up/Down every
			// REPEAT_RATE seconds after REPEAT_DELAY. Real-time clock, so it
			// works the same whether or not the game is paused.
			const char* HeldName() const
			{
				return _heldDir == -1 ? "Up" : _heldDir == 1 ? "Down" : _heldDir == -2 ? "Left" : "Right";
			}

			void RepeatHeldNavigation()
			{
				if (_heldDir == 0 || !g_focused || _paused) {
					return;
				}
				constexpr double REPEAT_DELAY = 0.35;
				constexpr double REPEAT_RATE = 0.06;
				const double     held = std::chrono::duration<double>(std::chrono::steady_clock::now() - _heldSince).count();
				const int        due = held < REPEAT_DELAY ? 0 : static_cast<int>((held - REPEAT_DELAY) / REPEAT_RATE) + 1;
				for (int i = 0; _repeats < due && i < 3; ++i) {  // at most 3 per frame, so a hitch can't jump far
					++_repeats;
					GValue args[2]{ HeldName(), true };
					InvokeRequired("ProcessUserEvent", args, 2);
				}
			}
		};

		void ShowMenu(bool a_show)
		{
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(MENU_NAME, a_show ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
			}
		}

		// Maps a button to the HUD's input names (HUD_API.md). Keyboard codes
		// are Windows virtual-key codes.
		const char* InputName(const RE::ButtonEvent* a_event)
		{
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			switch (a_event->device.get()) {
			case RE::INPUT_DEVICE::kKeyboard:
				switch (code) {
				case 0x26: return "Up";
				case 0x28: return "Down";
				case 0x25: return "Left";
				case 0x27: return "Right";
				case 0x0D: return "Accept";  // Enter
				case 0x1B: return "Cancel";  // Esc
				case 0x08: return "Cancel";  // Backspace
				default: return nullptr;
				}
			case RE::INPUT_DEVICE::kGamepad:
				switch (code) {
				case 0x0001: return "Up";      // d-pad
				case 0x0002: return "Down";
				case 0x0004: return "Left";
				case 0x0008: return "Right";
				case 0x1000: return "Accept";  // A
				case 0x2000: return "Cancel";  // B
				case 0x0100: return "PrevTab"; // LB
				case 0x0200: return "NextTab"; // RB
				case 0x8000: return "SpeedUp";    // Y
				case 0x4000: return "SpeedDown";  // X
				default: return nullptr;
				}
			default:
				return nullptr;
			}
		}
	}

	void SetOptions(std::string a_theme, bool a_enabled)
	{
		std::scoped_lock lock(g_configLock);
		g_themeName = a_theme.empty() ? "Color" : std::move(a_theme);
		g_enabled = a_enabled;
	}

	void LoadConfig()
	{
		std::string name;
		{
			std::scoped_lock lock(g_configLock);
			name = g_themeName;
		}
		auto theme = LoadTheme(name);
		auto utility = LoadUtility();
		{
			std::scoped_lock lock(g_configLock);
			g_theme = std::move(theme);
			g_utility = std::move(utility);
		}
		F4SE::GetTaskInterface()->AddTask([]() {
			if (g_menu) {
				g_menu->SendAll();  // an open HUD picks up the new theme and entries
			}
		});
	}

	void RegisterMenu()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			ui->RegisterMenu(MENU_NAME, HUDMenu::Create);
			REX::INFO("HUD menu registered");
		}
	}

	void OnFocusedSceneChanged()
	{
		// Opening and closing are queued UI messages, so the menu can be on
		// its way in or out when this runs; the menu's constructor and
		// destructor check Wanted() again to settle any race.
		F4SE::GetTaskInterface()->AddTask([]() {
			if (Wanted()) {
				if (g_menu && g_menu->Loaded()) {
					g_menu->SendSceneData();
				} else if (!g_showRequested.exchange(true)) {
					ShowMenu(true);  // the new menu sends everything itself
				}
				return;
			}
			if (g_focused) {
				g_focused = false;
				SetFocusLayer(false);
			}
			if (g_menu) {
				ShowMenu(false);
			}
		});
	}

	void Reset()
	{
		g_focused = false;
		SetFocusLayer(false);
		g_showRequested = false;
		if (g_menu || (RE::UI::GetSingleton() && RE::UI::GetSingleton()->GetMenuOpen(MENU_NAME))) {
			ShowMenu(false);
		}
	}

	void PlayClimax(float a_strength)
	{
		F4SE::GetTaskInterface()->AddTask([a_strength]() {
			if (g_menu && g_menu->Loaded()) {
				GValue arg(static_cast<double>(a_strength));
				g_menu->InvokeOptionalPublic("PlayClimax", &arg, 1);
			}
		});
	}

	bool IsOpen()
	{
		return g_menu && g_menu->Loaded();
	}

	bool IsFocused()
	{
		return g_focused;
	}

	void SetFocus(bool a_focused)
	{
		F4SE::GetTaskInterface()->AddTask([a_focused]() {
			const bool focused = a_focused && IsOpen();
			if (focused == g_focused) {
				return;
			}
			g_focused = focused;
			SetFocusLayer(focused);
			if (g_menu) {
				g_menu->SendFocus();
			}
			REX::INFO("HUD: {}", focused ? "focused" : "unfocused");
		});
	}

	bool HandleInput(const RE::ButtonEvent* a_event)
	{
		if (!g_focused || !g_menu || !g_menu->Loaded()) {
			return false;
		}
		const auto name = InputName(a_event);
		if (!name) {
			return false;
		}
		const std::string_view n(name);
		const bool             upDown = n == "Up"sv || n == "Down"sv;
		const bool             leftRight = (n == "Left"sv || n == "Right"sv) && g_menu->WantsHorizontalRepeat();
		if (a_event->QJustPressed()) {
			if (upDown || leftRight) {
				g_menu->SetHeld(n == "Up"sv ? -1 : n == "Down"sv ? 1 : n == "Left"sv ? -2 : 2);
			}
			GValue args[2]{ name, true };
			g_menu->InvokeRequired("ProcessUserEvent", args, 2);
		} else if (!a_event->QPressed()) {
			if (upDown || leftRight) {
				g_menu->SetHeld(0);
			}
			GValue args[2]{ name, false };
			g_menu->InvokeRequired("ProcessUserEvent", args, 2);
		}
		return true;
	}
}
