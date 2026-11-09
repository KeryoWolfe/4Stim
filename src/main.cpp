#include "Core/Core.h"

// The plugin's entry points; everything else is in src/Core/ and
// src/Natives/, with Core/Core.h declaring what they share.

namespace Core
{
	constexpr auto STOP_WATCHING_ID = "__stopwatching__";

	struct InputHook
	{
		using func_t = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
		static inline func_t original = nullptr;

		static void Thunk(RE::BSInputEventReceiver* a_this, const RE::InputEvent* a_queueHead)
		{
			for (auto event = a_queueHead; event; event = event->next) {
				if (const auto button = event->As<RE::ButtonEvent>()) {
					HandleButton(button);
				} else if (const auto character = event->As<RE::CharacterEvent>()) {
					HandleCharacter(character);
				}
			}
			original(a_this, a_queueHead);  // always let the game process the input too
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE::MenuControls[0] };
			original = reinterpret_cast<func_t>(vtbl.write_vfunc(0, Thunk));
			REX::INFO("Input hook installed");
		}
	};

	// ---- Save data (F4SE co-save) ----
	// The scenes running when the game is saved: who, which scene, speed,
	// furniture, auto mode, excitement and what was taken off. On load they
	// start again (bResumeScenes=1), one by one once the loading screen is
	// gone; otherwise, or if someone's missing, everyone in them is let go
	// (unrestrained, interaction back, dressed again).

	constexpr std::uint32_t SAVE_UID = 'FSTM';

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kPostLoad:
			REX::INFO("kPostLoad");
			SceneEvents::SetNativeListener(API::OnSceneEvent);
			Intiface::Start();  // 4Stim's own toy plugin (connects while bIntiface=1)
			break;
		case F4SE::MessagingInterface::kPreLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			// Scene-event registrations aren't saved: scripts register again on
			// load. Scenes don't survive loading either, so the HUD goes too.
			if (a_msg->type == F4SE::MessagingInterface::kNewGame) {
				Undress::QueryPapyrusUndressing();
			}
			SceneEvents::Clear();
			ModEvents::Clear();  // as SKSE's: scripts register again after a load
			SetPlayerScene({});
			HUD::Reset();
			// Running scenes and their timers end with the old game.
			F4SE::GetTaskInterface()->AddTask([]() {
				g_activeScenes.clear();
				SetCameraFadeOff(false);
				// kGameLoaded comes once per launch (F4SE), so a save loaded
				// from inside a scene needs this here.
				ResetPlayerSceneState("another game loaded");
				{
					std::scoped_lock lock(g_blockedFurnitureLock);
					g_blockedFurniture.clear();  // the world comes from the save (version 4 says what was blocked)
				}
				{
					std::scoped_lock lock(g_pendingClutterLock);
					g_pendingClutter.Clear();
				}
				{
					std::scoped_lock lock(g_pendingFurnitureLock);
					g_pendingFurniture.Clear();
				}
				g_furnitureSpots.Clear();
				{
					std::scoped_lock lock(g_pendingResumeLock);
					g_pendingResume.Clear();
				}
				{
					std::scoped_lock lock(g_threadsLock);
					g_threads.clear();  // threads end with the old game
				}
				g_endedThreads.clear();
				Excitement::Clear();
				AnimCues::Clear();
				Expressions::Clear();
				Sounds::Clear();
				Peaks::Clear();
				SexToys::Clear();
				PlaybackSpeed::Clear();
				API::ForgetThreads();
				Lights::Clear();
				Undress::Clear();
				EquipObjects::Clear();
				{
					std::scoped_lock weaponsLock(g_sceneWeaponsLock);
					g_sceneWeapons.clear();
				}
				PenisBend::Clear();
				Erection::Clear();
				EyeLook::Clear();
				Actor3D::Clear();
				ApiForms::Clear();
				ScenePackage::Clear();
			});
			{
				std::scoped_lock lock(g_pendingSequenceLock);
				g_pendingSequence.Clear();
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
			Undress::QueryPapyrusUndressing();  // as OStim: asked at every load
			ResumeSavedScenes();
			SexToys::Reload();  // OStim: device plugins register their toys on every save load
			break;
		case F4SE::MessagingInterface::kPreSaveGame:
			// Lights are placed references: none go into the save (the scene
			// clock puts them back).
			Lights::RemoveAll();
			break;
		case F4SE::MessagingInterface::kGameLoaded:
			{
				REX::INFO("kGameLoaded");
				g_mainThread = std::this_thread::get_id();
				// The scene files and the rest the first picker and the first
				// scene read (with a big pack: hundreds of scene files, their
				// translations, every plugin's Idle editor IDs), read now on a
				// thread of their own, while the player is still at the main
				// menu or loading, instead of as a pause on the first hotkey
				// press of every launch.
				static bool preloaded = false;
				if (!preloaded) {
					preloaded = true;
					std::thread([]() {
						const auto start = std::chrono::steady_clock::now();
						SceneRegistry::Preload();
						ActorProperties::Reload();
						Expressions::Reload();
						Lights::Reload();
						// (Voice sets are left to the first scene: Sounds has no lock
						// of its own, and they're quick.)
						REX::INFO("Preload: scenes, actor properties, expressions and lights read in {} ms",
							std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
					}).detach();
				}
				InstallAnimEventHook();
				static bool menuRegistered = false;
				if (!menuRegistered) {
					if (const auto ui = RE::UI::GetSingleton()) {
						RegisterPickerMenu(ui);
						HUD::RegisterMenu();
						menuRegistered = true;
						REX::INFO("Picker menu registered");
					}
				}
				static bool translated = false;
				if (!translated) {
					Translations::Load();  // the game's language is known by now
					Translations::AddToGame();  // MCM's "$4stim_..." labels
					translated = true;
				}
				HUD::LoadConfig();
				ScenePackage::Prepare();  // forms are loaded by now
				ScenePackage::Clear();
				// A loaded save starts with no scene running.
				HUD::Reset();
				g_activeScenes.clear();
				Excitement::Clear();
				AnimCues::Clear();
				Expressions::Clear();
				Sounds::Clear();
				Peaks::Clear();
				SexToys::Clear();
				PlaybackSpeed::Clear();
				API::ForgetThreads();
				Lights::Clear();
				Undress::Clear();
				EquipObjects::Clear();
				{
					std::scoped_lock weaponsLock(g_sceneWeaponsLock);
					g_sceneWeapons.clear();
				}
				PenisBend::Clear();
				Erection::Clear();
				EyeLook::Clear();
				SetPlayerScene({});
				ResetPlayerSceneState("game loaded");
			}
			break;
		default:
			break;
		}
	}
}

using namespace Core;


// ---- Bridge.h: what the HUD and scene events use ----

namespace FourStim
{
	RE::BSScript::IVirtualMachine* GetVM()
	{
		return g_vm;
	}

	FocusedScene GetFocusedScene()
	{
		return GetPlayerScene();
	}

	int ChangeFocusedSpeed(int a_delta)
	{
		return ChangeSceneSpeedImpl(a_delta);
	}

	bool NavigateFocused(const std::string& a_sceneID)
	{
		if (a_sceneID == END_SCENE_ID) {
			EndFocusedScene();
			return true;
		}
		if (a_sceneID == STOP_WATCHING_ID) {
			F4SE::GetTaskInterface()->AddTask([]() { StopWatching(); });
			return true;
		}
		return NavigateSceneImpl(a_sceneID);
	}

	void EndFocusedScene()
	{
		if (g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			MarkPlayerSceneEnding();
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndPlayerScene"sv, callback);
		}
	}

	void OpenSearchForFocused()
	{
		if (GetPlayerScene().Active()) {
			ShowPicker(PickerMode::kSearch);
		}
	}

	bool UsesFurniture()
	{
		return g_settings.useFurniture;
	}

	void OpenFurnitureForFocused()
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto focused = GetPlayerScene();
			const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
			if (!active || !g_settings.useFurniture) {
				return;
			}
			Furniture::Reload();
			auto found = FurnitureNear(active->center, RE::TESForm::GetFormByID<RE::Actor>(active->actors.front()));
			std::erase_if(found, [&](const Furniture::Found& f) {
				return (f.ref && f.ref == active->furnitureRef) || ScenesForRunning(active->actors, f.type).empty();
			});
			if (found.empty()) {
				REX::INFO("Furniture change: nothing near that any scene fits these actors on");
				Notify(TR("$4stim_no_furniture_near", "No furniture near for this scene"));
				return;
			}
			{
				std::scoped_lock lock(g_changeFurnitureLock);
				g_changeFurnitureScene = active->actors.front();
				g_changeFurniture = std::move(found);
			}
			ShowPicker(PickerMode::kFurniture);
		});
	}

	std::string FormatLabel(std::string a_label, const FocusedScene& a_scene)
	{
		return FormatNavLabel(std::move(a_label), a_scene);
	}

	bool FocusedInSequence()
	{
		const auto focused = GetPlayerScene();
		const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
		return active && active->sequence && !active->sequence->route;
	}

	bool CanPlayChain(const SceneRegistry::Scene& a_scene, const std::vector<std::uint32_t>& a_ids, const std::string& a_furniture)
	{
		const SceneRegistry::Scene*                 scene = &a_scene;
		std::shared_ptr<const SceneRegistry::Scene> next;
		for (int hop = 0; scene && hop < 8; ++hop) {
			if (!SexesFit(*scene, a_ids) || !SceneFitsFurniture(*scene, a_furniture)) {
				return false;
			}
			if (!scene->IsTransition()) {
				break;
			}
			next = SceneRegistry::Find(scene->destination);
			scene = next.get();
		}
		return true;
	}

	bool FocusedCanPlay(const SceneRegistry::Scene& a_scene)
	{
		const auto focused = GetPlayerScene();
		return CanPlayChain(a_scene, focused.ActorIDs(), focused.furniture);
	}

	bool GetFocusedAlignment(std::size_t a_role, Alignment::Offset& a_offset, std::string& a_sceneID)
	{
		const auto focused = GetPlayerScene();
		const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
		if (!active || a_role >= active->actors.size()) {
			return false;
		}
		const auto scene = SceneRegistry::Find(active->sceneID);
		a_sceneID = scene ? SceneRegistry::RoleScene(scene)->id : active->sceneID;
		// None set: OStim's default alignment, the scene's penis bend.
		const auto found = Alignment::Find(active->alignKey, a_sceneID, a_role);
		a_offset = found.value_or(Alignment::Offset{});
		if (!found && scene) {
			const auto settled = SceneRegistry::RoleScene(scene);
			a_offset.bend = a_role < settled->actors.size() ? settled->actors[a_role].penisBend : 0.0F;
		}
		return true;
	}

	std::string GetFocusedAlignKey()
	{
		const auto focused = GetPlayerScene();
		const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
		return active ? active->alignKey : std::string{};
	}

	void SetFocusedAlignment(std::size_t a_role, const Alignment::Offset& a_offset)
	{
		Alignment::Offset old;
		std::string       sceneID;
		if (!GetFocusedAlignment(a_role, old, sceneID)) {
			return;
		}
		const auto active = FindActiveScene(GetPlayerScene().role0);
		if (!active) {
			return;
		}
		Alignment::Set(active->alignKey, sceneID, a_role, a_offset);
		REX::INFO("Alignment: \"{}\" ({}) role {}: x {:.1f} y {:.1f} z {:.1f} rot {:.1f} scale {:.2f} bend {}", sceneID, active->alignKey, a_role, a_offset.x, a_offset.y,
			a_offset.z, a_offset.rot, a_offset.scale, a_offset.bend);
		LockScene(*active);
	}

	void ResetFocusedAlignment(std::size_t a_role)
	{
		Alignment::Offset old;
		std::string       sceneID;
		if (!GetFocusedAlignment(a_role, old, sceneID)) {
			return;
		}
		if (const auto active = FindActiveScene(GetPlayerScene().role0)) {
			Alignment::Remove(active->alignKey, sceneID, a_role);
			REX::INFO("Alignment: \"{}\" ({}) role {}: back to the scene's", sceneID, active->alignKey, a_role);
			LockScene(*active);
		}
	}

	std::vector<RE::Actor*> ResolveActors(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<RE::Actor*> actors;
		actors.reserve(a_ids.size());
		for (const auto id : a_ids) {
			actors.push_back(RE::TESForm::GetFormByID<RE::Actor>(id));
		}
		return actors;
	}
}

F4SE_PLUGIN_PRELOAD(const F4SE::PreLoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	REX::INFO("4Stim preloaded");

	return true;
}

// Old-gen F4SE (0.6.x, for game version 1.10.163) asks plugins with
// F4SEPlugin_Query whether they load; newer F4SE reads F4SEPlugin_Version
// instead and never calls this. (OStim has no such thing: Skyrim's SKSE
// versions differ in other ways.)
extern "C" __declspec(dllexport) bool F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	if (a_info) {
		a_info->infoVersion = F4SE::PluginInfo::kVersion;
		a_info->name = "4Stim";
		a_info->version = FOURSTIM_VERSION_MAJOR * 10000 + FOURSTIM_VERSION_MINOR * 100 + FOURSTIM_VERSION_PATCH;  // xmake.lua's version (0.5.0: 500)
	}
	return a_f4se && !a_f4se->IsEditor();
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	REX::INFO("Game version {}, F4SE {}", a_f4se->RuntimeVersion().string(), a_f4se->F4SEVersion().string());

	// On a game version other than AE with its Address Library, 4Stim finds
	// the game's code through CommonLibF4RD's Runtime Database (and its own
	// table for 1.10.163; compat/IDDB.cpp). Every address it uses must be
	// found, or it doesn't load at all (instead of crashing later), apart
	// from the few it can do without (compat/Fallbacks.h).
	if (Compat::UsingAddressTable()) {
		for (const auto& probe : Compat::RuntimeDatabaseProbes()) {
			REX::INFO("Addresses: Runtime Database at {}", probe);
		}
		REX::INFO("Addresses: {}, {}", Compat::UsingRuntimeDatabase() ? "CommonLibF4RD's Runtime Database" : "4Stim's own table",
			Compat::AddressTablePath().string());
		if (const auto oldGen = Compat::OldGenLibraryPath(); !oldGen.empty()) {
			REX::INFO("Addresses: old-gen IDs from the game's Address Library first, {}", oldGen.string());
		}
		if (Compat::UsingRuntimeDatabase()) {
			for (const auto& probe : Compat::AddressTableProbes()) {
				REX::INFO("Addresses: 4Stim's table at {}", probe);
			}
		}
		int missing = 0;
		int doneWithout = 0;
		for (const auto& required : Compat::REQUIRED_IDS) {
			if (Compat::HasAddress(required.id)) {
				continue;
			}
			// A few have a stand-in (compat/Fallbacks.h): 4Stim loads without them.
			const auto optional = std::ranges::find_if(Compat::OPTIONAL_IDS, [&](const Compat::OptionalID& a_optional) { return a_optional.id == required.id; });
			if (optional != std::ranges::end(Compat::OPTIONAL_IDS)) {
				REX::WARN("Addresses: {} ({}) not found ({}); {}", required.name, required.id, Compat::WhyMissing(required.id), optional->without);
				++doneWithout;
				continue;
			}
			REX::ERROR("Addresses: {} ({}) not found: {}", required.name, required.id, Compat::WhyMissing(required.id));
			++missing;
		}
		if (missing > 0) {
			REX::ERROR("Addresses: {} of {} missing; 4Stim can't run on this game version and isn't loaded", missing, std::size(Compat::REQUIRED_IDS));
			if (!Compat::UsingRuntimeDatabase()) {
				REX::ERROR("Addresses: no Runtime Database found (looked for {} and Data\\F4SE\\Plugins\\f4rd-runtime.bin under the game's folder; nexusmods.com/fallout4/mods/108394): on this game version 4Stim needs it",
					Compat::RuntimeDatabasePathLooked().string());
			}
			return false;
		}
		Compat::Prime();
		if (doneWithout > 0) {
			REX::INFO("Addresses: {} of {} found, the other {} done without", std::ssize(Compat::REQUIRED_IDS) - doneWithout, std::size(Compat::REQUIRED_IDS), doneWithout);
		} else {
			REX::INFO("Addresses: all {} found", std::size(Compat::REQUIRED_IDS));
		}
	}

	AddonSettings::WritePage();  // OStim's add-on settings: the MCM's Add-Ons page, before any menu reads it
	MCM::Sync();  // whichever of 4Stim.ini and the MCM's settings changed last wins
	LoadSettings();
	InputHook::Install();
	PlaybackSpeed::Install();
	ScenePackage::Install();
	F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
	// The plugin interface, for any plugin that asks (a nullptr sender:
	// messages from all of them), from now on as OStim's (registered in
	// SKSEPlugin_Load): a plugin asking at its own kPostLoad gets it even
	// if that comes before 4Stim's.
	F4SE::GetMessagingInterface()->RegisterListener(API::OnPluginMessage, std::string_view{});
	F4SE::GetPapyrusInterface()->Register(RegisterPapyrusFunctions);
	if (const auto serialization = F4SE::GetSerializationInterface()) {
		serialization->SetUniqueID(SAVE_UID);
		serialization->SetSaveCallback(OnGameSaved);
		serialization->SetLoadCallback(OnGameLoadedData);
		serialization->SetRevertCallback(OnGameReverted);
	}

	REX::INFO("4Stim loaded");

	return true;
}

// OStim's exports for its C++ APIs: the interface map (PluginInterface.h's
// getInterfaceMap) and the OstimNG-API interfaces (FourStimNG-API.h).
extern "C" __declspec(dllexport) FourStimAPI::InterfaceMap* requestInterfaceMap()
{
	return API::Map();
}

extern "C" __declspec(dllexport) FourStimNG_API::Scene::ISceneInterface* RequestPluginAPI_Scene(FourStimNG_API::Scene::InterfaceVersion a_version,
	const char* a_pluginName, FourStimNG_API::PluginVersion a_pluginVersion)
{
	if (!a_pluginName) {
		REX::WARN("RequestPluginAPI_Scene called with no plugin name");
		return nullptr;
	}
	REX::INFO("RequestPluginAPI_Scene: interface version {} for {} {}.{}.{}", static_cast<int>(a_version) + 1, a_pluginName,
		a_pluginVersion.major, a_pluginVersion.minor, a_pluginVersion.patch);
	return a_version == FourStimNG_API::Scene::InterfaceVersion::V1 ? API::NGScenes() : nullptr;
}

extern "C" __declspec(dllexport) FourStimNG_API::Thread::IThreadInterface* RequestPluginAPI_Thread(FourStimNG_API::Thread::InterfaceVersion a_version,
	const char* a_pluginName, FourStimNG_API::PluginVersion a_pluginVersion)
{
	if (!a_pluginName) {
		REX::WARN("RequestPluginAPI_Thread called with no plugin name");
		return nullptr;
	}
	REX::INFO("RequestPluginAPI_Thread: interface version {} for {} {}.{}.{}", static_cast<int>(a_version) + 1, a_pluginName,
		a_pluginVersion.major, a_pluginVersion.minor, a_pluginVersion.patch);
	return a_version == FourStimNG_API::Thread::InterfaceVersion::V1 ? API::NGThreads() : nullptr;
}
