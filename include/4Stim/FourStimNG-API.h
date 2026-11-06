#pragma once

// 4Stim's port of OStim NG's second C++ API, "OstimNG-API" (OStim's
// skse/src/ModAPI): a Scene interface to start and stop scenes and a Thread
// interface for external scene UIs (OStim Prism and the like) and companion
// plugins. Copy this one header into your plugin; no 4Stim code or libraries
// are needed to build against it. (4Stim's other C++ interface, the
// interface map of include/4Stim/PluginInterface.h, is separate, as OStim's
// PluginInterface is.)
//
//     auto threads = FourStimNG_API::Thread::GetAPI("MyPlugin", 1, 0, 0);
//     auto scenes = FourStimNG_API::Scene::GetAPI("MyPlugin", 1, 0, 0);
//
// Call them at F4SE's kPostLoad or later. Both are nullptr when 4Stim isn't
// installed.
//
// The same functions, arguments, structs and enums as OStim's, except where
// Fallout 4 or a header that needs no CommonLib forces otherwise:
// - The namespace is FourStimNG_API (OStim: OstimNG_API), the DLL 4Stim.dll.
// - Actors and furniture are RE::Actor* / RE::TESObjectREFR* from
//   CommonLibF4 (forward-declared here); the plugin version is three
//   numbers, not CommonLibSSE's REL::Version (RequestPluginAPI_* takes a
//   PluginVersion with the same 8-byte layout, so the exports keep OStim's
//   shape).
// - Scene IDs are 4Stim's (the scene files'), lowercase as OStim's.
// - Excitement is 4Stim's 0-100; isFemale / hasSchlong are 4Stim's reading of
//   the actor (docs/ACTORS.md).
// - MigrateThread starts the new thread as a new 4Stim scene on the same
//   furniture with the same auto mode, carrying the actors' excitement over;
//   undressing isn't carried over (they redress when the old one ends).
//   Without a callback it blocks the calling thread, so never call it that
//   way on the game's main thread (it returns -1 there).
// - The options menu is 4Stim's Utility tab's options (OStim's options
//   files, docs/OPTIONS.md); items have no description yet ("").
//
// Threads: the player's scene is thread 0, as OStim's player thread. Events
// (ThreadStarted, NodeChanged, ThreadEnded) are sent for the player's thread
// only, as OStim's; control input for the player's or the watched scene's.
// Callbacks are called on the game's main thread. Call everything else from
// the main thread too (F4SE's task interface), unless it says otherwise.
// Strings handed out stay valid until the next call that fills the same kind
// of buffer (copy them to keep them).

#include <cstdint>

// 4Stim itself builds this header with FOURSTIM_API_IMPLEMENTATION defined:
// no Windows.h, no GetAPI loaders.
#ifndef FOURSTIM_API_IMPLEMENTATION
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <Windows.h>
#endif

namespace RE
{
	class Actor;
	class TESObjectREFR;
}

namespace FourStimNG_API
{
	// CommonLib's REL::Version layout: major, minor, patch, build.
	struct PluginVersion
	{
		std::uint16_t major = 0;
		std::uint16_t minor = 0;
		std::uint16_t patch = 0;
		std::uint16_t build = 0;
	};
	static_assert(sizeof(PluginVersion) == 8);

	namespace Scene
	{
		enum class InterfaceVersion : std::uint8_t
		{
			V1
		};

		enum class APIResult : std::uint8_t
		{
			OK,       // done
			Invalid,  // the arguments were invalid
			Failed
		};

		class ISceneInterface
		{
		public:
			// actors: up to 256, ended by a nullptr. startingAnimation: a scene ID,
			// or nullptr / an unknown ID for one 4Stim picks. furniture may be nullptr.
			[[nodiscard]] virtual APIResult StartScene(const char* pluginName, RE::TESObjectREFR* furniture, const char* startingAnimation,
				RE::Actor* actors[256], std::uint32_t* threadID) noexcept = 0;
			[[nodiscard]] virtual APIResult StartCoupleScene(const char* pluginName, RE::TESObjectREFR* furniture, const char* startingAnimation,
				RE::Actor* dom, RE::Actor* sub, std::uint32_t* threadID) noexcept = 0;
			[[nodiscard]] virtual APIResult StartThreesomeScene(const char* pluginName, RE::TESObjectREFR* furniture, const char* startingAnimation,
				RE::Actor* firstActor, RE::Actor* secondActor, RE::Actor* thirdActor, std::uint32_t* threadID) noexcept = 0;
			[[nodiscard]] virtual APIResult StartFoursomeScene(const char* pluginName, RE::TESObjectREFR* furniture, const char* startingAnimation,
				RE::Actor* firstActor, RE::Actor* secondActor, RE::Actor* thirdActor, RE::Actor* fourthActor, std::uint32_t* threadID) noexcept = 0;
			[[nodiscard]] virtual APIResult StopScene(const char* pluginName, std::uint32_t threadID) noexcept = 0;
			[[nodiscard]] virtual APIResult SetAutoMode(const char* pluginName, std::uint32_t threadID, bool autoMode) noexcept = 0;
			[[nodiscard]] virtual APIResult TryGetAutoMode(const char* pluginName, std::uint32_t threadID, bool* autoMode) noexcept = 0;
		};

		using _RequestPluginAPI_Scene = ISceneInterface* (*)(InterfaceVersion a_interfaceVersion, const char* a_pluginName, PluginVersion a_pluginVersion);

#ifndef FOURSTIM_API_IMPLEMENTATION
		inline ISceneInterface* GetAPI(const char* pluginName, std::uint16_t major = 1, std::uint16_t minor = 0, std::uint16_t patch = 0)
		{
			const auto module = GetModuleHandleA("4Stim.dll");
			const auto request = module ? reinterpret_cast<_RequestPluginAPI_Scene>(reinterpret_cast<void*>(GetProcAddress(module, "RequestPluginAPI_Scene"))) : nullptr;
			return request ? request(InterfaceVersion::V1, pluginName, { major, minor, patch, 0 }) : nullptr;
		}
#endif
	}

	namespace Thread
	{
		using f32 = float;
		static_assert(sizeof(f32) == 4, "FourStimNG API: f32 must be 32-bit IEEE 754");

		enum class InterfaceVersion : std::uint8_t
		{
			V1
		};

		enum class APIResult : std::uint8_t
		{
			OK,
			Invalid,
			Failed
		};

		enum class ThreadEvent : std::uint8_t
		{
			ThreadStarted,
			ThreadEnded,
			NodeChanged,
			ControlInput
		};

		enum class Controls : std::uint8_t
		{
			Up,
			Down,
			Left,
			Right,
			Toggle,
			Yes,
			No,
			Menu,
			KEY_HIDE,
			AlignMenu,
			SearchMenu
		};

		// 4Stim.ini's keys (Windows virtual-key codes, 0 = none).
		struct KeyData
		{
			std::int32_t keyUp;
			std::int32_t keyDown;
			std::int32_t keyLeft;
			std::int32_t keyRight;
			std::int32_t keyYes;
			std::int32_t keyEnd;
			std::int32_t keyToggle;
			std::int32_t keySearch;
			std::int32_t keyAlignment;
			std::int32_t keySceneStart;
			std::int32_t keyNpcSceneStart;
			std::int32_t keySpeedUp;
			std::int32_t keySpeedDown;
			std::int32_t keyPullOut;
			std::int32_t keyAutoMode;
			std::int32_t keyFreeCam;
			std::int32_t keyHideUI;
		};

		typedef void (*ThreadEventCallback)(ThreadEvent eventType, std::uint32_t threadID, void* userData);
		typedef void (*ControlEventCallback)(Controls controlType, std::uint32_t threadID, void* userData);

		struct ActorData
		{
			std::uint32_t formID;
			f32           excitement;  // 0.0 to 100.0
			bool          isFemale;
			bool          hasSchlong;
			std::int32_t  timesClimaxed;  // in this scene
		};

		struct NavigationData
		{
			const char* sceneId;
			const char* destinationId;  // where it ends up, past transitions
			const char* icon;
			const char* description;
			const char* border;  // "rrggbb"
			bool        isTransition;
		};

		struct ActorAlignmentData
		{
			f32 offsetX;   // to the actor's right, game units
			f32 offsetY;   // forward
			f32 offsetZ;   // up
			f32 scale;     // times the actor's own
			f32 rotation;  // degrees
			f32 sosBend;   // penis bend steps, -9 to +9
		};

		struct SceneSearchResult
		{
			const char*   sceneId;
			const char*   name;
			std::uint32_t actorCount;
		};

		struct OptionsMenuItem
		{
			const char* id;
			const char* title;
			const char* icon;
			const char* border;
			const char* description;
		};

		class IThreadInterface
		{
		public:
			virtual std::uint32_t GetPlayerThreadID() noexcept = 0;
			virtual bool          IsThreadValid(std::uint32_t threadID) noexcept = 0;
			virtual const char*   GetCurrentSceneID(std::uint32_t threadID) noexcept = 0;
			virtual std::uint32_t GetActorCount(std::uint32_t threadID) noexcept = 0;
			virtual std::uint32_t GetActors(std::uint32_t threadID, ActorData* buffer, std::uint32_t bufferSize) noexcept = 0;
			virtual std::uint32_t GetNavigationCount(std::uint32_t threadID) noexcept = 0;
			virtual std::uint32_t GetNavigationOptions(std::uint32_t threadID, NavigationData* buffer, std::uint32_t bufferSize) noexcept = 0;
			virtual APIResult     NavigateToScene(std::uint32_t threadID, const char* sceneID) noexcept = 0;
			virtual bool          IsTransition(std::uint32_t threadID) noexcept = 0;
			virtual bool          IsInSequence(std::uint32_t threadID) noexcept = 0;
			virtual bool          IsAutoMode(std::uint32_t threadID) noexcept = 0;
			virtual bool          IsPlayerControlDisabled(std::uint32_t threadID) noexcept = 0;
			virtual void          RegisterEventCallback(ThreadEventCallback callback, void* userData) noexcept = 0;
			virtual void          UnregisterEventCallback(ThreadEventCallback callback) noexcept = 0;
			virtual void          RegisterControlCallback(ControlEventCallback callback, void* userData) noexcept = 0;
			virtual void          UnregisterControlCallback(ControlEventCallback callback) noexcept = 0;
			// True: 4Stim's scene HUD and search picker stay closed, and the
			// navigation keys only reach the control callbacks.
			virtual void SetExternalUIEnabled(bool enabled) noexcept = 0;
			virtual void GetKeyData(KeyData* outData) noexcept = 0;

			// --- Navigation ---
			virtual const char*  GetCurrentNodeName(std::uint32_t threadID) noexcept = 0;
			virtual std::int32_t GetCurrentSpeed(std::uint32_t threadID) noexcept = 0;
			virtual std::int32_t GetMaxSpeed(std::uint32_t threadID) noexcept = 0;
			virtual APIResult    SetSpeed(std::uint32_t threadID, std::int32_t speed) noexcept = 0;

			// --- Alignment ---
			virtual bool      GetActorAlignment(std::uint32_t threadID, std::uint32_t actorIndex, ActorAlignmentData* outData) noexcept = 0;
			virtual APIResult SetActorAlignment(std::uint32_t threadID, std::uint32_t actorIndex, const ActorAlignmentData* data) noexcept = 0;

			// --- Search (scenes the player's thread could play: its actor count and furniture) ---
			virtual std::uint32_t SearchScenes(const char* query, SceneSearchResult* buffer, std::uint32_t bufferSize) noexcept = 0;
			virtual bool          GetSceneInfo(const char* sceneID, SceneSearchResult* outInfo) noexcept = 0;
			virtual APIResult     NavigateToSearchResult(std::uint32_t threadID, const char* sceneID) noexcept = 0;

			// --- Options menu (the player's thread) ---
			virtual void RebuildOptionsTree() noexcept = 0;
			// Items on the page, not counting back / exit.
			virtual std::uint32_t GetOptionsItemCount() noexcept = 0;
			// As OStim's: back / exit first (id "-1"), then the page's items (ids "0", "1"...).
			virtual std::uint32_t GetOptionsItems(OptionsMenuItem* buffer, std::uint32_t bufferSize) noexcept = 0;
			// -1: back (or exit at the top); 0+: that item. False once it exited.
			virtual bool SelectOptionsItem(std::int32_t index) noexcept = 0;
			virtual bool IsOptionsAtRoot() noexcept = 0;

			// --- Actor management ---
			virtual bool         IsActorInAnyThread(std::uint32_t actorFormID) noexcept = 0;
			virtual bool         HasCompatibleNode(std::uint32_t threadID, const std::uint32_t* actorFormIDs, std::uint32_t actorCount) noexcept = 0;
			virtual std::int32_t MigrateThread(std::uint32_t threadID, const std::uint32_t* actorFormIDs, std::uint32_t actorCount,
				void (*onComplete)(std::int32_t newThreadID, void* context) = nullptr, void* context = nullptr, int startDelayMs = 500) noexcept = 0;
			virtual bool         IsUnrestrictedNavigation() noexcept = 0;
			virtual bool         IsIntendedSexOnly() noexcept = 0;
			virtual std::int32_t GetActorPosition(std::uint32_t threadID, std::uint32_t actorFormID) noexcept = 0;
		};

		using _RequestPluginAPI_Thread = IThreadInterface* (*)(InterfaceVersion a_interfaceVersion, const char* a_pluginName, PluginVersion a_pluginVersion);

#ifndef FOURSTIM_API_IMPLEMENTATION
		inline IThreadInterface* GetAPI(const char* pluginName, std::uint16_t major = 1, std::uint16_t minor = 0, std::uint16_t patch = 0)
		{
			const auto module = GetModuleHandleA("4Stim.dll");
			const auto request = module ? reinterpret_cast<_RequestPluginAPI_Thread>(reinterpret_cast<void*>(GetProcAddress(module, "RequestPluginAPI_Thread"))) : nullptr;
			return request ? request(InterfaceVersion::V1, pluginName, { major, minor, patch, 0 }) : nullptr;
		}
#endif
	}
}
