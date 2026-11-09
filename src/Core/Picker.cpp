#include "Core/Core.h"

namespace Core
{
	// Picker IDs of sequences (vs scenes) start with this.
	constexpr std::string_view SEQUENCE_PREFIX = "seq:"sv;
	constexpr auto CHANGE_FURNITURE_ID = "__furniture__";

	// ---- Concurrent scenes: watching one ----
	// Any number of scenes run at once; the HUD and the free camera follow
	// the focused one (the player's, or one the player watches).

	bool PlayerInAScene()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		return player && FindActiveScene(player->GetFormID()) != nullptr;
	}

	// Main thread. Makes a running NPC scene the focused one, with the free
	// camera, as if the player had started it to watch.
	void WatchScene(std::uint32_t a_role0)
	{
		const auto active = FindActiveScene(a_role0);
		if (!active) {
			return;
		}
		if (PlayerInAScene()) {
			Notify(TR("$4stim_in_scene_cant_watch", "4Stim: you're in a scene; end it to watch another"));
			return;
		}
		auto focus = FocusFor(active->actors, active->sceneID);
		focus.speed = active->speed;
		focus.furniture = active->furnitureType;
		SetPlayerScene(focus);
		BeginSceneCamera({});
		REX::INFO("Watching \"{}\"", active->sceneID);
	}

	// Main thread. Stops watching the focused NPC scene (it keeps running).
	void StopWatching()
	{
		const auto focused = GetPlayerScene();
		if (!focused.Active() || PlayerInAScene()) {
			return;
		}
		SetPlayerScene({});
		EndSceneCamera({});
		REX::INFO("Stopped watching \"{}\"", focused.sceneID);
	}

	// The picker's commands for a running scene (role 0's form ID). Main thread.
	void RunningSceneCommand(const std::string& a_command, std::uint32_t a_role0)
	{
		const auto active = FindActiveScene(a_role0);
		if (!active) {
			Notify(TR("$4stim_scene_ended", "4Stim: that scene has ended"));
			return;
		}
		if (a_command == "@watch") {
			WatchScene(a_role0);
		} else if (a_command == "@auto") {
			active->autoMode.on ? StopAutoMode(*active) : StartAutoMode(*active);
			Notify(active->autoMode.on ? TR("$4stim_auto_mode_on", "Auto mode on") : TR("$4stim_auto_mode_off", "Auto mode off"));
		} else if (a_command == "@endscene") {
			EndActiveScene(*active, "ended from the picker");
		}
	}
	std::atomic<PickerMode>    g_pickerMode = PickerMode::kStart;
	std::mutex              g_questionLock;
	std::optional<Question> g_question;
	// OStim's search menu: what's been typed in the picker's search mode
	// (UTF-8, main thread) and whether the picker is taking typing now.
	std::string                g_searchQuery;
	std::atomic<bool>          g_searchTyping = false;

	// ---- Starting a scene: who, where, then an idle (like OStim) ----
	// kStart walks through steps in the same picker list: the actors (the
	// player plus any nearby NPCs picked, one at a time), then where (right
	// here or a piece of furniture near), and then starts a random "idle"
	// scene that fits them; or the full list of scenes that fit, to browse.
	enum class StartStep
	{
		kActors,
		kRole,      // which role the player takes (playerSelectRole...)
		kPlace,
		kBrowse,
		kRunning,  // the scenes running now (to watch, end, or put in auto mode)
		kScene     // one of them
	};
	// A running scene as the picker shows it (copied on the main thread when
	// the picker opens).
	struct RunningScene
	{
		std::vector<std::uint32_t> actors;
		std::string                sceneID;
		std::string                label;  // "Missionary: Settler, Lily"
		bool                       autoMode = false;
		bool                       withPlayer = false;
	};
	std::atomic<StartStep>     g_startStep = StartStep::kActors;
	std::mutex                 g_castLock;
	std::vector<std::uint32_t> g_cast;            // NPCs picked, in pick order
	bool                       g_includePlayer = true;  // the player is in the new scene
	bool                       g_playerBusy = false;    // the player is in a scene already (so can't be in the new one)
	std::vector<RunningScene>  g_running;         // scenes running when the picker opened
	std::uint32_t              g_selectedScene = 0;  // kScene: its role 0
	std::vector<Candidate>     g_candidates;      // eligible NPCs near the player
	int                        g_placeChoice = -1;  // index into PickerFurniture(); -1 = right here
	int                        g_playerRole = -1;   // the role the player chose (playerSelectRole...); -1 = none
	std::uint32_t              g_leadActor = 0;     // a scene without the player: the NPC chosen for the first role; 0 = none
	std::size_t                g_maxCast = 2;       // the most actors any scene has

	// Where the player goes in a cast of two by OStim's PlayerAlways...
	// settings: 0 (dominant), 1 (submissive), or -1 (no setting).
	int PlayerAlwaysIndex(const std::vector<std::uint32_t>& a_cast)
	{
		if (a_cast.size() != 2) {
			return -1;
		}
		const bool same = ActorProperties::Cached(a_cast[0]).penis == ActorProperties::Cached(a_cast[1]).penis;
		if (same ? g_settings.playerAlwaysDomGay : g_settings.playerAlwaysDomStraight) {
			return 0;
		}
		if (same ? g_settings.playerAlwaysSubGay : g_settings.playerAlwaysSubStraight) {
			return 1;
		}
		return -1;
	}

	// The player (unless left out) and the picked NPCs, in the order they
	// take the roles, as OStim's ActorUtil::sort: those with a penis first
	// (otherwise in pick order), then the player moved to the role they
	// chose, or for two, the one the PlayerAlways... settings say.
	std::vector<std::uint32_t> CastIDs()
	{
		std::vector<std::uint32_t> ids;
		int                        chosen;
		{
			std::scoped_lock lock(g_castLock);
			if (g_includePlayer) {
				ids.push_back(RE::PlayerCharacter::GetSingleton()->GetFormID());
			}
			ids.insert(ids.end(), g_cast.begin(), g_cast.end());
			chosen = g_playerRole;
		}
		std::ranges::stable_sort(ids, [](std::uint32_t a, std::uint32_t b) { return ActorProperties::Cached(a).penis && !ActorProperties::Cached(b).penis; });
		const auto player = RE::PlayerCharacter::GetSingleton()->GetFormID();
		const auto at = std::ranges::find(ids, player);
		if (at == ids.end()) {
			// Without the player: the NPC chosen to lead goes first.
			std::uint32_t lead;
			{
				std::scoped_lock lock(g_castLock);
				lead = g_leadActor;
			}
			if (const auto it = std::ranges::find(ids, lead); lead && it != ids.end()) {
				ids.erase(it);
				ids.insert(ids.begin(), lead);
			}
			return ids;
		}
		const int want = chosen >= 0 ? chosen : PlayerAlwaysIndex(ids);
		if (want >= 0 && static_cast<std::size_t>(want) < ids.size()) {
			ids.erase(at);
			ids.insert(ids.begin() + want, player);
		}
		return ids;
	}

	// The player's place in a_cast (CastIDs' order) if it's theirs to keep:
	// chosen, or by the PlayerAlways... settings.
	std::optional<std::size_t> PlayerPin(const std::vector<std::uint32_t>& a_cast)
	{
		const auto player = RE::PlayerCharacter::GetSingleton()->GetFormID();
		const auto at = std::ranges::find(a_cast, player);
		if (g_pickerMode != PickerMode::kStart) {
			return std::nullopt;
		}
		if (at == a_cast.end()) {
			std::uint32_t lead;
			{
				std::scoped_lock lock(g_castLock);
				lead = g_leadActor;
			}
			const auto it = std::ranges::find(a_cast, lead);
			return lead && it != a_cast.end() ? std::optional<std::size_t>(static_cast<std::size_t>(it - a_cast.begin())) : std::nullopt;
		}
		int chosen;
		{
			std::scoped_lock lock(g_castLock);
			chosen = g_playerRole;
		}
		if (chosen < 0 && PlayerAlwaysIndex(a_cast) < 0) {
			return std::nullopt;
		}
		return static_cast<std::size_t>(at - a_cast.begin());
	}

	// Whether the player picks their role in this cast (OStim's
	// SelectIndexAndSort): for three or more, playerSelectRoleThreesome;
	// for two, by whether they look the same sex (Gay) or not (Straight).
	bool PlayerChoosesRole(const std::vector<std::uint32_t>& a_cast)
	{
		// 4Stim: a scene without the player asks too, which NPC leads
		// (OStim's NPC threads just sort them).
		if (a_cast.size() < 2) {
			return false;
		}
		if (a_cast.size() >= 3) {
			return g_settings.playerSelectRoleThreesome;
		}
		// OStim's handleActorSorting: "gay" when both have a penis or
		// neither does (hasSchlong, not the body).
		return ActorProperties::Cached(a_cast[0]).penis == ActorProperties::Cached(a_cast[1]).penis ? g_settings.playerSelectRoleGay : g_settings.playerSelectRoleStraight;
	}

	// Snapshot of the running scenes for the picker. Main thread.
	std::vector<RunningScene> SnapshotRunning()
	{
		std::vector<RunningScene> out;
		for (const auto& active : g_activeScenes) {
			RunningScene r;
			r.actors = active.actors;
			r.sceneID = active.sceneID;
			r.autoMode = active.autoMode.on;
			const auto scene = SceneRegistry::Find(active.sceneID);
			r.label = scene ? scene->name : active.sceneID;
			std::string names;
			for (const auto id : active.actors) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				r.withPlayer = r.withPlayer || IsPlayer(actor);
				const char* name = actor ? actor->GetDisplayFullName() : nullptr;
				names += (names.empty() ? "" : ", ") + std::string(name && *name ? name : "?");
			}
			r.label += ": " + names;
			out.push_back(std::move(r));
		}
		return out;
	}

	// The furniture type picked for the new scene ("" = right here).
	std::string ChosenFurnitureType()
	{
		int choice;
		{
			std::scoped_lock lock(g_castLock);
			choice = g_placeChoice;
		}
		const auto found = PickerFurniture();
		return choice >= 0 && static_cast<std::size_t>(choice) < found.size() ? found[choice].type : std::string{};
	}

	// Every scene these actors (any role order) can start with, here or on
	// furniture of type a_furniture.
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForCast(const std::vector<std::uint32_t>& a_cast, const std::string& a_furniture)
	{
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> out;
		for (const auto& summary : SceneRegistry::List(a_cast.size(), SexFilter(a_cast, false))) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && SceneFitsFurniture(*scene, a_furniture)) {
				out.push_back(scene);
			}
		}
		return out;
	}

	bool HasTag(const SceneRegistry::Scene& a_scene, std::string_view a_tag)
	{
		return std::ranges::any_of(a_scene.tags, [&](const std::string& t) { return _stricmp(t.c_str(), std::string(a_tag).c_str()) == 0; });
	}

	// Eligible NPCs near the player, nearest first (main thread).
	std::vector<Candidate> FindCandidates(float a_radius)
	{
		std::vector<Candidate> out;
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return out;
		}
		const auto center = player->GetPosition();
		ActorProperties::Remember(player);
		REX::INFO("Picker: player's actor properties worked out");
		Compat::ForEachReferenceInRange(center, a_radius, [&](RE::TESObjectREFR* a_ref) {
			const auto actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
			if (!actor || actor == player || actor->GetDelete() || (actor->formFlags & 0x800) != 0 || !actor->Get3D()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			// Same rules as FourStimScene.CanUseActor; children never.
			if (actor->IsChild() || actor->IsDead(false) || actor->IsInCombat() || FindActiveScene(actor->GetFormID())) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			// As OStim: only actors with an actor type (docs/ACTORS.md:
			// people, by the default actor properties). Worked out here, on
			// the main thread, for the picker's lists.
			REX::INFO("Picker: checking {:08X}", actor->GetFormID());
			ActorProperties::Remember(actor);
			if (ActorProperties::Cached(actor->GetFormID()).type.empty()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const char* name = actor->GetDisplayFullName();
			out.push_back({ actor->GetFormID(), name && *name ? name : std::format("{:08X}", actor->GetFormID()), center.GetDistance(actor->GetPosition()), SceneRegistry::SexOf(actor) });
			return RE::BSContainer::ForEachResult::kContinue;
		});
		std::ranges::sort(out, [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
		return out;
	}

	class PickerMenu : public RE::IMenu
	{
	public:
		static RE::IMenu* Create(const RE::UIMessage&) { return new PickerMenu(); }

		PickerMenu()
		{
			using F = RE::UI_MENU_FLAGS;
			// kAdvancesUnderPauseMenu: the picker pauses the game, and a menu at
			// this depth otherwise stops advancing (no AS3 frame events, no
			// hover updates) while the game is paused.
			for (const auto f : { F::kPausesGame, F::kUsesCursor, F::kUsesMenuContext, F::kModal, F::kDisablePauseMenu,
					 F::kTopmostRenderedMenu, F::kUpdateUsesCursor, F::kAdvancesUnderPauseMenu, F::kRendersUnderPauseMenu }) {
				UpdateFlag(f, true);
			}
			depthPriority = RE::UI_DEPTH_PRIORITY::kGameMessage;
			inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kBasicMenuNav;

			const auto scaleform = RE::BSScaleformManager::GetSingleton();
			// Empty path: find the menu clip ourselves below instead of
			// assuming the game's usual "root1.Menu_mc".
			if (!scaleform || !scaleform->LoadMovieEx(*this, "Interface/FourStimPickerMenu.swf"sv, ""sv)) {
				REX::WARN("Picker: couldn't load Interface/FourStimPickerMenu.swf");
				return;
			}
			LinkMenuObject();
			// Search mode: type to filter by name, as OStim's search menu. Text
			// entry mode makes the game send typed characters (the input hook
			// takes them) and keeps typed letters from working as game keys.
			if (g_pickerMode == PickerMode::kSearch) {
				g_searchQuery.clear();
				if (const auto controls = RE::ControlMap::GetSingleton()) {
					controls->SetTextEntryMode(true);
					_textEntry = true;
				}
				g_searchTyping = true;
			}
			// Send the list right away rather than waiting for the menu to ask
			// (its RequestScenes call remains as a backup).
			SendScenes();
		}

		~PickerMenu() override
		{
			// A question left unanswered (the picker closed): -1, as the start
			// flow's "None" / "No" (OStim's message boxes can't be closed).
			std::function<void(int)> then;
			{
				std::scoped_lock lock(g_questionLock);
				if (g_question) {
					then = std::move(g_question->then);
					g_question.reset();
				}
			}
			if (then) {
				F4SE::GetTaskInterface()->AddTask([then = std::move(then)]() { then(-1); });
			}
			g_searchTyping = false;
			if (_textEntry) {
				if (const auto controls = RE::ControlMap::GetSingleton()) {
					controls->SetTextEntryMode(false);
				}
			}
		}

		// The list again, for what's been typed now (the input hook).
		void Refresh() { SendScenes(); }

		// A navigation name to the menu, as if from the game (the input hook,
		// for keys typed while searching).
		void SendUserEvent(const char* a_name)
		{
			if (menuObj.IsObject()) {
				Scaleform::GFx::Value args[2];
				args[0] = a_name;
				args[1] = true;
				menuObj.Invoke("ProcessUserEvent", nullptr, args, 2);
			}
		}

		// While typing, the input hook does the keyboard's keys (so a key
		// isn't done twice when text entry mode still maps it); the controller
		// and mouse work as usual.
		void OnButtonEvent(const RE::ButtonEvent* a_event) override
		{
			if (g_searchTyping && a_event && a_event->device.get() == RE::INPUT_DEVICE::kKeyboard) {
				return;
			}
			RE::IMenu::OnButtonEvent(a_event);
		}

		// Finds the AS3 menu clip and attaches BGSCodeObj to it. The game's
		// menus (built in Adobe's Flash tool) live at "root1.Menu_mc"; a
		// Flex-compiled movie's root may be named differently, so try the
		// likely paths and log which one resolved.
		void LinkMenuObject()
		{
			constexpr std::array paths{ "root1.Menu_mc", "root.Menu_mc", "Menu_mc", "_root.Menu_mc", "root1", "root" };
			for (const auto path : paths) {
				Scaleform::GFx::Value candidate;
				if (uiMovie->GetVariable(&candidate, path) && candidate.IsObject()) {
					menuObj = candidate;
					RegisterCodeObject(*uiMovie, menuObj);
					REX::INFO("Picker: menu clip found at \"{}\"", path);
					return;
				}
			}
			REX::WARN("Picker: couldn't find the menu clip (tried root1.Menu_mc, root.Menu_mc, Menu_mc, _root.Menu_mc, root1, root)");
		}

		// Menu navigation can arrive marked disabled: keyboard Enter always does
		// in this menu, and during a scene the player-controls layer blocks
		// menu actions. Accept them anyway, so IMenu passes their real names
		// on to the AS3 side.
		static bool IsNavAction(const RE::BSFixedString& a_action)
		{
			return a_action == "Accept"sv || a_action == "Cancel"sv || a_action == "Up"sv || a_action == "Down"sv ||
			       a_action == "Left"sv || a_action == "Right"sv;
		}

		bool CanHandleWhenDisabled(const RE::ButtonEvent* a_event) override
		{
			return a_event && IsNavAction(a_event->QRawUserEvent());
		}

		// Hold-to-scroll: menus only receive a key's press and release, so the
		// input hook tracks which navigation key is held (g_navHeld) and this
		// per-frame update sends AS3 a fresh Up/Down every REPEAT_RATE seconds
		// after an initial REPEAT_DELAY. Real-time clock: game time is frozen
		// while the picker pauses the game.
		void AdvanceMovie(float a_timeDelta, std::uint64_t a_time) override
		{
			RE::IMenu::AdvanceMovie(a_timeDelta, a_time);

			const int dir = g_navHeld;
			if (dir == 0 || !menuObj.IsObject()) {
				return;
			}
			constexpr double REPEAT_DELAY = 0.35;
			constexpr double REPEAT_RATE = 0.06;
			const double held = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_navHeldSince.load()).count();
			const int    due = held < REPEAT_DELAY ? 0 : static_cast<int>((held - REPEAT_DELAY) / REPEAT_RATE) + 1;
			const auto   press = g_navPressCount.load();
			if (press != _repeatPress) {  // a new press: start counting again
				_repeatPress = press;
				_repeatSteps = 0;
			}
			for (int i = 0; _repeatSteps < due && i < 3; ++i) {  // at most 3 per frame, so a hitch can't jump far
				++_repeatSteps;
				Scaleform::GFx::Value args[2];
				args[0] = dir < 0 ? "Up" : "Down";
				args[1] = true;
				menuObj.Invoke("ProcessUserEvent", nullptr, args, 2);
			}
		}

		void MapCodeObjectFunctions() override
		{
			MapCodeMethodToASFunction("RequestScenes", 0);
			MapCodeMethodToASFunction("PickScene", 1);
			MapCodeMethodToASFunction("CloseMenu", 2);
		}

		void Call(const Params& a_params) override
		{
			switch (reinterpret_cast<std::uintptr_t>(a_params.userData)) {
			case 0:
				SendScenes();
				break;
			case 1:
				if (a_params.argCount > 0 && a_params.args[0].IsString()) {
					const std::string id = a_params.args[0].GetString();
					if (!HandleAskPick(id) && !HandleFurniturePick(id) && !HandleStartPick(id)) {
						StartPicked(id);
					}
				}
				break;
			case 2:
				Close();
				break;
			default:
				break;
			}
		}

	private:
		bool          _textEntry = false;  // turned text entry mode on (search)
		int           _repeatSteps = 0;  // hold-to-scroll repeats sent for the current hold
		std::uint32_t _repeatPress = 0;  // which press _repeatSteps belongs to

		static void Close()
		{
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kHide);
			}
		}

		void SendScenes()
		{
			if (!uiMovie || !menuObj.IsObject()) {
				return;
			}
			SendStrings();
			const auto mode = g_pickerMode.load();
			if (mode == PickerMode::kNavigate) {
				SendNavigation();
				return;
			}
			if (mode == PickerMode::kFurniture) {
				SendFurnitureChoices();
				return;
			}
			if (mode == PickerMode::kAsk) {
				SendQuestion();
				return;
			}
			if (mode == PickerMode::kStart && g_startStep != StartStep::kBrowse) {
				SendStartStep();
				return;
			}
			const bool search = mode == PickerMode::kSearch;
			// Only what these actors can play: a running scene's in its role
			// order, a new one's (the cast picked) in any.
			const auto ids = search ? GetPlayerScene().ActorIDs() : CastIDs();
			const auto actorCount = ids.size();
			const bool solo = actorCount == 1;
			const auto filter = SexFilter(ids, search);
			// Sequences first, as "Sequence: <name>" with SEQUENCE_PREFIX on
			// the ID, then the scenes.
			auto scenes = SceneRegistry::ListSequences(actorCount, filter);
			for (auto& seq : scenes) {
				seq.id = std::string(SEQUENCE_PREFIX) + seq.id;
				seq.name = TRF("$4stim_sequence_name", "Sequence: {}", seq.name);
				seq.tags = seq.tags.empty() ? "sequence" : "sequence, " + seq.tags;
			}
			std::ranges::move(SceneRegistry::List(actorCount, filter), std::back_inserter(scenes));

			// Furniture: a running scene stays on what it's on (or off
			// furniture); a new one goes where the cast was placed.
			const auto here = search ? GetPlayerScene().furniture : ChosenFurnitureType();
			const auto found = search ? std::vector<Furniture::Found>{} : PickerFurniture();
			// What's been typed (search): names that have it, any case, as
			// OStim's SearchMenu (name contains the query).
			const auto lower = [](std::string a_text) {
				std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return a_text;
			};
			const std::string query = search ? lower(g_searchQuery) : std::string{};
			std::erase_if(scenes, [&](SceneRegistry::SceneSummary& a_scene) {
				if (!FurnitureFits(a_scene.furniture, here)) {
					return true;
				}
				if (!query.empty() && lower(a_scene.name).find(query) == std::string::npos) {
					return true;
				}
				const auto full = a_scene.id.starts_with(SEQUENCE_PREFIX) ? nullptr : SceneRegistry::Find(a_scene.id);
				// No transitions, as OStim's search menu (SearchMenu::Search):
				// one plays on to its destination, it isn't a scene to pick.
				return full && (full->IsTransition() || !SceneFitsFurniture(*full, here));
			});
			std::size_t sequencesLeft = 0;
			for (const auto& scene : scenes) {
				sequencesLeft += scene.id.starts_with(SEQUENCE_PREFIX) ? 1 : 0;
			}

			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			for (const auto& scene : scenes) {
				Scaleform::GFx::Value entry;
				uiMovie->CreateObject(&entry);
				Scaleform::GFx::Value id, name, tags;
				id = scene.id.c_str();
				name = scene.name.c_str();
				// What happens in it, else its tags; and the OStim modpack it's from.
				const auto detail = (scene.actions.empty() ? scene.tags : scene.actions) + (scene.modpack.empty() ? std::string{} : "  (" + scene.modpack + ")");
				tags = detail.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			}

			const std::string title = search ? (g_searchQuery.empty() ? TR("$4stim_search_title", "Search: type a scene's name") :
			                                                            TRF("$4stim_search_title_query", "Search: {}_", g_searchQuery)) :
			                          solo ? "Solo scenes" : std::format("Scenes for {}", actorCount);
			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = title.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
			if (search && menuObj.HasMember("SetFooter")) {
				Scaleform::GFx::Value footer(TR("$4stim_search_footer", "Type to filter   Backspace: delete   Esc: close").c_str());
				menuObj.Invoke("SetFooter", nullptr, &footer, 1);
			}
			REX::INFO("Picker: sent {} scene(s) and {} sequence(s) ({}{} actor(s)), {} furniture piece(s) near", scenes.size() - sequencesLeft, sequencesLeft, search ? "search, " : "", actorCount, found.size());
		}

		// The start steps before browsing: who, then where.
		void SendStartStep()
		{
			struct Row
			{
				std::string id, name, detail;
			};
			// "Settler (F)": many NPCs share a name, so their sex tells them apart.
			auto withSex = [](const Candidate& a_c) -> std::string {
				return a_c.name + (a_c.sex == SceneRegistry::Sex::kMale ? " (M)" : a_c.sex == SceneRegistry::Sex::kFemale ? " (F)" : "");
			};
			std::vector<Row> rows;
			std::string      title;
			const auto       cast = CastIDs();
			if (g_startStep == StartStep::kActors) {
				std::vector<Candidate> candidates;
				std::vector<std::uint32_t> picked;
				std::size_t maxCast;
				bool        withMe;
				bool        busy;
				std::size_t running;
				{
					std::scoped_lock lock(g_castLock);
					candidates = g_candidates;
					picked = g_cast;
					maxCast = g_maxCast;
					withMe = g_includePlayer;
					busy = g_playerBusy;
					running = g_running.size();
				}
				std::string who = withMe ? TR("$4stim_you", "You") : std::string{};
				for (const auto id : picked) {
					const auto it = std::ranges::find_if(candidates, [&](const Candidate& c) { return c.id == id; });
					who += (who.empty() ? "" : ", ") + (it != candidates.end() ? withSex(*it) : std::format("{:08X}", id));
				}
				const std::size_t castSize = picked.size() + (withMe ? 1 : 0);
				if (castSize == 0) {
					rows.push_back({ "@none", TR("$4stim_pick_someone", "Pick someone first"), TR("$4stim_pick_someone_detail", "a scene without you needs at least one NPC") });
				} else {
					rows.push_back({ "@go", castSize == 1 && withMe ? TR("$4stim_continue_alone", "Continue alone") : TRF("$4stim_continue_with", "Continue with {}", castSize), who });
				}
				if (busy) {
					rows.push_back({ "@none", TR("$4stim_you_not_in", "You: not in it (NPCs only)"), TR("$4stim_you_busy", "you're in a scene already") });
				} else {
					rows.push_back({ "@me", withMe ? TR("$4stim_you_in", "You: in the scene") : TR("$4stim_you_not_in", "You: not in it (NPCs only)"), TR("$4stim_choose_to_switch", "choose to switch") });
				}
				for (const auto& c : candidates) {
					if (std::ranges::find(picked, c.id) != picked.end()) {
						rows.push_back({ std::format("@drop:{:X}", c.id), "[x] " + withSex(c), TRF("$4stim_picked_away", "picked, {:.0f} m away: choose to remove", c.distance / 70.0F) });
					}
				}
				if (running > 0) {
					rows.push_back({ "@running", TRF("$4stim_running_scenes_count", "Running scenes ({})...", running), TR("$4stim_running_scenes_detail", "watch, end or auto-play a scene") });
				}
				if (castSize < maxCast) {
					for (const auto& c : candidates) {
						if (std::ranges::find(picked, c.id) == picked.end()) {
							rows.push_back({ std::format("@add:{:X}", c.id), withSex(c), TRF("$4stim_meters_away", "{:.0f} m away", c.distance / 70.0F) });
						}
					}
				}
				title = picked.empty() ? TR("$4stim_whos_in", "Who's in the scene?") : TR("$4stim_add_someone", "Add someone else?");
			} else if (g_startStep == StartStep::kRunning || g_startStep == StartStep::kScene) {
				std::vector<RunningScene> running;
				std::uint32_t             selected;
				bool                      busy;
				{
					std::scoped_lock lock(g_castLock);
					running = g_running;
					selected = g_selectedScene;
					busy = g_playerBusy;
				}
				const auto it = std::ranges::find_if(running, [&](const RunningScene& r) { return !r.actors.empty() && r.actors.front() == selected; });
				if (g_startStep == StartStep::kScene && it != running.end()) {
					if (!it->withPlayer && !busy) {  // not while you're in a scene yourself
						rows.push_back({ "@watch", TR("$4stim_watch", "Watch"), TR("$4stim_watch_detail", "follow it with the free camera and the HUD") });
					}
					rows.push_back({ "@auto", it->autoMode ? TR("$4stim_auto_mode_is_on", "Auto mode: on") : TR("$4stim_auto_mode_is_off", "Auto mode: off"), TR("$4stim_choose_to_switch", "choose to switch") });
					rows.push_back({ "@endscene", TR("$4stim_end_scene", "End scene"), "" });
					rows.push_back({ "@running", TR("$4stim_back", "Back"), TR("$4stim_all_running_scenes", "all running scenes") });
					title = it->label;
				} else {
					for (const auto& r : running) {
						rows.push_back({ std::format("@scene:{:X}", r.actors.front()), r.label,
							(r.withPlayer ? TR("$4stim_youre_in_it", "you're in it") : TR("$4stim_npcs", "NPCs")) + (r.autoMode ? TR("$4stim_comma_auto_mode", ", auto mode") : std::string{}) });
					}
					rows.push_back({ "@back", TR("$4stim_back", "Back"), TR("$4stim_start_a_scene", "start a scene") });
					title = TR("$4stim_running_scenes", "Running scenes");
				}
			} else if (g_startStep == StartStep::kRole) {
				// OStim's role selection: the player picks their place in
				// the cast's order (1 = first, usually the one giving).
				const auto player = RE::PlayerCharacter::GetSingleton()->GetFormID();
				if (std::ranges::find(cast, player) != cast.end()) {
					for (std::size_t i = 0; i < cast.size(); ++i) {
						const auto detail = i == 0 ? TR("$4stim_role_first", "first, usually the one giving") : i + 1 == cast.size() ? TR("$4stim_role_last", "last, usually the one receiving") : std::string{};
						rows.push_back({ std::format("@role:{}", i), TRF("$4stim_role_n", "Role {}", i + 1), detail });
					}
					rows.push_back({ "@role:any", TR("$4stim_any_role", "Any role"), TR("$4stim_any_role_detail", "whichever the scene gives you") });
					title = TR("$4stim_your_role", "Your role?");
				} else {
					// Without the player: which NPC takes the first role.
					std::vector<Candidate> candidates;
					{
						std::scoped_lock lock(g_castLock);
						candidates = g_candidates;
					}
					for (const auto id : cast) {
						const auto it = std::ranges::find_if(candidates, [&](const Candidate& c) { return c.id == id; });
						rows.push_back({ std::format("@lead:{:X}", id), TRF("$4stim_leads", "{} leads", it != candidates.end() ? withSex(*it) : std::format("{:08X}", id)),
							TR("$4stim_leads_detail", "takes the first role, usually the one giving") });
					}
					rows.push_back({ "@role:any", TR("$4stim_either", "Either"), TR("$4stim_either_detail", "whichever the scene gives them") });
					title = TR("$4stim_who_leads", "Who leads?");
				}
				rows.push_back({ "@back", TR("$4stim_back", "Back"), TR("$4stim_change_who", "change who's in it") });
			} else {
				// Where: right here, or a piece of furniture near, if any
				// scene fits this cast there.
				if (!ScenesForCast(cast, {}).empty()) {
					const bool withMe = std::ranges::find(cast, RE::PlayerCharacter::GetSingleton()->GetFormID()) != cast.end();
					rows.push_back({ "@here", withMe ? TR("$4stim_right_here", "Right here") : TR("$4stim_where_they_are", "Where they are"), TR("$4stim_no_furniture", "no furniture") });
				}
				const auto found = PickerFurniture();
				for (std::size_t i = 0; i < found.size(); ++i) {
					if (!ScenesForCast(cast, found[i].type).empty()) {
						rows.push_back({ std::format("@furn:{}", i), found[i].name, TRF("$4stim_meters_away", "{:.0f} m away", found[i].distance / 70.0F) });
					}
				}
				if (rows.empty()) {
					rows.push_back({ "@back", TR("$4stim_no_scenes_for_cast", "No scenes for this cast"), TR("$4stim_choose_to_go_back", "choose to go back") });
				}
				rows.push_back({ "@browse", TR("$4stim_browse_all", "Browse all scenes..."), TRF("$4stim_for_n_actors", "for {} actor(s)", cast.size()) });
				rows.push_back({ "@back", TR("$4stim_back", "Back"), TR("$4stim_change_who", "change who's in it") });
				title = TR("$4stim_where", "Where?");
			}

			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			for (const auto& row : rows) {
				Scaleform::GFx::Value entry, id, name, tags;
				uiMovie->CreateObject(&entry);
				id = row.id.c_str();
				name = row.name.c_str();
				tags = row.detail.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			}
			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = title.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
		}

		// A pick in the start steps (ids starting with '@'). True if it was
		// one; the list is resent, or the scene started.
		bool HandleStartPick(const std::string& a_id)
		{
			if (g_pickerMode != PickerMode::kStart || !a_id.starts_with("@")) {
				return false;
			}
			auto hex = [](const std::string& a_text) { return static_cast<std::uint32_t>(std::stoul(a_text, nullptr, 16)); };
			if (a_id.starts_with("@add:")) {
				std::scoped_lock lock(g_castLock);
				g_cast.push_back(hex(a_id.substr(5)));
			} else if (a_id.starts_with("@drop:")) {
				std::scoped_lock lock(g_castLock);
				std::erase(g_cast, hex(a_id.substr(6)));
			} else if (a_id == "@go") {
				{
					std::scoped_lock lock(g_castLock);
					g_playerRole = -1;
					g_leadActor = 0;
				}
				g_startStep = PlayerChoosesRole(CastIDs()) ? StartStep::kRole : StartStep::kPlace;
				if (g_startStep == StartStep::kPlace && SkipPlaceStep()) {
					return true;
				}
			} else if (a_id.starts_with("@lead:")) {
				{
					std::scoped_lock lock(g_castLock);
					g_leadActor = hex(a_id.substr(6));
				}
				REX::INFO("Picker: {} leads", a_id.substr(6));
				g_startStep = StartStep::kPlace;
				if (SkipPlaceStep()) {
					return true;
				}
			} else if (a_id.starts_with("@role:")) {
				{
					std::scoped_lock lock(g_castLock);
					g_playerRole = a_id == "@role:any" ? -1 : std::stoi(a_id.substr(6));
				}
				REX::INFO("Picker: the player takes {}", a_id == "@role:any" ? std::string("any role") : "role " + a_id.substr(6));
				g_startStep = StartStep::kPlace;
				if (SkipPlaceStep()) {
					return true;
				}
			} else if (a_id == "@none") {
				// nothing to do yet
			} else if (a_id == "@me") {
				std::scoped_lock lock(g_castLock);
				g_includePlayer = !g_includePlayer;
			} else if (a_id == "@running") {
				g_startStep = StartStep::kRunning;
			} else if (a_id.starts_with("@scene:")) {
				{
					std::scoped_lock lock(g_castLock);
					g_selectedScene = hex(a_id.substr(7));
				}
				g_startStep = StartStep::kScene;
			} else if (a_id == "@watch" || a_id == "@auto" || a_id == "@endscene") {
				std::uint32_t selected;
				{
					std::scoped_lock lock(g_castLock);
					selected = g_selectedScene;
				}
				Close();
				F4SE::GetTaskInterface()->AddTask([a_id, selected]() { RunningSceneCommand(a_id, selected); });
				return true;
			} else if (a_id == "@back") {
				g_startStep = StartStep::kActors;
			} else if (a_id == "@browse") {
				g_startStep = StartStep::kBrowse;
			} else if (a_id == "@here" || a_id.starts_with("@furn:")) {
				{
					std::scoped_lock lock(g_castLock);
					g_placeChoice = a_id == "@here" ? -1 : std::stoi(a_id.substr(6));
				}
				StartIdleScene();
				return true;
			}
			SendScenes();
			return true;
		}

		// OStim without SetSelectFurniture (or SetEnableFurniture): no "Where?",
		// the nearest piece of furniture any scene fits this cast on (OStim's
		// findFurniture: the nearest of any type with scenes), else right
		// here; and the scene starts. False (the step is shown) when asking.
		static bool SkipPlaceStep()
		{
			if (g_settings.useFurniture && g_settings.selectFurniture) {
				return false;
			}
			const auto cast = CastIDs();
			const auto found = PickerFurniture();
			int        choice = -1;
			for (std::size_t i = 0; i < found.size() && g_settings.useFurniture; ++i) {  // nearest first
				if (!ScenesForCast(cast, found[i].type).empty()) {
					choice = static_cast<int>(i);
					break;
				}
			}
			{
				std::scoped_lock lock(g_castLock);
				g_placeChoice = choice;
			}
			REX::INFO("Picker: {}", choice >= 0 ? std::format("on the nearest furniture, {} ({:08X})", found[choice].type, found[choice].ref) :
			                                       std::string("right here (no furniture, or SetEnableFurniture=0)"));
			StartIdleScene();
			return true;
		}

		// The scene a new scene starts with: PickStartScene for the cast and
		// the place; tagged "intro" / "idle" with the player in it, any
		// without (OStim's NPC threads).
		static void StartIdleScene()
		{
			const auto cast = CastIDs();
			const auto here = ChosenFurnitureType();
			bool       withPlayer;
			{
				std::scoped_lock lock(g_castLock);
				withPlayer = g_includePlayer;
			}
			std::string why;
			const auto  scene = PickStartScene(ScenesForCast(cast, here), here, withPlayer, why);
			if (!scene) {
				REX::WARN("Picker: no scene fits {} actor(s) {}", cast.size(), here.empty() ? "here" : "on " + here);
				Close();
				return;
			}
			REX::INFO("Picker: starting \"{}\" ({})", scene->id, why);
			StartPicked(scene->id);
		}

		// The translations, for the picker's own labels (its empty-list text
		// and footer). Optional in the movie.
		void SendStrings()
		{
			if (!uiMovie || !menuObj.IsObject() || !menuObj.HasMember("SetStrings")) {
				return;
			}
			Scaleform::GFx::Value table;
			uiMovie->CreateObject(&table);
			for (const auto& [key, text] : Translations::Entries()) {
				table.SetMember(key, Scaleform::GFx::Value(text.c_str()));
			}
			menuObj.Invoke("SetStrings", nullptr, &table, 1);
		}

		// kAsk: the question and its answers.
		void SendQuestion()
		{
			Question question;
			{
				std::scoped_lock lock(g_questionLock);
				if (!g_question) {
					return;  // answered: the next one, or the close, is on its way
				}
				question.title = g_question->title;
				question.answers = g_question->answers;
			}
			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			for (std::size_t i = 0; i < question.answers.size(); ++i) {
				Scaleform::GFx::Value entry, id, name, tags;
				uiMovie->CreateObject(&entry);
				const auto key = std::format("@ask:{}", i);
				id = key.c_str();
				name = question.answers[i].name.c_str();
				tags = question.answers[i].detail.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			}
			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = question.title.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
		}

		// A pick in kAsk: its answer, on the main thread; the picker closes
		// unless that asks the next question.
		static bool HandleAskPick(const std::string& a_id)
		{
			if (g_pickerMode != PickerMode::kAsk) {
				return false;
			}
			if (!a_id.starts_with("@ask:")) {
				return true;
			}
			std::function<void(int)> then;
			{
				std::scoped_lock lock(g_questionLock);
				if (!g_question) {
					return true;  // picked twice
				}
				then = std::move(g_question->then);
				g_question.reset();
			}
			const int index = std::stoi(a_id.substr(5));
			F4SE::GetTaskInterface()->AddTask([then = std::move(then), index]() {
				if (then) {
					then(index);
				}
				bool asked;
				{
					std::scoped_lock lock(g_questionLock);
					asked = g_question.has_value();
				}
				if (!asked) {
					Close();
				}
			});
			return true;
		}

		// kFurniture: as OStim's furniture selection, "None" (stay) first,
		// then the furniture near, nearest first.
		void SendFurnitureChoices()
		{
			std::vector<Furniture::Found> found;
			{
				std::scoped_lock lock(g_changeFurnitureLock);
				found = g_changeFurniture;
			}
			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			auto add = [&](const std::string& a_id, const std::string& a_name, const std::string& a_detail) {
				Scaleform::GFx::Value entry, id, name, tags;
				uiMovie->CreateObject(&entry);
				id = a_id.c_str();
				name = a_name.c_str();
				tags = a_detail.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			};
			add("@stay", TR("$4stim_none", "None"), TR("$4stim_stay_here", "stay where you are"));
			for (std::size_t i = 0; i < found.size(); ++i) {
				add(std::format("@cfurn:{}", i), found[i].name, TRF("$4stim_meters_away", "{:.0f} m away", found[i].distance / 70.0F));
			}
			Scaleform::GFx::Value args[2];
			args[0] = list;
			const auto changeTitle = TR("$4stim_change_furniture_title", "Change furniture");
			args[1] = changeTitle.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
		}

		// A pick in the kFurniture list. True if it was one.
		static bool HandleFurniturePick(const std::string& a_id)
		{
			if (g_pickerMode != PickerMode::kFurniture) {
				return false;
			}
			Close();
			if (!a_id.starts_with("@cfurn:")) {
				return true;  // "None"
			}
			std::optional<Furniture::Found> piece;
			std::uint32_t                   role0;
			{
				std::scoped_lock lock(g_changeFurnitureLock);
				role0 = g_changeFurnitureScene;
				const auto index = static_cast<std::size_t>(std::stoul(a_id.substr(7)));
				if (index < g_changeFurniture.size()) {
					piece = g_changeFurniture[index];
				}
			}
			if (piece) {
				F4SE::GetTaskInterface()->AddTask([role0, piece = *piece]() { RequestFurnitureChange(role0, piece, {}); });
			}
			return true;
		}

		// Navigation mode: the current scene's links, then "End scene".
		void SendNavigation()
		{
			const auto current = GetPlayerScene();
			const auto scene = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;

			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			auto add = [&](const std::string& a_id, const std::string& a_name, const std::string& a_detail) {
				Scaleform::GFx::Value entry, id, name, tags;
				uiMovie->CreateObject(&entry);
				id = a_id.c_str();
				name = a_name.c_str();
				tags = a_detail.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			};
			std::string title = TR("$4stim_scene", "Scene");
			if (scene) {
				// During a transition, the options of where it's going.
				// None during a sequence, as OStim's scene menu.
				const auto settled = SceneRegistry::Settled(scene);
				const bool inSequence = FourStim::FocusedInSequence();
				for (const auto& nav : inSequence ? std::vector<SceneRegistry::Navigation>{} : settled->navigations) {
					// As OStim's Navigation::fulfilledBy: every scene on the way,
					// transitions and the scene it ends at.
					const auto dest = SceneRegistry::Find(nav.to);
					if (dest && !FourStim::CanPlayChain(*dest, current.ActorIDs(), current.furniture)) {
						continue;
					}
					add(nav.to, FormatNavLabel(nav.label, current), dest ? dest->name : nav.to);
				}
				title = scene->name + "   (speed " + std::to_string(current.speed + 1) + "/" + std::to_string(scene->speeds.size()) + ")";
			}
			if (g_settings.useFurniture) {
				add(CHANGE_FURNITURE_ID, TR("$4stim_change_furniture", "Change furniture..."), TR("$4stim_change_furniture_detail", "move the scene onto furniture near"));
			}
			add(END_SCENE_ID, TR("$4stim_end_scene", "End scene"), "");

			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = title.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
			REX::INFO("Picker: sent {} navigation option(s)", scene ? SceneRegistry::Settled(scene)->navigations.size() : 0);
		}

		static void StartPicked(std::string a_sceneID)
		{
			REX::INFO("Picker: picked \"{}\"", a_sceneID);
			Close();
			std::vector<SceneRegistry::SceneActor> roles;  // what its roles ask for
			std::string                            furnitureType;  // what it's played on
			if (a_sceneID.starts_with(SEQUENCE_PREFIX)) {
				const auto sequence = SceneRegistry::FindSequence(std::string_view(a_sceneID).substr(SEQUENCE_PREFIX.size()));
				if (!sequence) {
					return;
				}
				roles = sequence->actors;
				furnitureType = sequence->furniture;
				if (g_pickerMode == PickerMode::kSearch) {
					// On the running scene.
					F4SE::GetTaskInterface()->AddTask([sequence]() { StartSequenceOnScene(GetPlayerScene().role0, sequence); });
					return;
				}
				// Start its first scene the usual way; the sequence attaches
				// when that scene starts.
				QueueSequence(sequence, CastIDs());
				a_sceneID = sequence->entries.front().scene;
			}
			if (g_pickerMode == PickerMode::kSearch) {
				NavigateSceneImpl(a_sceneID, true);
				return;
			}
			if (g_pickerMode == PickerMode::kNavigate) {
				if (a_sceneID == CHANGE_FURNITURE_ID) {
					FourStim::OpenFurnitureForFocused();
				} else if (a_sceneID == END_SCENE_ID) {
					if (g_vm) {
						RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
						MarkPlayerSceneEnding();
						Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndPlayerScene"sv, callback);
					}
				} else {
					NavigateSceneImpl(a_sceneID);
				}
				return;
			}
			// A new scene: the cast picked, on the place picked.
			const auto cast = CastIDs();
			const auto scene = SceneRegistry::Find(a_sceneID);
			if (!scene) {
				return;
			}
			if (roles.empty()) {
				roles = scene->actors;
			}
			{
				std::scoped_lock lock(g_pendingFurnitureLock);
				g_pendingFurniture.Erase(cast);
				int choice;
				{
					std::scoped_lock castLock(g_castLock);
					choice = g_placeChoice;
				}
				const auto found = PickerFurniture();
				if (choice >= 0 && static_cast<std::size_t>(choice) < found.size()) {
					const auto& piece = found[choice];
					PendingFurniture pending{ piece.ref, piece.type, cast };
					pending.offset = scene->furnitureOffset;
					pending.floorScene = scene->furniture.empty();
					pending.fixedSpot = piece.spot;
					std::ranges::sort(pending.actors);
					REX::INFO("Picker: \"{}\" goes on {:08X} ({}, {:.0f} away)", a_sceneID, piece.ref, piece.type, piece.distance);
					g_pendingFurniture.Put(pending.actors, pending);
				}
			}

			// Who takes which role: the cast's order (CastIDs: OStim's sort)
			// unless only another order fits the roles; the player keeps a
			// role they chose.
			auto order = SceneRegistry::AssignRoles(roles, ConditionsOf(cast), PlayerPin(cast));
			if (order.size() != cast.size()) {
				order.resize(cast.size());
				for (std::size_t i = 0; i < order.size(); ++i) {
					order[i] = i;
				}
			}
			std::vector<std::int32_t> ordered;
			for (const auto index : order) {
				ordered.push_back(static_cast<std::int32_t>(cast[index]));
			}
			if (g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				// Without the player, the scene goes where the first NPC picked stands.
				std::int32_t anchor = 0;
				{
					std::scoped_lock lock(g_castLock);
					if (!g_includePlayer && !g_cast.empty()) {
						anchor = static_cast<std::int32_t>(g_cast.front());
					}
				}
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "StartPickedCast"sv, callback, std::move(a_sceneID), ordered, anchor);
			}
		}
	};

	void ShowPicker(PickerMode a_mode)
	{
		g_pickerMode = a_mode;
		if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kShow);
		}
	}

	// Main thread.
	void Ask(Question a_question)
	{
		REX::INFO("Picker: asking \"{}\" ({} answer(s))", a_question.title, a_question.answers.size());
		{
			std::scoped_lock lock(g_questionLock);
			g_question = std::move(a_question);
		}
		const auto ui = RE::UI::GetSingleton();
		const auto menu = ui && ui->GetMenuOpen(PICKER_MENU) ? ui->GetMenu(PICKER_MENU) : Scaleform::Ptr<RE::IMenu>{};
		if (menu) {
			g_pickerMode = PickerMode::kAsk;
			static_cast<PickerMenu*>(menu.get())->Refresh();
		} else {
			ShowPicker(PickerMode::kAsk);
		}
	}

	// The focused scene's controls: the HUD, focused for input, or, if the
	// HUD isn't available (turned off or its movie missing), the picker in
	// navigation mode (navigation links plus "End scene").
	void OpenSceneNavigation(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			if (HUD::IsOpen()) {
				HUD::SetFocus(true);
			} else {
				ShowPicker(PickerMode::kNavigate);
			}
		});
	}

	// Opens the picker for a new scene: a_targetID already picked (0 = no
	// one), the player in it unless a_npcsOnly.
	void OpenPicker(std::int32_t a_targetID, bool a_npcsOnly)
	{
		g_pickerMode = PickerMode::kStart;
		g_startStep = StartStep::kActors;
		// Look for actors and furniture first (on the main thread: it walks
		// the loaded cells), then open the picker.
		F4SE::GetTaskInterface()->AddTask([a_targetID, a_npcsOnly]() {
			{
				REX::INFO("Picker: looking for actors (target {:08X})", static_cast<std::uint32_t>(a_targetID));
				auto candidates = FindCandidates(g_settings.actorRadius);
				REX::INFO("Picker: {} candidate(s) found", candidates.size());
				auto running = SnapshotRunning();
				std::scoped_lock lock(g_castLock);
				g_cast.clear();
				g_placeChoice = -1;
				g_playerRole = -1;
				g_leadActor = 0;
				// In a scene yourself, you can't be in another.
				g_playerBusy = FindActiveScene(RE::PlayerCharacter::GetSingleton()->GetFormID()) != nullptr;
				g_includePlayer = !a_npcsOnly && !g_playerBusy;
				g_running = std::move(running);
				// The hotkey found someone already in a scene: that scene's options.
				if (const auto active = a_targetID ? FindActiveScene(static_cast<std::uint32_t>(a_targetID)) : nullptr) {
					g_selectedScene = active->actors.front();
					g_startStep = StartStep::kScene;
				}
				const auto target = static_cast<std::uint32_t>(a_targetID);
				if (target != 0 && std::ranges::any_of(candidates, [&](const Candidate& c) { return c.id == target; })) {
					g_cast.push_back(target);  // the one the hotkey found: already picked
				}
				g_candidates = std::move(candidates);
				g_maxCast = 1;
				for (std::size_t count = 2; count <= 8; ++count) {
					if (!SceneRegistry::List(count).empty()) {
						g_maxCast = count;
					}
				}
				REX::INFO("Picker: {} actor(s) near, {} picked, scenes for up to {}", g_candidates.size(), g_cast.size(), g_maxCast);
			}
			std::vector<Furniture::Found> found;
			const auto                    player = RE::PlayerCharacter::GetSingleton();
			if (player && g_settings.useFurniture) {
				REX::INFO("Picker: looking for furniture");
				Furniture::Reload();
				found = FurnitureNear(player->GetPosition(), player);
				REX::INFO("Picker: {} furniture found", found.size());
			}
			{
				std::scoped_lock lock(g_pickerFurnitureLock);
				g_pickerFurniture = std::move(found);
			}
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kShow);
			}
		});
	}

	// Natives: the hotkey (you in the scene, unless you're in one already)
	// and the NPC scene key (a scene without you).
	void OpenScenePicker(std::monostate, std::int32_t a_targetID)
	{
		OpenPicker(a_targetID, false);
	}

	void OpenNPCScenePicker(std::monostate, std::int32_t a_targetID)
	{
		OpenPicker(a_targetID, true);
	}

	// Returns the form ID of the actor currently selected in the console, or
	// 0 if nothing (or a non-actor) is selected. Papyrus turns it back into
	// an Actor with Game.GetForm().
	//
	// Deliberately returns an int, not an Actor: returning an object from a
	// native goes through the VM's CreateObject on this build, which crashed
	// the game (null read inside CreateObject). Receiving actors as
	// parameters works fine; only returning them is affected.
	std::int32_t GetSelectedActorID(std::monostate)
	{
		const auto ref = RE::Console::GetPickRef().get();
		const auto actor = ref ? ref->As<RE::Actor>() : nullptr;
		return actor ? static_cast<std::int32_t>(actor->GetFormID()) : 0;
	}

	// Form ID of the actor in the given role of the scene the player is in,
	// or 0 if the player isn't in a scene / there's no such role.
	std::int32_t GetPlayerSceneActorID(std::monostate, std::int32_t a_role)
	{
		const auto ids = GetPlayerScene().ActorIDs();
		return a_role >= 0 && static_cast<std::size_t>(a_role) < ids.size() ? static_cast<std::int32_t>(ids[a_role]) : 0;
	}

	// How many actors the scene the player is in has (0 = none).
	std::int32_t GetPlayerSceneActorCount(std::monostate)
	{
		return static_cast<std::int32_t>(GetPlayerScene().ActorIDs().size());
	}

	void RegisterPickerMenu(RE::UI* a_ui)
	{
		a_ui->RegisterMenu(PICKER_MENU, PickerMenu::Create);
	}

	// The picker while typing a search, or nullptr.
	PickerMenu* TypingPicker()
	{
		if (!g_searchTyping) {
			return nullptr;
		}
		const auto ui = RE::UI::GetSingleton();
		const auto menu = ui ? ui->GetMenu(PICKER_MENU) : Scaleform::Ptr<RE::IMenu>{};
		return menu ? static_cast<PickerMenu*>(menu.get()) : nullptr;
	}

	// A typed character (text entry mode, the picker's search): added to
	// the search. Printable ASCII and other Unicode, as UTF-8.
	void HandleCharacter(const RE::CharacterEvent* a_event)
	{
		const auto picker = TypingPicker();
		const auto c = a_event->charCode;
		if (!picker || c < 32 || c == 127 || g_searchQuery.size() >= 64) {
			return;
		}
		if (c < 0x80) {
			g_searchQuery += static_cast<char>(c);
		} else if (c < 0x800) {
			g_searchQuery += static_cast<char>(0xC0 | (c >> 6));
			g_searchQuery += static_cast<char>(0x80 | (c & 0x3F));
		} else if (c < 0x10000) {
			g_searchQuery += static_cast<char>(0xE0 | (c >> 12));
			g_searchQuery += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			g_searchQuery += static_cast<char>(0x80 | (c & 0x3F));
		} else {
			return;
		}
		picker->Refresh();
	}

	// The keyboard's keys while typing a search (the picker ignores the
	// keyboard then): Backspace deletes, the arrows and Enter as usual.
	// Esc closes it (HandleButton's escape hatch). Keyboard codes are Windows
	// virtual-key codes.
	void HandleTypingKey(PickerMenu* a_picker, std::uint32_t a_code);

	// A key while the picker's open: to its search, if it's being typed.
	void TypeIntoPicker(std::uint32_t a_code)
	{
		if (const auto picker = TypingPicker()) {
			HandleTypingKey(picker, a_code);
		}
	}

	void HandleTypingKey(PickerMenu* a_picker, std::uint32_t a_code)
	{
		switch (a_code) {
		case 0x08:  // Backspace: the last character (a whole UTF-8 one)
			if (!g_searchQuery.empty()) {
				auto end = g_searchQuery.size() - 1;
				while (end > 0 && (static_cast<unsigned char>(g_searchQuery[end]) & 0xC0) == 0x80) {
					--end;
				}
				g_searchQuery.erase(end);
				a_picker->Refresh();
			}
			break;
		case 0x26:
			a_picker->SendUserEvent("Up");
			break;
		case 0x28:
			a_picker->SendUserEvent("Down");
			break;
		case 0x21:  // Page Up / Down
			a_picker->SendUserEvent("Left");
			break;
		case 0x22:
			a_picker->SendUserEvent("Right");
			break;
		case 0x0D:
			a_picker->SendUserEvent("Accept");
			break;
		default:
			break;
		}
	}
}
