#pragma once

// What used to be main.cpp's anonymous namespace (roadmap 2b.3): every type,
// constant, setting and piece of shared state, and a declaration of every
// function, in their old order. The functions themselves are in src/Core/
// and src/Natives/ by area, as main.cpp's "----" sections had them.

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <future>
#include <set>
#include <unordered_set>

#include "Actions.h"
#include "compat/ScrapFunction.h"
#include "compat/Fallbacks.h"
#include "AnimCues.h"
#include "Excitement.h"
#include "Undress.h"
#include "ScenePackage.h"
#include "Alignment.h"
#include "Scaling.h"
#include "Expressions.h"
#include "Sounds.h"
#include "StimOverrides.h"
#include "Events.h"
#include "Peaks.h"
#include "PenisMesh.h"
#include "Lights.h"
#include "Bridge.h"
#include "HUD.h"
#include "SceneEvents.h"
#include "Scheduler.h"
#include "ModEvents.h"
#include "Actor3D.h"
#include "Furniture.h"
#include "SceneRegistry.h"
#include "ActorProperties.h"
#include "EquipObjects.h"
#include "Erection.h"
#include "PenisBend.h"
#include "Physics.h"
#include "ApiForms.h"
#include "EyeLook.h"
#include "Translations.h"
#include "AddonSettings.h"
#include "MCM.h"
#include "Options.h"
#include "SexToys.h"
#include "Intiface.h"
#include "4Stim/PluginInterface.h"
#include "4Stim/FourStimNG-API.h"
#include "compat/Addresses.h"
#include "compat/RequiredIDs.h"

namespace Core
{
	using namespace std::literals;

	inline bool g_lockScenes = true;  // bLockScenes: see "Scene lock"

	// Where actors were last placed for a scene, and when: the scene that
	// starts with them takes its spot and heading from here (the actors'
	// own positions can already be a little off). Main thread.
	struct PlacedSpot
	{
		RE::NiPoint3                          position;
		float                                 heading = 0.0F;
		std::chrono::steady_clock::time_point when;
	};
	inline std::unordered_map<std::uint32_t, PlacedSpot> g_placedSpots;

	// Where NPCs stood before their scene (OStim's positionBefore), to put
	// them back when it ends (bResetPosition, OStim's "Reset position").
	inline std::unordered_map<std::uint32_t, PlacedSpot> g_positionBefore;
	inline bool                                          g_resetPosition = true;
	inline bool                                          g_endOnDamage = true;         // bEndOnDamage (4Stim's)
	inline bool                                          g_stayWhenInterrupted = true;  // bStayWhenInterrupted (4Stim's)
	void FaceHeading(RE::Actor* a_actor, float a_heading);
	bool SheatheNow(RE::Actor* a_actor, std::string_view a_why);

	inline bool g_guardScenes = true;  // bGuardScenes: see GuardScenes

	// 4Stim's own fade to black (SetUseFades) is up: the scene camera doesn't
	// wait for that one, so the camera change happens while it's black.
	inline std::atomic<bool> g_sceneFade = false;

	// Furniture kicks by the scene guard, per actor: when the last was, and
	// how many in this scene (it gives up after a few, rather than replay
	// the scene over and over).
	struct GuardKicks
	{
		std::chrono::steady_clock::time_point last{};
		int                                   count = 0;
		std::uint32_t                         takenOutOf = 0;  // the furniture they were last taken out of
		bool                                  staleLogged = false;
	};
	inline std::unordered_map<std::uint32_t, GuardKicks> g_guardKicks;
	inline std::unordered_map<std::uint32_t, GuardKicks> g_holsterKicks;  // the scene guard's weapons put away, per actor, this scene

	constexpr float PI_F = 3.14159265358979323846f;

	// Stashed at Papyrus registration time so we can dispatch calls INTO
	// Papyrus-native engine functions (e.g. Actor.SetRestrained) later,
	// not just receive calls Papyrus makes to us.
	inline RE::BSScript::IVirtualMachine* g_vm = nullptr;

	// ---- Settings (Data\F4SE\Plugins\4Stim.ini) ----

	struct Settings
	{
		std::uint32_t hotkey = 0x26;           // Windows virtual-key code; OStim's: the Up arrow
		std::int32_t  targetMode = 0;          // 0 = Crosshair, 1 = Proximity
		float         maxDistance = 300.0f;    // crosshair: how far to look
		float         crosshairCone = 10.0f;   // crosshair: degrees either side of your facing
		float         proximityRadius = 200.0f;
		// The scene camera, as OStim's SetUseFreeCam / SetCameraSpeed /
		// SetFreeCamFOV / SetForceFirstPerson: the free camera, its speed (the
		// game's fFreeCameraTranslationSpeed:Camera, set outright as OStim
		// does), the world FOV during the scene, and first person after it.
		bool          useFreeCam = true;
		float         freeCamSpeed = 3.0F;
		float         freeCamSpeedMult = -1.0F;  // old fFreeCameraSpeed (a multiplier on the game's speed); < 0 = not set
		float         freeCamFOV = 45.0F;        // 0 = leave the FOV alone
		bool          forceFirstPerson = false;
		bool          useFades = true;           // SetUseFades: fade to black as scenes with the player start and end
		bool          useAutoFades = false;      // SetUseAutoFades: and as auto mode jumps to another scene
		bool          useIntroScenes = true;     // SetUseIntroScenes: scenes with the player start with one tagged "intro"
		float         customTimescale = 0.0F;    // SetCustomTimescale: the game's TimeScale in scenes with the player (0 = unchanged)
		bool          addActorsAtStart = true;   // addActorsAtStart: a script's player scene asks to add actors near (OStim's start flow)
		float         npcSceneDuration = 300.0F; // NPCSceneDuration (OStim: ms): scenes without the player end after this many seconds (0 = never)
		// OStim's scene keys, with OStim's defaults (as virtual-key codes):
		std::uint32_t speedUpKey = 0x6B;       // numpad +
		std::uint32_t speedDownKey = 0x6D;     // numpad -
		std::uint32_t pullOutKey = 0x61;       // numpad 1
		std::uint32_t endKey = 0x6E;           // numpad . (OStim's navigation "End")
		std::uint32_t searchKey = 0x4B;        // K
		std::uint32_t alignmentKey = 0x4C;     // L
		std::uint32_t freeCamKey = 0x6F;       // numpad /
		std::uint32_t hideUIKey = 0x6A;        // numpad *
		// OStim's navigation keys (its "OSA keys"): the HUD's Up / Down / Left /
		// Right, Yes (choose) and Toggle (the tab buttons), besides the arrow
		// keys, Enter and Esc. 0 = none.
		std::uint32_t navUpKey = 0x68;         // numpad 8
		std::uint32_t navDownKey = 0x65;       // numpad 5
		std::uint32_t navLeftKey = 0x64;       // numpad 4
		std::uint32_t navRightKey = 0x66;      // numpad 6
		std::uint32_t navYesKey = 0x67;        // numpad 7
		std::uint32_t navToggleKey = 0x69;     // numpad 9
		// Controller buttons for the scene keys ([Controller], 4Stim's: OStim
		// binds gamepad buttons and mouse buttons through the same key codes
		// as keys, which Fallout 4's virtual-key codes can't hold; 4Stim
		// has no mouse buttons). Gamepad codes as the game's (0x100 LB...);
		// 0 = none, the default.
		std::uint32_t padHotkey = 0, padNpcScene = 0, padSpeedUp = 0, padSpeedDown = 0, padAutoMode = 0, padPullOut = 0;
		std::uint32_t padEnd = 0, padSearch = 0, padAlign = 0, padFreeCam = 0, padHideUI = 0;
		bool          padClassic = false;  // sPadScheme: Modern (4Stim's) or Classic (OStim's navigation)
		float         hudFadeTime = 10.0F; // fHUDFadeTime (OStim's UI fadeTime, 10 s): the HUD fades after this long without input
		// OStim's excitement bar settings (its MCM's bars section), same defaults.
		bool          playerBar = true;       // bEnablePlayerBar (enablePlayerBar)
		bool          npcBar = true;          // bEnableNpcBar (enableNpcBar)
		bool          autoHideBars = false;   // bAutoHideBars (SetAutoHideBar): the bars fade with the HUD
		bool          matchBarColor = false;  // bMatchColorToGender (SetMatchColorToGender): colored by sex, not role
		std::string   hudTheme = "Color";     // file name in Data\Interface\4Stim\Themes\, without .json
		bool          hudEnabled = true;
		bool          hudColumns = false;     // sHUDControls=Columns: OStim's control scheme (Left / Right between the list and the tab buttons)
		bool          logAnimEvents = false;  // log scene actors' animation events (for authors)
		bool          timeAnimCues = true;    // bTimeAnimCues: read and time the animation files' annotation cues (AnimCues.h)
		bool          collisionWatch = false; // bCollisionWatch: log where scene actors' controllers and bodies are against their spots
		bool          translationLock = true; // bTranslationLock: hold scene actors with a never-ending translation (0: placed once, for finding a shake)
		bool          directNatives = true;   // bDirectNatives: game functions called directly, not through Papyrus (roadmap 2b.6)
		int           evaluatePackage = 0;    // iEvaluatePackage: 0 through Papyrus; 1 / 2 directly with a first argument of false / true (the test in roadmap 2b.6)
		bool          playerAIDriven = true;  // bPlayerAIDriven: the player is AI driven in scenes, as OStim's lock (SetAIDriven)
		bool          furnitureOutOfWay = true; // bFurnitureOutOfTheWay: scene furniture doesn't collide with the scene's actors
		bool          passThroughWalls = true;  // bPassThroughWalls: a scene actor touching a wall (no reference) collides with nothing for the scene
		float         transitionLead = 0.0F;   // seconds before a transition's length to move on (see 4Stim.ini)
		bool          matchSex = true;         // SetOnlyGayAnimsInGayScenes (OStim's intended sex only): a role's sex must be the actor's
		// Who the player is in a scene for two (OStim's): always the first
		// role (dominant) or the second, with a man and a woman (Straight)
		// or two of the same sex (Gay); or asked (playerSelectRole...), with
		// three or more for Threesome.
		bool          playerAlwaysDomStraight = false;
		bool          playerAlwaysSubStraight = false;
		bool          playerAlwaysDomGay = false;
		bool          playerAlwaysSubGay = false;
		bool          playerSelectRoleStraight = false;
		bool          playerSelectRoleGay = true;
		bool          playerSelectRoleThreesome = false;
		float         actorRadius = 1500.0F;     // how far from the player to list NPCs for a new scene
		float         furnitureRadius = 1600.0F; // how far from the player to look for furniture (OStim's SetFurnitureSearchDistance 15)
		float         furnitureHeight = 100.0F;  // and how far up or down
		bool          logFurniture = false;      // log every candidate object when looking
		// OStim's furniture settings: use furniture at all (SetEnableFurniture),
		// ask where (SetSelectFurniture; off: the nearest piece, as OStim),
		// and put back the clutter a furniture scene knocked about
		// (SetResetClutter, within SetResetClutterRadius x 100 units).
		bool          useFurniture = true;
		bool          selectFurniture = true;
		bool          resetClutter = true;
		float         resetClutterRadius = 500.0F;
		std::uint32_t npcSceneKey = 0;           // opens the picker for an NPC-only scene, even in a scene (0 = Shift + the hotkey)
		bool          resumeScenes = false;      // scenes running in a save start again when it's loaded (OStim ends them)
		bool          usePlaybackSpeed = true;   // scenes' playbackSpeed (OStim's, always on there): their actors' animations faster / slower
	};
	inline Settings g_settings;

	// ---- playbackSpeed ----
	// OStim's per-speed animation rate ("playbackSpeed", which OStim feeds to
	// its OStimSpeed graph variable). Fallout 4's animation graphs have no
	// such variable, so 4Stim scales the time step of each scene actor's
	// animation update instead (TESObjectREFR::UpdateAnimation, vfunc 0x9F,
	// on the Actor and PlayerCharacter vtables): 2 plays twice as fast.
	// bUsePlaybackSpeed=0 leaves animations at their own rate.
	struct PlaybackSpeed
	{
		using func_t = void (*)(RE::TESObjectREFR*, float);
		static inline func_t                                  originalActor = nullptr;
		static inline func_t                                  originalPlayer = nullptr;
		static inline std::mutex                              lock;
		static inline std::unordered_map<std::uint32_t, float> rates;  // only actors not at 1
		static inline std::atomic<bool>                       any = false;

		static float RateOf(RE::TESObjectREFR* a_ref)
		{
			if (!any.load(std::memory_order_relaxed) || !a_ref) {
				return 1.0F;
			}
			std::scoped_lock l(lock);
			const auto it = rates.find(a_ref->GetFormID());
			return it != rates.end() ? it->second : 1.0F;
		}

		static void ThunkActor(RE::TESObjectREFR* a_this, float a_delta) { originalActor(a_this, a_delta * RateOf(a_this)); }
		static void ThunkPlayer(RE::TESObjectREFR* a_this, float a_delta) { originalPlayer(a_this, a_delta * RateOf(a_this)); }

		static void Install()
		{
			REL::Relocation<std::uintptr_t> actorVtbl{ RE::VTABLE::Actor[0] };
			originalActor = reinterpret_cast<func_t>(actorVtbl.write_vfunc(0x9F, ThunkActor));
			REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE::PlayerCharacter[0] };
			originalPlayer = reinterpret_cast<func_t>(playerVtbl.write_vfunc(0x9F, ThunkPlayer));
			REX::INFO("playbackSpeed hook installed");
		}

		// a_ids play at a_rate (1 = their own rate).
		static void Set(const std::vector<std::uint32_t>& a_ids, float a_rate)
		{
			std::scoped_lock l(lock);
			for (const auto id : a_ids) {
				if (a_rate == 1.0F || !g_settings.usePlaybackSpeed) {
					rates.erase(id);
				} else {
					rates[id] = a_rate;
				}
			}
			any = !rates.empty();
		}

		static void Clear()
		{
			std::scoped_lock l(lock);
			rates.clear();
			any = false;
		}
	};


	// [AutoMode] in 4Stim.ini (see "Auto mode" below).
	struct AutoConfig
	{
		bool          player = false;        // scenes with the player start in auto mode (OStim's SetAIControl)
		bool          playerSolo = false;    // or only the player's solo ones (SetForceAIForMasturbation)
		bool          playerPartner = false; // or only the player's with partners (SetForceAIInConsensualScenes)
		bool          npc = true;            // scenes without the player do (OStim: always; 4Stim's switch)
		std::uint32_t key = 0x60;            // toggles it for the scene you're in / watching (0 = no key); OStim's: numpad 0
		float         sceneMin = 7.5F;       // seconds in each scene
		float         sceneMax = 15.0F;
		int           foreplayChance = 35;   // % of scenes that start with foreplay
		float         foreplayMin = 15.0F;   // excitement that ends it
		float         foreplayMax = 35.0F;
		int           pulloutChance = 75;    // % of scenes where a man pulls out before climaxing
		float         pulloutMin = 80.0F;    // at this excitement
		float         pulloutMax = 90.0F;
		int           maxSteps = 5;          // navigations auto mode walks through to reach a scene
		bool          limitToNavigation = true;  // during sex, only pick scenes reachable by navigation
		bool          standingOnFloor = true;    // off furniture prefer standing scenes, on a bed lying ones
		bool          autoSpeed = true;      // speed up as excitement rises
		float         speedIntervalMin = 2.5F;
		float         speedIntervalMax = 7.5F;
		float         speedExcitementMin = 15.0F;  // no speed-ups below this excitement...
		float         speedExcitementMax = 85.0F;  // ...always at or above this
	};
	inline AutoConfig g_autoConfig;
	void LoadSettings();


	// ---- The focused scene ----
	// The scene the player is in, or an NPC scene the player started to watch
	// with the free camera (see FourStim::FocusedScene). Lets the hotkey open
	// the HUD instead of starting a new scene, lets speed/navigation replay the
	// right idles, and is what the HUD shows. Set when the scene's animation
	// starts, cleared when it stops. Not save-persisted yet (that's the claim
	// registry's job). Still called "player scene" in names Papyrus sees.

	using PlayerScene = FourStim::FocusedScene;

	// Set by SaveStartView (the first step of a scene the player watches with
	// the free camera): the next scene to start becomes the focused scene even
	// without the player in it.
	inline std::atomic<bool> g_focusNextScene = false;
	PlayerScene GetPlayerScene();
	void SetPlayerScene(PlayerScene a_scene);
	void ClearFocusIfIn(std::uint32_t a_actorID);
	std::vector<SceneRegistry::ActorCondition> ConditionsOf(const std::vector<std::uint32_t>& a_ids);

	std::optional<std::size_t> PlayerPin(const std::vector<std::uint32_t>& a_cast);
	SceneRegistry::ListFilter SexFilter(const std::vector<std::uint32_t>& a_ids, bool a_fixedOrder);
	bool SexesFit(const SceneRegistry::Scene& a_scene, const std::vector<std::uint32_t>& a_ids);

	// ---- Furniture ----
	// A scene with "furniture" plays on a piece of furniture of that type (or
	// a subtype); one without plays anywhere but on furniture.

	bool FurnitureFits(const std::string& a_sceneFurniture, const std::string& a_here);
	bool SceneFitsFurniture(const SceneRegistry::Scene& a_scene, const std::string& a_here);
	bool FurnitureFits(const std::string& a_sceneFurniture, const std::string& a_here);

	// Furniture near the player when the picker opened (new scenes only).
	inline std::mutex                      g_pickerFurnitureLock;
	inline std::vector<Furniture::Found>   g_pickerFurniture;
	std::vector<Furniture::Found> PickerFurniture();
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForRunning(const std::vector<std::uint32_t>& a_ids, const std::string& a_furniture);
	std::shared_ptr<const SceneRegistry::Scene> PickStartScene(std::vector<std::shared_ptr<const SceneRegistry::Scene>> a_scenes, const std::string& a_here, bool a_tagged, std::string& a_why);

	// Furniture near a_center (FindNear, walls by ray casts from a_from's
	// cell), less what running scenes are on.
	std::vector<Furniture::Found> FurnitureNear(const RE::NiPoint3& a_center, RE::TESObjectREFR* a_from);

	// The furniture the next scene started for these actors goes on (set
	// when the picker starts a furniture scene).
	struct PendingFurniture
	{
		std::uint32_t              ref = 0;
		std::string                type;
		std::vector<std::uint32_t> actors;  // sorted
		std::array<float, 4>       offset{};  // the scene's own offset from the spot
		bool                       floorScene = false;  // the starting scene has no furniture of its own
		std::optional<Furniture::Spot> fixedSpot;  // a wall: no object, where it was found
	};

	// Something a scene about to start takes over when it does, one per set
	// of actors (sorted form IDs): any number of scenes can be starting at
	// once (a script starting several NPC scenes in a row, the picker, a
	// save's scenes), and each finds its own. Before 0.5.0 each of these was
	// one slot, and a second start before the first was placed took the
	// first's (parity audit, tier 2 item 11). The caller locks.
	template <class T>
	class ActorSlots
	{
	public:
		static std::vector<std::uint32_t> Key(std::vector<std::uint32_t> a_actors)
		{
			std::ranges::sort(a_actors);
			return a_actors;
		}
		void Put(const std::vector<std::uint32_t>& a_actors, T a_value) { _slots[Key(a_actors)] = std::move(a_value); }
		T*   Peek(const std::vector<std::uint32_t>& a_actors)
		{
			const auto it = _slots.find(Key(a_actors));
			return it != _slots.end() ? &it->second : nullptr;
		}
		std::optional<T> Take(const std::vector<std::uint32_t>& a_actors)
		{
			const auto it = _slots.find(Key(a_actors));
			if (it == _slots.end()) {
				return std::nullopt;
			}
			auto out = std::move(it->second);
			_slots.erase(it);
			return out;
		}
		void Erase(const std::vector<std::uint32_t>& a_actors) { _slots.erase(Key(a_actors)); }
		void Clear() { _slots.clear(); }

	private:
		std::map<std::vector<std::uint32_t>, T> _slots;
	};

	// The clutter around furniture a scene is about to go on, noted before
	// its actors are moved there (SetResetClutter); the scene takes it when
	// it starts.
	struct PendingClutter
	{
		std::vector<std::uint32_t>          actors;  // sorted
		std::vector<Furniture::ClutterItem> items;
		bool                                fromSave = false;  // noted before the save: not noted again
	};
	inline std::mutex                    g_pendingClutterLock;
	inline ActorSlots<PendingClutter>    g_pendingClutter;

	// Furniture whose activation 4Stim blocked (FourStimFurniture.Block, which
	// leaves furniture blocked by someone else alone): only these are
	// unblocked again.
	inline std::mutex                        g_blockedFurnitureLock;
	inline std::unordered_set<std::uint32_t> g_blockedFurniture;
	void BlockFurniture(std::uint32_t a_ref);
	void UnblockFurniture(std::uint32_t a_ref);
	bool FurnitureBlocked(std::uint32_t a_ref);

	// Edge furniture (beds): the spot for scenes made for it and the spot
	// for floor scenes played on it (see Furniture::SpotFor), worked out
	// when the actors are placed, for the scene that starts with them.
	struct FurnitureSpots
	{
		std::vector<std::uint32_t> actors;  // sorted
		Furniture::Spot            furniture;
		Furniture::Spot            floor;
	};
	inline ActorSlots<FurnitureSpots>     g_furnitureSpots;  // main thread
	inline std::mutex                     g_pendingFurnitureLock;
	inline ActorSlots<PendingFurniture>   g_pendingFurniture;
	void SetPendingFurniture(PendingFurniture a_pending);

	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot);

	bool IsPlayer(const RE::Actor* a_actor);
	void NotePlayerSpotBefore(RE::Actor* a_actor);
	bool PlaceOnPendingFurniture(const std::vector<RE::Actor*>& a_actors);

	// ---- Running scenes ----
	// Every scene started through 4Stim, player or not: other mods hear
	// about starts and ends (SceneEvents.h), and transitions and sequences
	// move scenes on by themselves (below). Main thread only.

	struct ActiveScene
	{
		std::vector<std::uint32_t> actors;  // role order
		std::string                sceneID;
		int                        speed = 0;

		// Autoplay: a sequence being played (step = its current entry), and
		// the game time left before the scene moves on (< 0 = it doesn't).
		std::shared_ptr<const SceneRegistry::Sequence> sequence;
		std::size_t                                    step = 0;
		float                                          remaining = -1.0F;

		// OStim's relativeSpeed: where the speed is among the scene's speeds
		// (speed / count), set by every speed played outside a transition;
		// -1 until one is (a scene's start, which plays its defaultSpeed). A
		// scene moved to maps it onto its own speeds (MappedSpeed), as OStim's
		// Thread::ChangeNode, so a transition's destination, a navigation and
		// an auto transition all keep the pace.
		float relativeSpeed = -1.0F;

		// A scene the player picked during a transition: played when the
		// transition ends, instead of its destination.
		std::string queuedScene;

		// The furniture it's played on (0 / "" = none); for a wall, no object
		// but the spot where it was found.
		std::uint32_t                  furnitureRef = 0;
		std::string                    furnitureType;
		std::optional<Furniture::Spot> furnitureFixedSpot;
		// OStim's furnitureScaleMult (the type's multiplyScale: actors on it
		// are scaled by it) and the furniture's own scale (for scenes with
		// scaleOffsetWithFurniture, the actors' offsets).
		float furnitureScaleMult = 1.0F;
		float furnitureRefScale = 1.0F;
		// Loose items around the furniture when the scene went on it, put back
		// when it leaves (SetResetClutter).
		std::vector<Furniture::ClutterItem> clutter;

		// Game time left before it ends after a climax (< 0 = not ending).
		float endIn = -1.0F;
		// Scenes without the player: time left before they end anyway
		// (OStim's NPCSceneDuration; < 0 = no limit).
		float stopTimer = -1.0F;
		// Actors playing their climax animation, who climax at its
		// 4StimClimax annotation or when the scene leaves it (OStim's
		// awaitingClimax).
		struct AwaitingClimax
		{
			std::uint32_t actor = 0;
			std::string   sceneID;            // the climax animation's scene
			bool          annotated = false;  // its 4StimClimax annotation came
			// OStim's awaitingClimaxInner: due, but their moan is still
			// playing; the climax waits for it to end.
			bool          afterSound = false;
		};
		std::vector<AwaitingClimax> awaitingClimax;

		// When its idles were last played (the scene guard leaves it alone
		// for a moment after).
		std::chrono::steady_clock::time_point lastPlayed = std::chrono::steady_clock::now();
		bool                                  checkedAfterPlay = false;  // the scene guard's check a second after each play (GuardScenes)
		bool                                  stallClimax = false;       // OStim's Thread::stallClimax (OThread.StallClimax), apart from the actors' own
		// The scene's spot: where its animations are played around (every
		// actor of a paired animation shares it), and the heading they face.
		// Each actor is locked there for the whole scene (see "Scene lock").
		RE::NiPoint3                          center{};
		float                                 heading = 0.0F;  // radians
		std::chrono::steady_clock::time_point lastRelock{};
		std::vector<float>                    baseScale;     // per role: the actor's own scale when the scene started
		std::vector<float>                    appliedScale;  // per role: the scale the alignment last set (0 = none)
		std::string                           alignKey;      // OStim's actor set key for alignment (Alignment::KeyFor), for the scene playing
		std::vector<float>                    heels;         // per role: how high their heels lift them (Scaling::HeelHeight), read on each scene change
		std::vector<bool>                     heelsOff;      // per role: the High Heels System's height turned off (feet not on the ground, or shoes taken off)
		std::vector<bool>                     hadHeels;      // per role: wore heels at some point in this scene
		// On edge furniture (beds): the spots for its own scenes and for floor
		// scenes; SpotOf picks by the scene playing (center / heading are the
		// first scene's).
		std::optional<FurnitureSpots>         furnitureSpots;

		// Auto mode (see "Auto mode" below).
		struct Auto
		{
			enum class Stage
			{
				kNone,
				kForeplay,  // no intercourse yet, until excitement passes foreplayUntil
				kMain,      // intercourse
				kPullout    // pulled out before the climax, waiting for it
			};
			bool  on = false;
			Stage stage = Stage::kNone;
			float foreplayUntil = 0.0F;
			float pulloutAt = 0.0F;  // a man's excitement that makes him pull out (0 = never)
			float cooldown = 0.0F;   // seconds until it moves on
			float speedCooldown = 0.0F;
		};
		Auto autoMode;

		// OStim's threads (docs/API.md): an ID other mods use for it (0 =
		// the player's scene, as OStim's player thread; others count up from
		// 1), and what a thread builder asked for. Its metadata is in its
		// ThreadRecord (g_threads), readable from any thread.
		std::int32_t                                 threadID = 0;
		bool                                         noAutoMode = false;
		bool                                         noPlayerControl = false;
		bool                                         noUndress = false;
		bool                                         noPostDialogue = false;  // OStim's NoPostDialogue (a thread builder's)
		bool                                         endAfterSequence = false;

		// Its end has begun (EndActiveScene, or the player's end key / menu):
		// the end goes through Papyrus, which removes the scene a moment
		// later (after a fade, for the player's). Until then nothing plays
		// on it (PlayOnActiveScene): a transition or auto mode moving on in
		// that moment played its next scene in the same frame as the end's
		// IdleStop, which the game then ignored (the owner's ZaZ pillory
		// scene, 5 November: both actors kept the animation after the end).
		bool ending = false;
		// Ended because someone died, fought or was hurt (not a normal end):
		// with bStayWhenInterrupted no one is sent back where they stood.
		bool interrupted = false;
		// Each actor's health at the last look (bEndOnDamage), role order.
		std::vector<float> health;
	};
	void NoteSpeed(ActiveScene& a_active, const std::string& a_sceneID, int a_speed);
	int MappedSpeed(const ActiveScene& a_active, const SceneRegistry::Scene& a_to);
	int StartSpeed(const SceneRegistry::Scene& a_scene);
	inline std::vector<ActiveScene> g_activeScenes;
	// Each scene actor's last visibility summary and when it was checked (LogVisibility).
	inline std::unordered_map<std::uint32_t, std::pair<std::string, std::chrono::steady_clock::time_point>> g_visibilityLast;

	// A sequence asked for (StartSequence / the picker) that attaches to the
	// next scene started with these actors, if that scene is its first.
	struct PendingSequence
	{
		std::shared_ptr<const SceneRegistry::Sequence> sequence;
		std::vector<std::uint32_t>                     actors;  // any order
	};
	inline std::mutex                     g_pendingSequenceLock;
	inline ActorSlots<PendingSequence>    g_pendingSequence;

	// A scene from a loaded save, being started again (see "Save data"):
	// what to restore once it's running.
	struct PendingResume
	{
		std::vector<std::uint32_t> actors;  // sorted
		int                        speed = 0;
		bool                       autoMode = false;
		std::vector<std::pair<std::uint32_t, float>> excitement;
	};
	inline std::mutex                   g_pendingResumeLock;
	inline ActorSlots<PendingResume>    g_pendingResume;

	// ---- Threads (OStim's, docs/API.md) ----
	// A thread exists from the moment a mod asks for it (OStim's
	// OThreadBuilder.Start returns a thread that's already there), while
	// Papyrus is still placing its actors ("starting"), and while its scene
	// runs. Its metadata and options live here, behind a lock, so scripts
	// read and write them at once from their own threads (OStim's API is
	// immediate: AddMetadata then HasMetadata is true). Calls that need the
	// running scene (speed, navigation, stop...) made while it's starting
	// are kept and done once it runs. The scene itself is an ActiveScene
	// with the same threadID (main thread only).
	struct ThreadRecord
	{
		enum class Status
		{
			kStarting,  // asked for, actors being placed
			kRunning,   // its ActiveScene exists
			kEnded,     // its scene stopped; its end event not sent yet (metadata still readable)
		};
		std::int32_t                          id = 0;
		Status                                status = Status::kStarting;
		std::vector<std::uint32_t>            actors;      // sorted: matched against the scene that starts
		std::vector<std::uint32_t>            roleActors;  // the order asked for (GetActors while starting)
		std::chrono::steady_clock::time_point since = std::chrono::steady_clock::now();
		// A player thread in OStim's start flow (waiting for dialogue to end,
		// asking about furniture, actors and roles): not timed out, as OStim's
		// waits as long as it takes. Its flow's number (0 = none), so a flow
		// left over from a thread that's gone stops.
		std::uint64_t flow = 0;
		// What a thread builder asked for.
		float duration = 0.0F;  // seconds; 0 = the usual
		bool  undressAtStart = false;
		bool  noAutoMode = false;
		bool  noPlayerControl = false;
		bool  noUndress = false;
		bool  noPostDialogue = false;
		bool  endAfterSequence = false;
		// Metadata (OStim's thread metadata), lowercase keys.
		std::vector<std::string>                     metadata;
		std::unordered_map<std::string, float>       metaFloat;
		std::unordered_map<std::string, std::string> metaString;
		// Asked for while starting: done once the scene runs.
		bool                   stopWhenStarted = false;
		std::optional<bool>    stallWhenStarted;
		std::optional<bool>    autoWhenStarted;
		std::optional<int>     speedWhenStarted;
		std::string            sceneWhenStarted;
	};
	inline std::mutex                                    g_threadsLock;
	inline std::unordered_map<std::int32_t, ThreadRecord> g_threads;
	inline std::int32_t                                  g_nextThreadID = 1;  // NPC scenes; the player's is 0
	void PruneStartingLocked();
	ThreadRecord* ThreadRecordLocked(std::int32_t a_id);
	void DropStartingThreadOf(std::vector<std::uint32_t> a_actors, std::string_view a_why);

	void LockScene(ActiveScene& a_active);
	bool InterruptSequence(ActiveScene& a_active, std::string_view a_why);
	void UnlockActors(const std::vector<std::uint32_t>& a_ids);
	void RestoreScales(const ActiveScene& a_active);
	void MoveApart(const ActiveScene& a_ended);
	void ArmAutoplay(ActiveScene& a_scene);
	void EndActiveScene(ActiveScene& a_scene, std::string_view a_why);
	void MarkPlayerSceneEnding();
	void StopAgainIfJustPlayed(const std::vector<std::uint32_t>& a_ids, RE::TESIdleForm* a_idle);
	std::int32_t ThreadIDOf(std::uint32_t a_actorID);
	void ForgetActorMeta(const std::vector<std::uint32_t>& a_ids);
	void SetCameraFadeOff(bool a_off);  // the camera's actor fade, off while scenes run
	inline std::thread::id g_mainThread;  // the game's main thread (set at kGameLoaded)
	void StartAutoMode(ActiveScene& a_active);
	void StopAutoMode(ActiveScene& a_active);
	void AutoModeTick(ActiveScene& a_active, float a_seconds);
	bool IsPlayer(const RE::Actor* a_actor);
	void StartAutoplayTicks();
	void HandleTimedCue(const AnimCues::Due& a_due);  // AnimEventHook::Handle, for AnimCues' cues
	void CuePlayed(std::uint32_t a_actor, const RE::TESIdleForm* a_idle, const std::string& a_sceneID);
	void RefreshExcitement(const ActiveScene& a_scene);
	bool IsPlayer(const RE::Actor* a_actor);
	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed);

	// ---- Scene furniture out of the actors' way ----
	// Not in OStim, which has no collision handling: Fallout 4 forces it.
	// The scene lock (a TranslateTo, as OStim's lockAtPosition) moves an
	// actor's character controller, which collides with what's around it. A
	// chair scene stands its actors on the chair's own spot, inside the
	// chair's collision: the controller pushing them out and the lock pulling
	// them back in made them shake (the owner's chair-by-a-desk tests). So
	// for the scene, the scene's furniture and any other furniture right by
	// it join the actors' collision group (as IgnorePairCollision has the
	// actors share one): they no longer collide with the scene's actors, and
	// still do with everyone else. Furniture only: a static near the spot
	// can be the floor, and precombined pieces (walls, most desks) have no 3D
	// of their own. With bLockScenes.
	RE::bhkCharacterController* GetCharController(RE::Actor* a_actor);

	// ---- The physics world's lock ----
	// Havok steps the physics world on its own threads while the game runs,
	// and 4Stim's changes to it (collision filters, character controllers'
	// gravity, reading what a controller touches) run in the F4SE task
	// queue, which can overlap a step: a race on Havok's memory (pointed out
	// to the owner, 5 November; the collision watch's 1 November crash was
	// Havok's own read check failing there). The game guards its world with
	// hknpBSWorld::m_worldLock (a BSReadWriteLock): another F4SE plugin
	// takes it the same way around its Havok changes, and notes the game's
	// own picks take it too. So every 4Stim change and read of a cell's
	// physics world holds that lock for writing (Bethesda's lock lets the
	// same thread take it again, for the game's own calls inside). Never
	// with a pick (Furniture's raycasts) inside: those take it themselves.
	// The physics world's own threads hold it while they step, so this
	// waits for a step to finish (a fraction of a frame).
	class PhysicsLock
	{
	public:
		explicit PhysicsLock(const RE::TESObjectREFR* a_ref)
		{
			const auto cell = a_ref ? a_ref->GetParentCell() : nullptr;
			const auto bhk = cell ? cell->GetbhkWorld() : nullptr;
			const auto world = bhk ? bhk->m_worldNP.get() : nullptr;
			if (!world) {
				return;
			}
			// Waits for a step in progress, but not forever: if the lock
			// isn't free within 20 ms, this goes ahead without it (as
			// 4Stim did before) rather than risk freezing the game on a
			// lock it can't get, and says so once.
			const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
			do {
				if (world->m_worldLock.try_lock_write()) {
					_lock = &world->m_worldLock;
					return;
				}
				std::this_thread::yield();
			} while (std::chrono::steady_clock::now() < until);
			static std::atomic<int> warned = 0;
			if (warned++ < 3) {
				REX::WARN("Physics lock: the physics world stayed locked for 20 ms: changed without the lock");
			}
		}
		~PhysicsLock()
		{
			if (_lock) {
				_lock->unlock_write();
			}
		}
		PhysicsLock(const PhysicsLock&) = delete;
		PhysicsLock& operator=(const PhysicsLock&) = delete;

	private:
		RE::BSReadWriteLock* _lock = nullptr;
	};
	void IgnoreFurnitureCollision(std::uint32_t a_furniture, std::vector<std::uint32_t> a_actors, const std::string& a_type);
	void RestoreFurnitureCollision(std::uint32_t a_furniture);

	// ---- Collision watch ----
	// Not in OStim (see above). What a scene actor's character controller
	// is touching, checked by the scene guard: each thing is logged once a
	// scene (`Collision watch:`), and anything other than what they stand
	// on, the ground and other actors joins the scene's collision group, as
	// the furniture does. If they still touch it after that, it's set to
	// collide with nothing for the scene. All put back when the actor's
	// collision is (RestoreCollisionNow). For a chair by a wall or in a
	// corner (the owner's tests, 0.4.0 and 4 November): with the chair out
	// of the way the player still shook against the wall behind it.
	//
	// With bFurnitureOutOfTheWay (on by default). Until 0.5.0 this was off
	// (bCollisionWatch) after it crashed the owner's game: it named each
	// touched body with bhkNPCollisionObject::Getbhk, whose Havok read check
	// faulted, as the guard runs alongside the physics step. It no longer
	// calls Getbhk: the touched bodies are matched against the collision
	// objects of the references around the actor (their 3D, as the furniture
	// change reads it from the same task queue without trouble). A body no
	// reference near them owns (a precombined wall) can't join the group:
	// with bPassThroughWalls the actor collides with nothing instead.
	// bCollisionWatch now only adds the position lines.
	struct TouchedBody
	{
		RE::NiPointer<RE::bhkNPCollisionObject> object;
		std::uint32_t                           filter = 0;  // its own
		bool                                    noCollision = false;
	};
	inline std::unordered_map<std::uint32_t, std::vector<TouchedBody>>        g_touched;      // actor -> bodies changed for them (main thread)
	inline std::unordered_map<std::uint32_t, std::unordered_set<const void*>> g_touchLogged;  // actor -> bodies logged this scene

	// The collision objects of the references around an actor, looked up
	// again at most every 5 seconds (main thread).
	struct NearbyBodies
	{
		struct Entry
		{
			RE::NiPointer<RE::bhkNPCollisionObject> object;
			std::uint32_t                           ref = 0;
			std::uint32_t                           base = 0;
			std::uint8_t                            type = 0;
		};
		std::chrono::steady_clock::time_point built{};
		std::vector<Entry>                    entries;
	};
	inline std::unordered_map<std::uint32_t, NearbyBodies> g_nearbyBodies;  // actor -> what's around them

	// Each scene actor's controller filter before the scene (the shared
	// group, a wall's no collision), restored at scene end
	// (RestoreCollisionNow). Not save-persisted: character controllers are
	// rebuilt on load anyway. Main thread.
	inline std::unordered_map<std::uint32_t, std::uint32_t> g_savedFilters;
	void WatchCollisions(RE::Actor* a_actor, const std::string& a_sceneID);
	void HoldGravity(RE::Actor* a_actor);
	void RestoreGravity(RE::Actor* a_actor);
	void RestoreTouched(std::uint32_t a_actor);
	void FreeFurniture(ActiveScene& a_scene);
	std::vector<Furniture::Found> FurnitureNear(const RE::NiPoint3& a_center, RE::TESObjectREFR* a_from);
	void SetFurnitureScales(ActiveScene& a_scene, std::uint32_t a_ref, const std::string& a_type);
	ActiveScene* FindActiveScene(std::uint32_t a_actorID);

	// A thread whose scene stopped, waiting for Papyrus to release its
	// actors before other mods hear it ended (SceneEvents::SceneEnded).
	// Main thread.
	struct EndedThread
	{
		std::int32_t                threadID = 0;
		std::string                 sceneID;
		std::vector<std::uint32_t>  actors;  // role order
		std::vector<std::string>    metadata;
	};
	inline std::vector<EndedThread> g_endedThreads;
	void SendEndedFor(const std::vector<std::uint32_t>& a_actors, bool a_late);
	void TrackSceneStart(std::vector<std::uint32_t> a_actors, const std::string& a_sceneID, int a_speed);
	void TrackSceneStop(std::uint32_t a_actorID);
	bool InterruptSequence(ActiveScene& a_active, std::string_view a_why);
	void TrackSceneChange(std::uint32_t a_actorID, const std::string& a_sceneID, int a_speed);
	void ArmAutoplay(ActiveScene& a_scene);
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed);
	void AdvanceAutoplay(ActiveScene& a_active);
	void Notify(const std::string& a_text);
	std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, std::vector<std::string>>> Reachable( const ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_from, int a_steps);
	float AutoGoTo(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route);
	bool AutoPullOut(ActiveScene& a_active);
	void StartAutoMode(ActiveScene& a_active);
	void StopAutoMode(ActiveScene& a_active);
	void SpeedControlTick(ActiveScene& a_active, float a_seconds);
	void AutoModeTick(ActiveScene& a_active, float a_seconds);

	// ---- Scene guard ----
	// NPCs' AI keeps running during a scene (only their movement is held),
	// and their packages play the game's own idles (sandbox fidgets, idle
	// markers) over the scene's. Four times a second, for every NPC in a
	// scene: push back the package's next idle, and if a game idle took
	// over anyway, play the scene again for everyone in it (all roles
	// together, so they stay in sync). Not during transitions.


	// ---- Scene lock ----
	// Every actor in a scene is held on the scene's spot for the whole scene,
	// so nothing moves them: not other NPCs walking into them, furniture or
	// world collision, slopes or gravity, nor their AI. Like OStim: each
	// actor is put into a "translation" to its own spot at a huge speed and
	// a near-zero turning speed (Papyrus TranslateTo); the game then keeps a
	// translating reference exactly where the translation puts it every
	// frame, whatever pushes on it, until StopTranslation. The scene guard
	// checks four times a second and locks again if anyone got off the spot.
	// The Alignment menu's offsets will be added to each actor's spot here.
	template <class... Args>
	bool CallActorMethod(RE::Actor* a_actor, std::string_view a_scriptName, std::string_view a_funcName, Args... a_args);
	void SetActorScale(RE::Actor* a_actor, float a_scale);
	void EvaluateActorPackage(RE::Actor* a_actor, const char* a_why);

	// Where role a_role of a_active is held: the scene's spot plus that
	// role's alignment for the scene (for a transition, the scene it goes
	// to, so nothing jumps when it arrives).
	struct ActorSpot
	{
		RE::NiPoint3 position;
		float        heading = 0.0F;  // radians
		float        scale = 1.0F;    // times the actor's own scale
	};

	// Actors whose HHS height 4Stim turned off and who left their scene
	// barefoot: back on when their shoes are (HeelsChanged).
	inline std::mutex                        g_heelsWaitingLock;
	inline std::unordered_set<std::uint32_t> g_heelsWaiting;
	void RestartHeels(std::uint32_t a_id, const char* a_why);
	ActorSpot SpotOf(const ActiveScene& a_active, std::size_t a_role);
	void LockScene(ActiveScene& a_active);
	void RestoreScales(const ActiveScene& a_active);

	void RecenterFreeCamera();
	void ApplyFurnitureChange();
	void RequestFurnitureChange(std::uint32_t a_role0, Furniture::Found a_piece, std::string a_sceneID);
	void UnlockActors(const std::vector<std::uint32_t>& a_ids);
	void MoveApart(const ActiveScene& a_ended);
	RE::NiPointer<RE::TESObjectREFR> FurnitureOf(RE::Actor* a_actor);
	bool LeaveFurniture(RE::Actor* a_actor, bool a_doNothing);
	void PruneScenePackage(float a_seconds);
	void LogVisibility(RE::Actor* a_actor, const std::string& a_sceneID);
	void ShowPlayerBody(RE::Actor* a_player, const std::string& a_sceneID);
	void SetCameraFadeOff(bool a_off);
	void KeepFadedIn();
	void WatchShaking(float a_seconds);
	void GuardScenes(float a_seconds);

	// ---- Excitement and climax (Excitement.h) ----

	// Climax camera shake. The game's own shake (Game.ShakeCamera) only moves
	// the normal cameras, and scenes use the free camera, so this jitters the
	// free camera itself for a moment, then puts it back where it was. Run
	// from the scene clock (main thread).
	struct ClimaxShake
	{
		bool                                  active = false;
		std::chrono::steady_clock::time_point start;
		float                                 duration = 1.2F;
		float                                 strength = 1.0F;
		RE::NiPoint3                          moved{};     // offset applied now
		float                                 turnedX = 0.0F;
		float                                 turnedY = 0.0F;
	};
	inline ClimaxShake  g_climaxShake;
	void UpdateClimaxShake();
	inline bool                       g_slowMo = false;
	void EndSlowMotion();
	bool FireEvent(ActiveScene& a_active, const std::string& a_event, int a_actor, int a_target, int a_performer);
	void RefreshExcitement(const ActiveScene& a_scene);
	void EndActiveScene(ActiveScene& a_scene, std::string_view a_why);
	void ClimaxNow(ActiveScene& a_active, std::uint32_t a_actorID);
	void HandleClimax(ActiveScene& a_active, std::uint32_t a_actorID);
	void ResolveAwaitingClimaxes(ActiveScene& a_active);
	void StartAutoplayTicks();
	bool StartSequenceOnScene(std::uint32_t a_actorID, const std::shared_ptr<const SceneRegistry::Sequence>& a_sequence);
	void QueueSequence(std::shared_ptr<const SceneRegistry::Sequence> a_sequence, std::vector<std::uint32_t> a_actors);
	bool IsPlayer(const RE::Actor* a_actor);


	// Calls a native Papyrus-exposed member function on an Actor from
	// native code, via the VM dispatch path confirmed in
	// RE/B/BSScriptUtil.h. Fire-and-forget: we don't wait for or read a
	// return value -- the bool returned here only means "successfully
	// queued", not "the function actually ran and did something".
	//
	// a_scriptName must be the script that actually DECLARES the native
	// function (e.g. "Actor" for SetRestrained/EnableAI, but
	// "ObjectReference" for MoveTo/SetAngle, which Actor only inherits).
	// Dispatching with the wrong declaring script silently "succeeds"
	// (dispatch=true) while doing nothing -- that's what happened with
	// MoveTo/SetAngle before this was split out.
	template <class... Args>
	bool CallActorMethod(RE::Actor* a_actor, std::string_view a_scriptName, std::string_view a_funcName, Args... a_args)
	{
		if (!g_vm || !a_actor) {
			return false;
		}

		auto&      handles = g_vm->GetObjectHandlePolicy();
		const auto handle = handles.GetHandleForObject(
			RE::BSScript::GetVMTypeID<RE::Actor>(),
			static_cast<const void*>(a_actor));
		if (handle == handles.EmptyHandle()) {
			REX::WARN("CallActorMethod: could not get handle for actor {:08X}", a_actor->GetFormID());
			return false;
		}

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;  // null: fire-and-forget
		return Compat::DispatchMethodCall(g_vm,
			static_cast<std::uint64_t>(handle),
			a_scriptName,
			a_funcName,
			callback,
			a_args...);
	}
	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot);

	// ---- Spike 3: play / stop a pose (unchanged) ----

	// Single-actor scene: plays role 0 of a registered scene on a_actor.
	// A start that failed (no such scene, no AI process, or the game refused
	// every actor's animation) after Papyrus had already prepared the actors:
	// restrained, collision changed, interaction blocked, the player's
	// controls locked and the scene camera on its way, with no running scene
	// the end key could end. OStim applies nothing to its actors until its
	// thread exists, so its failed starts leave nothing behind; 4Stim's
	// Papyrus start does its work first (Fallout 4's Idle-based start needs
	// the actors placed and locked before the idles play), so it undoes it
	// here: once Papyrus's start has finished, FourStimMenu.ReleaseFailedStart
	// runs a scene's normal end for them.
	inline std::atomic<std::int64_t> g_failedStartAt = 0;  // steady clock, ms
	std::int64_t NowMs();
	void ReleaseFailedStart(std::vector<std::uint32_t> a_ids, std::string_view a_why);
	void StartScene(std::monostate, RE::Actor* a_actor, RE::Actor*, std::string a_sceneID);
	void StopScene(std::monostate, RE::Actor* a_actor);
	void SuppressInteraction(std::monostate, RE::Actor* a_actor);
	void RestoreInteraction(std::monostate, RE::Actor* a_actor);
	void LockActor(std::monostate, RE::Actor* a_actor);
	void UnlockActor(std::monostate, RE::Actor* a_actor);
	void MoveActorTo(std::monostate, RE::Actor* a_target, RE::Actor* a_anchor, float a_distance, float a_angleOffsetDeg);
	RE::bhkCharacterController* GetCharController(RE::Actor* a_actor);
	void IgnorePairCollision(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1);
	void RestoreCollision(std::monostate, RE::Actor* a_actor);
	void PlacePair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, RE::Actor* a_anchor, float a_distance, float a_headingOffsetDeg);
	void PlaySceneIdles(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, std::string a_sceneID);
	std::string QueueSequenceNative(std::monostate, std::string a_sequenceID, RE::Actor* a_actor0, RE::Actor* a_actor1);
	bool StartSequenceOnSceneNative(std::monostate, RE::Actor* a_actor, std::string a_sequenceID);
	void StopSequenceNative(std::monostate, RE::Actor* a_actor);
	std::int32_t GetSceneActorCount(std::monostate, std::string a_sceneID);
	bool SceneHasAction(std::monostate, std::string a_sceneID, std::string a_type);
	bool SceneHasActionTag(std::monostate, std::string a_sceneID, std::string a_tag);
	std::int32_t GetSceneActionCount(std::monostate, std::string a_sceneID);
	std::int32_t FindSceneAction(std::monostate, std::string a_sceneID, std::string a_type, std::int32_t a_actor, std::int32_t a_target);
	std::string GetSceneActionType(std::monostate, std::string a_sceneID, std::int32_t a_index);
	std::int32_t GetSceneActionRole(std::monostate, std::string a_sceneID, std::int32_t a_index, std::int32_t a_which);
	std::vector<std::string> GetActionTypes(std::monostate);
	bool CallEvent(std::monostate, RE::Actor* a_inScene, std::string a_event, std::int32_t a_actor, std::int32_t a_target, std::int32_t a_performer);
	std::vector<std::string> GetEvents(std::monostate);
	std::string GetActionName(std::monostate, std::string a_type);
	std::vector<std::string> GetActionTags(std::monostate, std::string a_type);
	bool ActionHasTag(std::monostate, std::string a_type, std::string a_tag);
	float GetExcitement(std::monostate, RE::Actor* a_actor);
	void SetExcitement(std::monostate, RE::Actor* a_actor, float a_value);
	void AddExcitement(std::monostate, RE::Actor* a_actor, float a_value, bool a_useMultiplier);
	std::int32_t GetTimesClimaxed(std::monostate, RE::Actor* a_actor);
	void Climax(std::monostate, RE::Actor* a_actor, bool a_ignoreStall);
	void StallClimax(std::monostate, RE::Actor* a_actor, bool a_stall);
	bool IsClimaxStalled(std::monostate, RE::Actor* a_actor, bool a_checkThread);
	float GetExcitementMultiplier(std::monostate, RE::Actor* a_actor);
	void SetExcitementMultiplier(std::monostate, RE::Actor* a_actor, float a_multiplier);
	float GetTimeUntilClimax(std::monostate, RE::Actor* a_actor);
	void NoteStripped(std::monostate, RE::Actor* a_actor, std::vector<RE::TESForm*> a_items);
	void UndressActor(std::monostate, RE::Actor* a_actor);
	void RedressActor(std::monostate, RE::Actor* a_actor);
	bool IsInScene(std::monostate, RE::Actor* a_actor);
	void SceneReleased(std::monostate, std::vector<RE::Actor*> a_actors);
	void SetAutoMode(std::monostate, RE::Actor* a_actor, bool a_on);
	bool IsAutoMode(std::monostate, RE::Actor* a_actor);
	bool AutoTransition(std::monostate, RE::Actor* a_actor, std::string a_event, std::int32_t a_role);
	bool SceneActorHasTag(std::monostate, std::string a_sceneID, std::int32_t a_role, std::string a_tag);
	std::vector<std::string> GetSceneActorTags(std::monostate, std::string a_sceneID, std::int32_t a_role);
	std::int32_t ReloadScenes(std::monostate);
	bool EquipObject(std::monostate, RE::Actor* a_actor, std::string a_type);
	void UnequipObject(std::monostate, RE::Actor* a_actor, std::string a_type);
	bool IsObjectEquipped(std::monostate, RE::Actor* a_actor, std::string a_type);
	bool SetObjectVariant(std::monostate, RE::Actor* a_actor, std::string a_type, std::string a_variant, float a_duration);
	void UnsetObjectVariant(std::monostate, RE::Actor* a_actor, std::string a_type);
	std::vector<std::string> GetEquipObjectTypes(std::monostate);
	void HeelsChanged(std::monostate, std::int32_t a_actorID);
	void EvaluatePackageOf(std::monostate, RE::Actor* a_actor);
	bool HasPenis(std::monostate, RE::Actor* a_actor);
	std::string GetActorType(std::monostate, RE::Actor* a_actor);
	void NoteFurnitureBlocked(std::monostate, std::int32_t a_refID);
	void ApplyFurnitureChangeNative(std::monostate);
	void ChangeFurnitureNative(std::monostate, RE::Actor* a_actor, RE::TESObjectREFR* a_furniture, std::string a_sceneID);
	void Mute(std::monostate, RE::Actor* a_actor);
	void Unmute(std::monostate, RE::Actor* a_actor);

	// ---- The weapon an actor held before their scene ----
	// FourStimScene.HolsterForScene notes it as it takes it off, and
	// ReleaseFromScene puts it back on, as OStim's SetRemoveWeaponsAtStart
	// (on by default: ThreadActor::removeWeapons, given back at the end).
	// Kept in the save with the scene.
	inline std::mutex                                   g_sceneWeaponsLock;
	inline std::unordered_map<std::uint32_t, std::uint32_t> g_sceneWeapons;  // actor -> weapon form
	void NoteSceneWeapon(std::uint32_t a_actor, std::uint32_t a_weapon);
	std::uint32_t SceneWeaponOf(std::uint32_t a_actor);
	bool RemoveWeaponsAtStart(std::monostate);
	bool PlayerAIDrivenInScenes(std::monostate);
	void SheatheForScene(std::monostate, RE::Actor* a_actor);
	void RememberSceneWeapon(std::monostate, RE::Actor* a_actor, RE::TESForm* a_weapon);
	std::int32_t TakeSceneWeaponID(std::monostate, RE::Actor* a_actor);
	std::vector<std::string> GetVoiceSetPairs(std::monostate);
	std::string GetVoiceSetName(std::monostate, std::int32_t a_formID);
	void SetVoiceSet(std::monostate, std::int32_t a_formID, std::string a_voice);
	std::vector<std::string> GetActions(std::monostate);
	std::pair<float, float> DefaultStim(StimOverrides::Kind a_kind, std::int32_t a_role, const std::string& a_id);
	void SetActorVoiceSet(std::monostate, RE::Actor* a_actor, std::string a_voice);
	std::string GetActorVoiceSetName(std::monostate, RE::Actor* a_actor);
	bool IsMuted(std::monostate, RE::Actor* a_actor);
	bool SetEquipObjectID(std::monostate, RE::Actor* a_actor, std::string a_type, std::string a_id);
	std::string GetEquipObjectID(std::monostate, RE::Actor* a_actor, std::string a_type);
	std::vector<std::string> GetEquipObjectPairs(std::monostate, std::int32_t a_formID, std::string a_type);
	std::string GetEquipObjectName(std::monostate, std::int32_t a_formID, std::string a_type);
	void SetFormEquipObjectID(std::monostate, std::int32_t a_formID, std::string a_type, std::string a_id);

	// ---- The MCM page (docs/MCM.md) ----

	void RunKeyAction(std::string_view a_action);
	namespace API
	{
		void NotifyNGControl(FourStimNG_API::Thread::Controls a_control);  // the thread API's control callbacks
	}
	void ReloadSettings(std::monostate);
	bool ExportSettings(std::monostate);
	bool ImportSettings(std::monostate);
	void UseIniSettings(std::monostate);
	void PressKey(std::monostate, std::string a_action);
	void OpenFurnitureMenu(std::monostate);
	std::string GetFurnitureType(std::monostate, RE::TESObjectREFR* a_furniture);
	bool IsFurnitureChildOf(std::monostate, std::string a_superType, std::string a_subType);
	bool IsEventChildOf(std::monostate, std::string a_superType, std::string a_subType);
	std::int32_t GetUndressingSlotMask(std::monostate);
	void SetUndressingSlotMask(std::monostate, std::int32_t a_mask);
	std::string LocalizeText(std::monostate, std::string a_text);
	RE::TESObjectREFR* GetSceneFurniture(std::monostate, RE::Actor* a_actor);
	std::string GetSceneFurnitureType(std::monostate, RE::Actor* a_actor);
	bool RegisterForSceneEvents(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver);
	void UnregisterForSceneEvents(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver);

	namespace Natives::FourModEvents
	{
		bool Register(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, std::string a_eventName, std::string a_callback);
		void Unregister(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver, std::string a_eventName);
		void UnregisterAll(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver);
		void Send(std::monostate, std::string a_eventName, std::string a_strArg, float a_numArg, RE::TESForm* a_sender);
	}

	namespace Natives::FourJSON
	{
		std::vector<RE::Actor*> GetActors(std::monostate, std::string a_json);
		std::string GetScene(std::monostate, std::string a_json);
		std::vector<std::string> GetMetadata(std::monostate, std::string a_json);
	}
	void MarkPlayerSceneEnding();
	void StopAgainIfJustPlayed(const std::vector<std::uint32_t>& a_ids, RE::TESIdleForm* a_idle);
	void StopPair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1);
	PlayerScene FocusFor(const std::vector<std::uint32_t>& a_ids, const std::string& a_sceneID);
	void IgnoreGroupCollision(std::monostate, std::vector<RE::Actor*> a_actors);
	void PlaceGroup(std::monostate, std::vector<RE::Actor*> a_actors);
	void PlayGroupIdles(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_sceneID);
	void StopGroup(std::monostate, std::vector<RE::Actor*> a_actors);

	constexpr std::int32_t flag(RE::UserEvents::USER_EVENT_FLAG a_flag) { return static_cast<std::int32_t>(a_flag); }
	constexpr std::int32_t flag(RE::OtherInputEvents::OTHER_EVENT_FLAG a_flag) { return static_cast<std::int32_t>(a_flag); }
	void LockPlayerControls(std::monostate);
	void UnlockPlayerControls(std::monostate);
	void SaveStartView(std::monostate);
	bool LoadingOrFading();
	bool UseFades(std::monostate);
	void SetSceneFade(std::monostate, bool a_fading);
	void BeginSceneCamera(std::monostate);
	void EndSceneCamera(std::monostate);
	void ResetPlayerSceneState(std::string_view a_why);
	void ToggleSceneFreeCamera();
	void RecenterFreeCamera();
	bool NoPlayerControl(std::uint32_t a_actorID);
	std::int32_t ChangeSceneSpeedImpl(std::int32_t a_delta);
	std::int32_t ChangeSceneSpeed(std::monostate, std::int32_t a_delta);
	bool NavigateSceneImpl(const std::string& a_destination, bool a_search = false);
	bool NavigateScene(std::monostate, std::string a_destination);
	std::string FormatNavLabel(std::string a_label, const PlayerScene& a_scene);

	// ---- Scene picker menu (AS3 front-end) ----
	// FourStimPickerMenu.swf, loaded like the game's own menus: AS3 calls
	// C++ through the BGSCodeObj the engine attaches to root1.Menu_mc
	// (RequestScenes, PickScene, CloseMenu), C++ calls AS3 by invoking
	// SetScenes on that clip, and navigation input arrives in AS3 as
	// ProcessUserEvent (forwarded by IMenu). The menu shows 2-actor scenes
	// when opened with a target, 1-actor scenes for a solo player scene.

	// Navigation key currently held (-1 up, +1 down, 0 none), when that hold
	// began, and a counter bumped on every press. Written by the input hook,
	// read by the picker's per-frame update.
	inline std::atomic<int>                                   g_navHeld = 0;
	inline std::atomic<std::chrono::steady_clock::time_point> g_navHeldSince{};
	inline std::atomic<std::uint32_t>                         g_navPressCount = 0;

	constexpr auto PICKER_MENU = "FourStimPickerMenu";
	constexpr auto END_SCENE_ID = "__end__";
	void StopWatching();

	enum class PickerMode
	{
		kStart,     // start a new scene: who, where, then an idle (see StartStep)
		kNavigate,  // the focused scene's navigation links, plus "End scene" (used when the HUD isn't available)
		kSearch,    // every scene with the focused scene's actor count; picking one moves the scene there
		kFurniture,  // furniture near the focused scene; picking one moves the scene onto it (OStim's changeFurniture)
		kAsk         // a question with its answers (OStim's message boxes: the start flow's)
	};

	// kAsk: OStim's Game::showMessageBox, in the picker. Its answer (the
	// index of the one picked, or -1 if the picker was closed) is given on the
	// main thread; it can ask the next question, which the open picker then
	// shows, else the picker closes.
	struct Question
	{
		struct Answer
		{
			std::string name;
			std::string detail;
		};
		std::string                 title;
		std::vector<Answer>         answers;
		std::function<void(int)>    then;
	};
	void                    Ask(Question a_question);

	// kFurniture: the focused scene's role 0 and the furniture near it that
	// any scene fits its actors on (found when the list opened).
	inline std::mutex                    g_changeFurnitureLock;
	inline std::uint32_t                 g_changeFurnitureScene = 0;
	inline std::vector<Furniture::Found> g_changeFurniture;
	struct Candidate
	{
		std::uint32_t      id = 0;
		std::string        name;
		float              distance = 0.0F;
		SceneRegistry::Sex sex = SceneRegistry::Sex::kAny;
	};
	int PlayerAlwaysIndex(const std::vector<std::uint32_t>& a_cast);
	std::optional<std::size_t> PlayerPin(const std::vector<std::uint32_t>& a_cast);
	bool HasTag(const SceneRegistry::Scene& a_scene, std::string_view a_tag);
	std::vector<Candidate> FindCandidates(float a_radius);
	void ShowPicker(PickerMode a_mode);
	void Ask(Question a_question);
	void OpenSceneNavigation(std::monostate);
	void OpenScenePicker(std::monostate, std::int32_t a_targetID);
	void OpenNPCScenePicker(std::monostate, std::int32_t a_targetID);
	std::int32_t GetSelectedActorID(std::monostate);
	std::int32_t GetPlayerSceneActorID(std::monostate, std::int32_t a_role);
	std::int32_t GetPlayerSceneActorCount(std::monostate);
	void RegisterPickerMenu(RE::UI* a_ui);
	void HandleCharacter(const RE::CharacterEvent* a_event);
	void TypeIntoPicker(std::uint32_t a_code);

	// A scene with the player alone, as OStim's held hotkey (a thread
	// builder with the player: OStim's start flow follows).
	void StartSoloScene();
	void HandleButton(const RE::ButtonEvent* a_event);
	void RunKeyAction(std::string_view a_action);
	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	void InstallAnimEventHook();
	void HandleTimedCue(const AnimCues::Due& a_due);
	ActiveScene* FindThread(std::int32_t a_threadID);
	std::int32_t ThreadIDOf(std::uint32_t a_actorID);

	// Runs a_do on the main thread with the thread a_threadID, if it's running then.
	template <class F>
	void WithThread(std::int32_t a_threadID, F a_do)
	{
		F4SE::GetTaskInterface()->AddTask([a_threadID, a_do = std::move(a_do)]() mutable {
			if (const auto active = FindThread(a_threadID)) {
				a_do(*active);
			}
		});
	}
	std::string LowerCopy(std::string a_text);
	std::vector<std::string> SplitCSV(const std::string& a_csv);
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForActors(const std::vector<std::uint32_t>& a_cast, const std::string& a_furniture, std::optional<std::size_t> a_pinned = std::nullopt);

	// OStim's thread builder.
	struct ThreadBuilder
	{
		std::vector<std::uint32_t>                actors;
		std::vector<std::uint32_t>                dominant;
		std::uint32_t                             furniture = 0;
		bool                                      noFurniture = false;
		float                                     duration = 0.0F;
		std::vector<SceneRegistry::SequenceEntry> starting;    // starting scenes / sequence
		std::vector<bool>                         navigateTo;  // per entry of starting: OStim's navigateTo (the way there from the one before)
		bool                                      endAfterSequence = false;
		bool                                      undress = false;
		bool                                      noAutoMode = false;
		bool                                      noPlayerControl = false;
		bool                                      noUndress = false;
		bool                                      noPostDialogue = false;
		std::vector<std::string>                  metadata;
	};
	inline std::mutex                                        g_buildersLock;
	inline std::unordered_map<std::int32_t, ThreadBuilder>   g_builders;
	std::int32_t BuilderCreate(std::vector<RE::Actor*> a_actors);

	// Runs a_do on builder a_id (if it exists).
	template <class F>
	bool WithBuilder(std::int32_t a_id, F a_do)
	{
		std::scoped_lock lock(g_buildersLock);
		const auto it = g_builders.find(a_id);
		if (it == g_builders.end()) {
			REX::WARN("ThreadBuilder: no builder {}", a_id);
			return false;
		}
		a_do(it->second);
		return true;
	}
	std::int32_t StartThread(ThreadBuilder a_builder);

	namespace Natives::FourThreadBuilder
	{
		std::int32_t Create(std::monostate, std::vector<RE::Actor*> a_actors);
		void SetDominantActors(std::monostate, std::int32_t a_id, std::vector<RE::Actor*> a_actors);
		void SetFurniture(std::monostate, std::int32_t a_id, RE::TESObjectREFR* a_ref);
		void SetDuration(std::monostate, std::int32_t a_id, float a_seconds);
		void SetStartingAnimation(std::monostate, std::int32_t a_id, std::string a_scene);
		void AddStartingAnimation(std::monostate, std::int32_t a_id, std::string a_scene, float a_duration, bool a_navigateTo);
		void SetStartingSequence(std::monostate, std::int32_t a_id, std::string a_sequence);
		void ConcatStartingSequence(std::monostate, std::int32_t a_id, std::string a_sequence, bool a_navigateTo);
		void EndAfterSequence(std::monostate, std::int32_t a_id);
		void UndressActors(std::monostate, std::int32_t a_id);
		void NoAutoMode(std::monostate, std::int32_t a_id);
		void NoPlayerControl(std::monostate, std::int32_t a_id);
		void NoUndressing(std::monostate, std::int32_t a_id);
		void NoPostDialogue(std::monostate, std::int32_t a_id);
		void NoFurniture(std::monostate, std::int32_t a_id);
		void SetMetadata(std::monostate, std::int32_t a_id, std::vector<std::string> a_metadata);
		void SetMetadataCSV(std::monostate, std::int32_t a_id, std::string a_csv);
		std::int32_t Start(std::monostate, std::int32_t a_id);
		void Cancel(std::monostate, std::int32_t a_id);
	}
	void StartSoloScene();

	namespace Natives::FourThread
	{
		std::int32_t QuickStart(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_scene, RE::TESObjectREFR* a_furniture);
		// The thread's record, under its lock, for a_do; false if there's no
		// such thread (starting, running, or ended with its event not sent).
		template <class F>
		bool WithRecord(std::int32_t a_id, F a_do)
		{
			std::scoped_lock lock(g_threadsLock);
			const auto record = ThreadRecordLocked(a_id);
			if (!record) {
				return false;
			}
			a_do(*record);
			return true;
		}
		bool IsRunning(std::monostate, std::int32_t a_id);
		// Changes that need the running scene: done on the main thread, or kept
		// for when it runs if it's still starting (a_whenStarting).
		template <class F, class G>
		void ThreadDo(std::int32_t a_id, F a_running, G a_whenStarting)
		{
			bool starting = false;
			WithRecord(a_id, [&](ThreadRecord& r) {
				if (r.status == ThreadRecord::Status::kStarting) {
					a_whenStarting(r);
					starting = true;
				}
			});
			if (!starting) {
				WithThread(a_id, std::move(a_running));
			}
		}
		void Stop(std::monostate, std::int32_t a_id);
		std::int32_t GetThreadCount(std::monostate);
		std::vector<std::int32_t> GetAllThreadIDs(std::monostate);
		std::string GetScene(std::monostate, std::int32_t a_id);
		void ThreadGoTo(std::int32_t a_id, std::string a_scene, bool a_warp);
		void NavigateTo(std::monostate, std::int32_t a_id, std::string a_scene);
		void WarpTo(std::monostate, std::int32_t a_id, std::string a_scene, bool);
		std::int32_t GetSpeed(std::monostate, std::int32_t a_id);
		void SetSpeed(std::monostate, std::int32_t a_id, std::int32_t a_speed);
		std::vector<RE::Actor*> GetActors(std::monostate, std::int32_t a_id);
		RE::Actor* GetActor(std::monostate, std::int32_t a_id, std::int32_t a_index);
		std::int32_t GetActorPosition(std::monostate, std::int32_t a_id, RE::Actor* a_actor);
		void StallClimax(std::monostate, std::int32_t a_id);
		void PermitClimax(std::monostate, std::int32_t a_id, bool a_permitActors);
		bool IsClimaxStalled(std::monostate, std::int32_t a_id);
		RE::TESObjectREFR* GetFurniture(std::monostate, std::int32_t a_id);
		std::string GetFurnitureType(std::monostate, std::int32_t a_id);
		bool IsInAutoMode(std::monostate, std::int32_t a_id);
		void StartAutoMode(std::monostate, std::int32_t a_id);
		void StopAutoMode(std::monostate, std::int32_t a_id);
		bool HasMetadata(std::monostate, std::int32_t a_id, std::string a_meta);
		void AddMetadata(std::monostate, std::int32_t a_id, std::string a_meta);
		std::vector<std::string> GetMetadata(std::monostate, std::int32_t a_id);
		bool HasMetaFloat(std::monostate, std::int32_t a_id, std::string a_key);
		float GetMetaFloat(std::monostate, std::int32_t a_id, std::string a_key);
		void SetMetaFloat(std::monostate, std::int32_t a_id, std::string a_key, float a_value);
		bool HasMetaString(std::monostate, std::int32_t a_id, std::string a_key);
		std::string GetMetaString(std::monostate, std::int32_t a_id, std::string a_key);
		void SetMetaString(std::monostate, std::int32_t a_id, std::string a_key, std::string a_value);
		bool PlaySequence(std::monostate, std::int32_t a_id, std::string a_sequence, bool, bool);
	}
	namespace Natives::FourActor
	{
		std::int32_t GetThreadID(std::monostate, RE::Actor* a_actor);
	}
	bool HasAnyOf(const std::vector<std::string>& a_have, const std::vector<std::string>& a_wanted);
	bool HasAllOf(const std::vector<std::string>& a_have, const std::vector<std::string>& a_wanted);
	std::vector<std::string> TagList(const std::string& a_csv);
	std::vector<std::vector<std::string>> TagMatrix(const std::string& a_text);
	int RoleIndex(std::size_t a_role);
	bool ActionIsType(const SceneRegistry::SceneAction& a_action, const std::string& a_type);

	namespace Natives::FourLibrary
	{
		std::string FindRandomSceneByRoleTags(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::string a_actorTags, bool a_allPerRole, bool a_forAll, std::string a_sceneTags, std::int32_t a_sceneMode);
		std::string FindRandomSceneByActionRoles(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_types, bool a_all, bool a_byActor, std::int32_t a_actor, bool a_byTarget, std::int32_t a_target);

		// OStim's superload helpers: an action matching a_type (as a type or one
		// of its tags) with its roles in the given lists (an empty list allows any).
		struct ActionRoleLists
		{
			std::vector<std::string>      types;
			std::vector<std::vector<int>> actors, targets, performers, matesAny, matesAll, participantsAny, participantsAll;

			static const std::vector<int>& At(const std::vector<std::vector<int>>& a_matrix, std::size_t a_i)
			{
				static const std::vector<int> none;
				return a_i < a_matrix.size() ? a_matrix[a_i] : none;
			}

			bool Matches(const SceneRegistry::SceneAction& a_action, std::size_t a_i) const
			{
				const int  actor = RoleIndex(a_action.actor);
				const int  target = RoleIndex(a_action.target);
				const int  performer = RoleIndex(a_action.performer);
				const auto in = [](const std::vector<int>& a_list, int a_role) { return std::ranges::find(a_list, a_role) != a_list.end(); };
				const auto anyIn = [&](const std::vector<int>& a_list, std::initializer_list<int> a_roles) {
					return std::ranges::any_of(a_roles, [&](int r) { return in(a_list, r); });
				};
				const auto allIn = [&](const std::vector<int>& a_list, std::initializer_list<int> a_roles) {
					return std::ranges::all_of(a_roles, [&](int r) { return in(a_list, r); });
				};
				const auto& act = At(actors, a_i);
				const auto& tar = At(targets, a_i);
				const auto& per = At(performers, a_i);
				const auto& mAny = At(matesAny, a_i);
				const auto& mAll = At(matesAll, a_i);
				const auto& pAny = At(participantsAny, a_i);
				const auto& pAll = At(participantsAll, a_i);
				return (ActionIsType(a_action, types[a_i]) || a_action.type->HasTag(types[a_i])) &&
				       (act.empty() || in(act, actor)) &&
				       (tar.empty() || in(tar, target)) &&
				       (per.empty() || in(per, performer)) &&
				       (mAny.empty() || anyIn(mAny, { actor, target })) &&
				       (mAll.empty() || allIn(mAll, { actor, target })) &&
				       (pAny.empty() || anyIn(pAny, { actor, target, performer })) &&
				       (pAll.empty() || allIn(pAll, { actor, target, performer }));
			}

			// OStim's hasAction for entry a_i.
			bool SceneHas(const SceneRegistry::Scene& a_scene, std::size_t a_i) const
			{
				return std::ranges::any_of(a_scene.actions, [&](const SceneRegistry::SceneAction& a) { return Matches(a, a_i); });
			}
			bool SceneHasAny(const SceneRegistry::Scene& a_scene) const
			{
				for (std::size_t i = 0; i < types.size(); ++i) {
					if (SceneHas(a_scene, i)) {
						return true;
					}
				}
				return false;
			}
			bool SceneHasAll(const SceneRegistry::Scene& a_scene) const
			{
				for (std::size_t i = 0; i < types.size(); ++i) {
					if (!SceneHas(a_scene, i)) {
						return false;
					}
				}
				return true;
			}
			// OStim's hasOnlyListedActions.
			bool SceneHasOnlyListed(const SceneRegistry::Scene& a_scene) const
			{
				return std::ranges::all_of(a_scene.actions, [&](const SceneRegistry::SceneAction& a) {
					for (std::size_t i = 0; i < types.size(); ++i) {
						if (Matches(a, i)) {
							return true;
						}
					}
					return false;
				});
			}
		};
		std::string FindRandomSceneSuperload(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_args);
		std::vector<std::string> GetScenesInRange(std::monostate, std::string a_id, std::vector<RE::Actor*> a_actors, std::int32_t a_distance);
		std::string FindRandomScene(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_anyTags, std::vector<std::string> a_allTags, std::vector<std::string> a_anyActions, std::vector<std::string> a_allActions, std::int32_t a_position, std::vector<std::string> a_anyActorTags, std::vector<std::string> a_allActorTags);
		std::vector<std::string> SplitCSV(std::monostate, std::string a_csv);
		std::vector<std::string> GetAllScenes(std::monostate);
	}

	// ---- FourMetadata: the rest of OMetadata (OStim's PapyrusMetadata) ----
	// Natives for what Papyrus can't do cheaply; FourMetadata.psc builds
	// OMetadata's 282 functions on them under OStim's names.

	namespace Natives::FourMetadata
	{
		std::vector<std::string> ScenesToNames(std::monostate, std::vector<std::string> a_ids);
		std::string GetAnimationId(std::monostate, std::string a_id, std::int32_t a_index);
		std::vector<std::string> GetActorRequirements(std::monostate, std::string a_id, std::int32_t a_position);
		std::vector<std::string> GetActionTypes(std::monostate, std::string a_id);
		std::vector<std::int32_t> ActionRolesOf(const std::string& a_id, int a_which);
		std::vector<std::int32_t> GetActionActors(std::monostate, std::string a_id);
		std::vector<std::int32_t> GetActionTargets(std::monostate, std::string a_id);
		std::vector<std::int32_t> GetActionPerformers(std::monostate, std::string a_id);
		std::vector<std::string> GetActionTags(std::monostate, std::string a_id, std::int32_t a_index);
		std::vector<std::string> GetAllActionsTags(std::monostate, std::string a_id);
		bool ListHasAny(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted);
		bool ListHasAll(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted);
		std::vector<std::string> ListOverlap(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted);
		std::vector<std::int32_t> IntsOfCSV(std::monostate, std::string a_csv);
		std::vector<float> FloatsOfCSV(std::monostate, std::string a_csv);
		std::vector<std::int32_t> IntListOverlap(std::monostate, std::vector<std::int32_t> a_have, std::vector<std::int32_t> a_wanted);
		std::vector<float> FloatListOverlap(std::monostate, std::vector<float> a_have, std::vector<float> a_wanted);
		std::vector<std::string> StringListOverlap(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted);
		std::vector<std::int32_t> FindActionsBy(std::monostate, std::string a_id, std::vector<std::string> a_types, std::int32_t a_mode, std::vector<std::int32_t> a_positions, std::vector<std::int32_t> a_positions2);
		const Actions::Side& SideOf(const SceneRegistry::SceneAction& a_action, int a_role);
		std::vector<std::int32_t> FindActionsSuperload(std::monostate, std::string a_id, std::vector<std::string> a_filters);
		bool HasCustomAction(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::int32_t a_kind, std::string a_record);
		std::int32_t GetCustomActionInt(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, std::int32_t a_fallback);
		float GetCustomActionFloat(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, float a_fallback);
		std::string GetCustomActionString(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, std::string a_fallback);
		std::vector<std::int32_t> GetCustomActionIntList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record);
		std::vector<float> GetCustomActionFloatList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record);
		std::vector<std::string> GetCustomActionStringList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record);

		// OStim's GetCustomSceneActor{Int,Float}{Min,Max,Sum,Product}: a_record's
		// values on every side the role at a_position takes in the scene's
		// actions (actor, target and performer). a_op 0 min, 1 max (a_start is
		// the fallback when there's none), 2 sum, 3 product (a_start is the
		// starting value).
		template <class T>
		T CustomSceneActorAggregate(const std::string& a_id, std::int32_t a_position, const std::string& a_record, std::int32_t a_op, T a_start)
		{
			const auto scene = SceneRegistry::Find(a_id);
			const auto key = LowerCopy(a_record);
			std::optional<T> best;
			T                acc = a_start;
			if (scene) {
				for (const auto& action : scene->actions) {
					const std::size_t roles[3] = { action.actor, action.target, action.performer };
					for (int side = 0; side < 3; ++side) {
						if (RoleIndex(roles[side]) != a_position) {
							continue;
						}
						const auto& values = SideOf(action, side);
						T           value{};
						if constexpr (std::is_same_v<T, std::int32_t>) {
							const auto it = values.ints.find(key);
							if (it == values.ints.end()) {
								continue;
							}
							value = it->second;
						} else {
							const auto it = values.floats.find(key);
							if (it == values.floats.end()) {
								continue;
							}
							value = it->second;
						}
						switch (a_op) {
						case 0: best = best ? std::min(*best, value) : value; break;
						case 1: best = best ? std::max(*best, value) : value; break;
						case 2: acc += value; break;
						default: acc *= value; break;
						}
					}
				}
			}
			return a_op <= 1 ? best.value_or(a_start) : acc;
		}
		std::int32_t GetCustomSceneActorInt(std::monostate, std::string a_id, std::int32_t a_position, std::string a_record, std::int32_t a_op, std::int32_t a_start);
		float GetCustomSceneActorFloat(std::monostate, std::string a_id, std::int32_t a_position, std::string a_record, std::int32_t a_op, float a_start);
		std::string GetName(std::monostate, std::string a_id);
		bool IsTransition(std::monostate, std::string a_id);
		std::string GetDestination(std::monostate, std::string a_id);
		std::vector<std::string> GetSceneTags(std::monostate, std::string a_id);
		std::string GetFurnitureType(std::monostate, std::string a_id);
		std::int32_t GetMaxSpeed(std::monostate, std::string a_id);
		std::int32_t GetDefaultSpeed(std::monostate, std::string a_id);
		std::vector<std::string> GetNavigations(std::monostate, std::string a_id);
		std::string GetAutoTransition(std::monostate, std::string a_id, std::string a_type);
		std::string GetAutoTransitionForActor(std::monostate, std::string a_id, std::int32_t a_position, std::string a_type);
		bool IsNoRandomSelection(std::monostate, std::string a_id);
		std::string GetModpack(std::monostate, std::string a_id);
	}

	// FourActor (OActor): per-actor metadata, kept while they're in a scene.
	inline std::mutex                                                g_actorMetaLock;
	struct ActorMeta
	{
		std::vector<std::string>                     tags;
		std::unordered_map<std::string, float>       floats;
		std::unordered_map<std::string, std::string> strings;
	};
	inline std::unordered_map<std::uint32_t, ActorMeta> g_actorMeta;
	void ForgetActorMeta(const std::vector<std::uint32_t>& a_ids);

	namespace Natives::FourActor
	{
		bool HasMetadata(std::monostate, RE::Actor* a_actor, std::string a_meta);
		void AddMetadata(std::monostate, RE::Actor* a_actor, std::string a_meta);
		std::vector<std::string> GetMetadata(std::monostate, RE::Actor* a_actor);
		bool HasMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key);
		float GetMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key);
		void SetMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key, float a_value);
		bool HasMetaString(std::monostate, RE::Actor* a_actor, std::string a_key);
		std::string GetMetaString(std::monostate, RE::Actor* a_actor, std::string a_key);
		void SetMetaString(std::monostate, RE::Actor* a_actor, std::string a_key, std::string a_value);
		float PlayExpression(std::monostate, RE::Actor* a_actor, std::string a_expression);
		void SetExpressionsEnabled(std::monostate, RE::Actor* a_actor, bool a_enabled, bool a_allowOverride);
		bool HasExpressionOverride(std::monostate, RE::Actor* a_actor);
		void UndressPartial(std::monostate, RE::Actor* a_actor, std::int32_t a_mask);
		void RedressPartial(std::monostate, RE::Actor* a_actor, std::int32_t a_mask);
		void ClearExpression(std::monostate, RE::Actor* a_actor);
	}
	std::vector<Furniture::Found> FurnitureAround(RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor);
	namespace Natives::FourFurniture
	{
		std::vector<RE::TESObjectREFR*> FindFurniture(std::monostate, std::int32_t a_actorCount, RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor);
		RE::TESObjectREFR* FindFurnitureOfType(std::monostate, std::string a_type, RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor);
		std::vector<float> GetOffset(std::monostate, RE::TESObjectREFR* a_ref);
	}
	namespace Natives::FourActorUtil
	{
		bool FulfillsCondition(std::monostate, RE::Actor* a_actor, RE::BGSPerk* a_condition);
		bool FulfillsAnyCondition(std::monostate, RE::Actor* a_actor, std::vector<RE::BGSPerk*> a_conditions);
		bool FulfillsAllConditions(std::monostate, RE::Actor* a_actor, std::vector<RE::BGSPerk*> a_conditions);
		std::vector<RE::Actor*> EmptyArray(std::monostate);
		std::vector<RE::Actor*> CreateArray(std::monostate, std::int32_t a_size, RE::Actor* a_filler);
		std::vector<RE::Actor*> ToArray(std::monostate, RE::Actor* a1, RE::Actor* a2, RE::Actor* a3, RE::Actor* a4, RE::Actor* a5, RE::Actor* a6, RE::Actor* a7, RE::Actor* a8, RE::Actor* a9, RE::Actor* a10);
		std::vector<RE::Actor*> Sort(std::monostate, std::vector<RE::Actor*> a_actors, std::vector<RE::Actor*> a_dominant, std::int32_t a_playerIndex);
		std::vector<RE::Actor*> GetActorsInRangeV2(std::monostate, RE::TESObjectREFR* a_center, float a_range, bool a_includeCenter, bool a_includePlayer, bool a_onlyKnown, RE::BGSPerk* a_condition);
		std::vector<std::string> ActorsToNames(std::monostate, std::vector<RE::Actor*> a_actors);
	}
	std::string SQ_Random(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_tags, std::string a_csv, std::int32_t a_mode);

	namespace Natives::FourFurniture
	{
		std::int32_t GetSceneID(std::monostate, RE::TESObjectREFR* a_ref);
	}

	// ---- C++ plugin interface (include/4Stim/PluginInterface.h, docs/API.md) ----
	// OStim's PluginInterfaceImplementation: an interface map other F4SE
	// plugins get with an InterfaceExchangeMessage, holding "Threads",
	// "Alignment" (4Stim's) and "SexToys" (SexToys.cpp). Threads and their
	// actors are handed out as objects that live for the session: a thread
	// keeps a copy of what it last was, so a stop listener can still read it.
	namespace API
	{
		class ActorImpl final : public FourStimAPI::ThreadActor
		{
		public:
			explicit ActorImpl(std::uint32_t a_id) :
				id(a_id) {}

			void* getGameActor() override { return RE::TESForm::GetFormByID<RE::Actor>(id); }
			void  undress() override
			{
				Undress::StripAll(id, true);
			}
			void redress() override
			{
				const auto i = id;
				F4SE::GetTaskInterface()->AddTask([i]() { Undress::Redress(i, true); });
			}
			void removeWeapons() override { Weapons("HolsterByID"sv); }
			void addWeapons() override { Weapons("RearmByID"sv); }
			float getExcitement() override { return Excitement::Get(id); }
			void  setExcitement(float a_excitement) override { Excitement::Set(id, a_excitement); }
			void  climax() override
			{
				Excitement::Orgasm(id, true);  // as OActor.Climax(act, true): now, stalled or not, the stall left as it is
			}

			std::uint32_t id;

		private:
			void Weapons(std::string_view a_function) const
			{
				if (g_vm) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, a_function, callback, static_cast<std::int32_t>(id));  // a form ID: Actors don't arrive
				}
			}
		};

		inline std::mutex                                                   g_apiLock;
		inline std::unordered_map<std::uint32_t, std::unique_ptr<ActorImpl>> g_apiActors;
		ActorImpl* ActorFor(std::uint32_t a_id);

		class ThreadImpl final : public FourStimAPI::Thread
		{
		public:
			explicit ThreadImpl(std::int32_t a_id) :
				id(a_id) {}

			// Copies what the running thread is now (main thread).
			void Refresh(const ActiveScene& a_active)
			{
				running = true;
				actors = a_active.actors;
				sceneID = LowerCopy(a_active.sceneID);
				speed = a_active.speed;
				furnitureType = a_active.furnitureType;
				furniture = a_active.furnitureRef;
				player = std::ranges::any_of(actors, [](std::uint32_t a) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a)); });
			}

			std::int32_t  getThreadID() override { return id; }
			bool          isPlayerThread() override { return player; }
			bool          isRunning() override { return running; }
			std::uint32_t getActorCount() override { return running ? static_cast<std::uint32_t>(actors.size()) : 0u; }
			FourStimAPI::ThreadActor* getActor(std::uint32_t a_position) override
			{
				return running && a_position < actors.size() ? ActorFor(actors[a_position]) : nullptr;
			}
			void forEachThreadActor(FourStimAPI::ThreadActorVisitor* a_visitor) override
			{
				if (!a_visitor || !running) {
					return;
				}
				for (const auto a : std::vector<std::uint32_t>(actors)) {
					if (!a_visitor->visit(ActorFor(a))) {
						break;
					}
				}
			}
			const char*  getCurrentSceneID() override { return sceneID.c_str(); }
			std::int32_t getSpeed() override { return speed; }
			const char*  getFurnitureType() override { return furnitureType.c_str(); }
			void*        getFurnitureObject() override { return furniture ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(furniture) : nullptr; }
			void         navigateTo(const char* a_sceneID) override { Natives::FourThread::ThreadGoTo(id, a_sceneID ? a_sceneID : "", false); }
			void         warpTo(const char* a_sceneID) override { Natives::FourThread::ThreadGoTo(id, a_sceneID ? a_sceneID : "", true); }
			void         setSpeed(std::int32_t a_speed) override { Natives::FourThread::SetSpeed({}, id, a_speed); }
			void         stop() override
			{
				WithThread(id, [](ActiveScene& a) { EndActiveScene(a, "asked by another plugin (Thread::stop)"); });
			}

			std::int32_t               id;
			bool                       running = false;
			bool                       player = false;
			std::vector<std::uint32_t> actors;
			std::string                sceneID;
			std::int32_t               speed = 0;
			std::string                furnitureType;
			std::uint32_t              furniture = 0;
		};

		inline std::unordered_map<std::int32_t, std::unique_ptr<ThreadImpl>> g_apiThreads;
		ThreadImpl* ThreadOf(const ActiveScene& a_active);

		// OStim's thread builder for C++: one of the Papyrus builders.
		class BuilderImpl final : public FourStimAPI::ThreadBuilder
		{
		public:
			explicit BuilderImpl(std::int32_t a_id) :
				id(a_id) {}

			static std::vector<RE::Actor*> Actors(std::uint32_t a_count, void** a_actors)
			{
				std::vector<RE::Actor*> out;
				for (std::uint32_t i = 0; a_actors && i < a_count; ++i) {
					out.push_back(static_cast<RE::Actor*>(a_actors[i]));
				}
				return out;
			}

			void setDominantActors(std::uint32_t a_count, void** a_actors) override { Natives::FourThreadBuilder::SetDominantActors({}, id, Actors(a_count, a_actors)); }
			void setFurniture(void* a_furniture) override { Natives::FourThreadBuilder::SetFurniture({}, id, static_cast<RE::TESObjectREFR*>(a_furniture)); }
			void setDuration(float a_seconds) override { Natives::FourThreadBuilder::SetDuration({}, id, a_seconds); }
			void setStartingScene(const char* a_scene) override { Natives::FourThreadBuilder::SetStartingAnimation({}, id, a_scene ? a_scene : ""); }
			void addStartingScene(const char* a_scene, float a_duration, bool a_navigateTo) override
			{
				Natives::FourThreadBuilder::AddStartingAnimation({}, id, a_scene ? a_scene : "", a_duration, a_navigateTo);
			}
			void setStartingSequence(const char* a_sequence) override { Natives::FourThreadBuilder::SetStartingSequence({}, id, a_sequence ? a_sequence : ""); }
			void concatStartingSequence(const char* a_sequence, bool a_navigateTo) override
			{
				Natives::FourThreadBuilder::ConcatStartingSequence({}, id, a_sequence ? a_sequence : "", a_navigateTo);
			}
			void endAfterSequence() override { Natives::FourThreadBuilder::EndAfterSequence({}, id); }
			void undressActors() override { Natives::FourThreadBuilder::UndressActors({}, id); }
			void noAutoMode() override { Natives::FourThreadBuilder::NoAutoMode({}, id); }
			void noPlayerControl() override { Natives::FourThreadBuilder::NoPlayerControl({}, id); }
			void noUndressing() override { Natives::FourThreadBuilder::NoUndressing({}, id); }
			void noFurniture() override { Natives::FourThreadBuilder::NoFurniture({}, id); }
			void setMetadataCSV(const char* a_metadata) override { Natives::FourThreadBuilder::SetMetadataCSV({}, id, a_metadata ? a_metadata : ""); }
			std::int32_t start() override
			{
				const auto thread = Natives::FourThreadBuilder::Start({}, id);
				delete this;
				return thread;
			}
			void cancel() override
			{
				Natives::FourThreadBuilder::Cancel({}, id);
				delete this;
			}

			std::int32_t id;
		};

		// As OStim's getVersion: the plugin's version packed as CommonLib's
		// Version::pack does (major << 24 | minor << 16 | patch << 4), so
		// 0.5.0 is 0x00050000 (OStim 7.5.1: 0x07050010).
		constexpr std::uint32_t PACKED_VERSION = (static_cast<std::uint32_t>(FOURSTIM_VERSION_MAJOR) & 0xFF) << 24 |
		                                         (static_cast<std::uint32_t>(FOURSTIM_VERSION_MINOR) & 0xFF) << 16 |
		                                         (static_cast<std::uint32_t>(FOURSTIM_VERSION_PATCH) & 0xFFF) << 4;

		using ThreadListeners = std::vector<FourStimAPI::ThreadEventListener*>;
		inline ThreadListeners                                     g_onStart, g_onSpeed, g_onNode, g_onStop;
		inline std::vector<FourStimAPI::ThreadActorEventListener*> g_onClimax;

		class ThreadInterfaceImpl final : public FourStimAPI::ThreadInterface
		{
		public:
			std::uint32_t getVersion() override { return PACKED_VERSION; }

			FourStimAPI::Thread* getThread(std::int32_t a_threadID) override
			{
				const auto active = FindThread(a_threadID);
				return active ? ThreadOf(*active) : nullptr;
			}

			void registerThreadStartListener(FourStimAPI::ThreadEventListener* a_listener) override { Add(g_onStart, a_listener); }
			void registerSpeedChangedListener(FourStimAPI::ThreadEventListener* a_listener) override { Add(g_onSpeed, a_listener); }
			void registerNodeChangedListener(FourStimAPI::ThreadEventListener* a_listener) override { Add(g_onNode, a_listener); }
			void registerClimaxListener(FourStimAPI::ThreadActorEventListener* a_listener) override
			{
				if (a_listener) {
					std::scoped_lock lock(g_apiLock);
					g_onClimax.push_back(a_listener);
				}
			}
			void registerThreadStopListener(FourStimAPI::ThreadEventListener* a_listener) override { Add(g_onStop, a_listener); }

			FourStimAPI::ThreadBuilder* createThreadBuilder(std::uint32_t a_count, void** a_actors) override
			{
				const auto id = BuilderCreate(BuilderImpl::Actors(a_count, a_actors));
				return id >= 0 ? new BuilderImpl(id) : nullptr;
			}

			FourStimAPI::ThreadActor* getActor(void* a_gameActor) override
			{
				const auto actor = static_cast<RE::Actor*>(a_gameActor);
				return actor && FindActiveScene(actor->GetFormID()) ? ActorFor(actor->GetFormID()) : nullptr;
			}

		private:
			static void Add(ThreadListeners& a_list, FourStimAPI::ThreadEventListener* a_listener)
			{
				if (a_listener) {
					std::scoped_lock lock(g_apiLock);
					a_list.push_back(a_listener);
				}
			}
		};

		// 4Stim's own: a thread's alignment, as the HUD's alignment menu edits it.
		class AlignmentInterfaceImpl final : public FourStimAPI::AlignmentInterface
		{
		public:
			std::uint32_t getVersion() override { return PACKED_VERSION; }

			bool getOffset(std::int32_t a_threadID, std::uint32_t a_position, FourStimAPI::AlignmentOffset* a_offset) override
			{
				const auto active = FindThread(a_threadID);
				if (!active || !a_offset || a_position >= active->actors.size()) {
					return false;
				}
				const auto scene = SceneRegistry::Find(active->sceneID);
				const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
				const auto sceneID = settled ? settled->id : active->sceneID;
				const auto found = Alignment::Find(active->alignKey, sceneID, a_position);
				auto       o = found.value_or(Alignment::Offset{});
				if (!found && settled && a_position < settled->actors.size()) {
					o.bend = settled->actors[a_position].penisBend;  // OStim's default: the scene's
				}
				*a_offset = { o.x, o.y, o.z, o.rot, o.scale, o.bend };
				return true;
			}

			bool setOffset(std::int32_t a_threadID, std::uint32_t a_position, const FourStimAPI::AlignmentOffset* a_offset) override
			{
				const auto active = FindThread(a_threadID);
				if (!active || !a_offset || a_position >= active->actors.size()) {
					return false;
				}
				Alignment::Offset o;
				o.x = a_offset->x;
				o.y = a_offset->y;
				o.z = a_offset->z;
				o.rot = a_offset->rot;
				o.scale = a_offset->scale;
				o.bend = std::clamp(a_offset->bend, -9.0F, 9.0F);
				WithThread(a_threadID, [a_position, o](ActiveScene& a) {
					if (a_position >= a.actors.size()) {
						return;
					}
					const auto scene = SceneRegistry::Find(a.sceneID);
					const auto id = scene ? SceneRegistry::RoleScene(scene)->id : a.sceneID;
					Alignment::Set(a.alignKey, id, a_position, o);
					LockScene(a);
				});
				return true;
			}

			bool resetOffset(std::int32_t a_threadID, std::uint32_t a_position) override
			{
				const auto active = FindThread(a_threadID);
				if (!active || a_position >= active->actors.size()) {
					return false;
				}
				WithThread(a_threadID, [a_position](ActiveScene& a) {
					if (a_position >= a.actors.size()) {
						return;
					}
					const auto scene = SceneRegistry::Find(a.sceneID);
					const auto id = scene ? SceneRegistry::RoleScene(scene)->id : a.sceneID;
					Alignment::Remove(a.alignKey, id, a_position);
					LockScene(a);
				});
				return true;
			}
		};

		class InterfaceMapImpl final : public FourStimAPI::InterfaceMap
		{
		public:
			FourStimAPI::PluginInterface* queryInterface(const char* a_name) override
			{
				std::scoped_lock lock(mapLock);
				const auto it = a_name ? map.find(a_name) : map.end();
				return it != map.end() ? it->second : nullptr;
			}
			bool addInterface(const char* a_name, FourStimAPI::PluginInterface* a_interface) override
			{
				std::scoped_lock lock(mapLock);
				return a_name && a_interface && map.emplace(a_name, a_interface).second;
			}
			FourStimAPI::PluginInterface* removeInterface(const char* a_name) override
			{
				std::scoped_lock lock(mapLock);
				const auto it = a_name ? map.find(a_name) : map.end();
				if (it == map.end()) {
					return nullptr;
				}
				const auto removed = it->second;
				map.erase(it);
				return removed;
			}

			std::mutex                                                     mapLock;
			std::unordered_map<std::string, FourStimAPI::PluginInterface*> map;
		};
		InterfaceMapImpl* Map();
		void OnPluginMessage(F4SE::MessagingInterface::Message* a_msg);
		void ForgetThreads();

		// SceneEvents' native listener: the threads' listeners (main thread).
		void NotifyNG(FourStimNG_API::Thread::ThreadEvent a_event, std::uint32_t a_threadID);
		void OnSceneEvent(SceneEvents::Kind a_kind, const std::vector<std::uint32_t>& a_actors, std::uint32_t a_actor);

		// ---- OStim's second C++ API, OstimNG-API (include/4Stim/FourStimNG-API.h) ----
		// OStim's ModAPI: a Scene interface (start / stop scenes) and a Thread
		// interface for external scene UIs, from the exports
		// RequestPluginAPI_Scene and RequestPluginAPI_Thread.

		namespace NG = FourStimNG_API;
		NG::Scene::APIResult StartNGScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene, std::vector<RE::Actor*> a_actors, std::uint32_t* a_threadID);

		class NGSceneInterface final : public NG::Scene::ISceneInterface
		{
		public:
			using R = NG::Scene::APIResult;

			R StartScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene, RE::Actor* a_actors[256], std::uint32_t* a_threadID) noexcept override
			{
				std::vector<RE::Actor*> actors;
				for (int i = 0; a_actors && i < 256 && a_actors[i]; ++i) {
					actors.push_back(a_actors[i]);
				}
				return StartNGScene(a_plugin, a_furniture, a_scene, std::move(actors), a_threadID);
			}
			R StartCoupleScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene, RE::Actor* a_dom, RE::Actor* a_sub, std::uint32_t* a_threadID) noexcept override
			{
				return StartNGScene(a_plugin, a_furniture, a_scene, { a_dom, a_sub }, a_threadID);
			}
			R StartThreesomeScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene, RE::Actor* a_1, RE::Actor* a_2, RE::Actor* a_3, std::uint32_t* a_threadID) noexcept override
			{
				return StartNGScene(a_plugin, a_furniture, a_scene, { a_1, a_2, a_3 }, a_threadID);
			}
			R StartFoursomeScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene, RE::Actor* a_1, RE::Actor* a_2, RE::Actor* a_3, RE::Actor* a_4, std::uint32_t* a_threadID) noexcept override
			{
				return StartNGScene(a_plugin, a_furniture, a_scene, { a_1, a_2, a_3, a_4 }, a_threadID);
			}
			R StopScene(const char*, std::uint32_t a_threadID) noexcept override
			{
				const auto id = static_cast<std::int32_t>(a_threadID);
				if (!FindThread(id)) {
					return R::Invalid;
				}
				WithThread(id, [](ActiveScene& a) { EndActiveScene(a, "asked by another plugin (Scene API StopScene)"); });
				return R::OK;
			}
			R SetAutoMode(const char*, std::uint32_t a_threadID, bool a_on) noexcept override
			{
				const auto id = static_cast<std::int32_t>(a_threadID);
				if (!FindThread(id)) {
					return R::Invalid;
				}
				a_on ? Natives::FourThread::StartAutoMode({}, id) : Natives::FourThread::StopAutoMode({}, id);
				return R::OK;
			}
			R TryGetAutoMode(const char*, std::uint32_t a_threadID, bool* a_on) noexcept override
			{
				const auto id = static_cast<std::int32_t>(a_threadID);
				if (!FindThread(id) || !a_on) {
					return R::Invalid;
				}
				*a_on = Natives::FourThread::IsInAutoMode({}, id);
				return R::OK;
			}
		};
		std::shared_ptr<const SceneRegistry::Scene> NavDestination(const std::string& a_to);
		FourStim::FocusedScene AsFocused(const ActiveScene& a_active);

		inline std::atomic<bool> g_externalUI = false;

		class NGThreadInterface final : public NG::Thread::IThreadInterface
		{
		public:
			using R = NG::Thread::APIResult;

			void Notify(NG::Thread::ThreadEvent a_event, std::uint32_t a_threadID)
			{
				std::vector<std::pair<NG::Thread::ThreadEventCallback, void*>> calls;
				{
					std::scoped_lock lock(callbackLock);
					calls = events;
				}
				for (const auto& [callback, data] : calls) {
					callback(a_event, a_threadID, data);
				}
			}
			void NotifyControl(NG::Thread::Controls a_control, std::uint32_t a_threadID)
			{
				std::vector<std::pair<NG::Thread::ControlEventCallback, void*>> calls;
				{
					std::scoped_lock lock(callbackLock);
					calls = controls;
				}
				for (const auto& [callback, data] : calls) {
					callback(a_control, a_threadID, data);
				}
			}

			std::uint32_t GetPlayerThreadID() noexcept override { return 0; }
			bool          IsThreadValid(std::uint32_t a_id) noexcept override { return FindThread(static_cast<std::int32_t>(a_id)) != nullptr; }
			const char*   GetCurrentSceneID(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				return active ? ThreadOf(*active)->sceneID.c_str() : nullptr;
			}
			std::uint32_t GetActorCount(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				return active ? static_cast<std::uint32_t>(active->actors.size()) : 0u;
			}
			std::uint32_t GetActors(std::uint32_t a_id, NG::Thread::ActorData* a_buffer, std::uint32_t a_size) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				if (!active || !a_buffer) {
					return 0;
				}
				std::uint32_t count = 0;
				for (const auto id : active->actors) {
					if (count >= a_size) {
						break;
					}
					const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
					a_buffer[count++] = { id, Excitement::Get(id), actor && SceneRegistry::SexOf(actor) == SceneRegistry::Sex::kFemale,
						actor && ActorProperties::HasPenis(actor), Excitement::TimesClimaxed(id) };
				}
				return count;
			}

			// OStim's: none in a transition, a sequence or with no player control.
			std::vector<const SceneRegistry::Navigation*> Navigations(const ActiveScene& a_active, std::shared_ptr<const SceneRegistry::Scene>& a_scene)
			{
				std::vector<const SceneRegistry::Navigation*> out;
				a_scene = SceneRegistry::Find(a_active.sceneID);
				if (!a_scene || a_scene->IsTransition() || a_active.sequence || a_active.noPlayerControl) {
					return out;
				}
				for (const auto& nav : a_scene->navigations) {
					const auto first = SceneRegistry::Find(nav.to);
					const auto dest = NavDestination(nav.to);
					if (first && dest && SexesFit(*first, a_active.actors) && SexesFit(*dest, a_active.actors) && SceneFitsFurniture(*dest, a_active.furnitureType)) {
						out.push_back(&nav);
					}
				}
				return out;
			}
			std::uint32_t GetNavigationCount(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				std::shared_ptr<const SceneRegistry::Scene> scene;
				return active ? static_cast<std::uint32_t>(Navigations(*active, scene).size()) : 0u;
			}
			std::uint32_t GetNavigationOptions(std::uint32_t a_id, NG::Thread::NavigationData* a_buffer, std::uint32_t a_size) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				if (!active || !a_buffer) {
					return 0;
				}
				std::shared_ptr<const SceneRegistry::Scene> scene;
				const auto navs = Navigations(*active, scene);
				navScene = scene;  // keeps the strings alive
				navText.clear();
				navKeep.clear();
				navText.reserve(navs.size() * 2);  // no reallocation below: the c_str()s handed out stay valid
				std::uint32_t count = 0;
				for (const auto nav : navs) {
					if (count >= a_size) {
						break;
					}
					const auto first = SceneRegistry::Find(nav->to);
					const auto dest = NavDestination(nav->to);
					navKeep.push_back(first);
					navKeep.push_back(dest);
					const auto& label = nav->label.empty() ? dest->name : nav->label;
					navText.push_back(FormatNavLabel(label, AsFocused(*active)));
					navText.push_back(!nav->icon.empty() ? nav->icon : dest->icon);
					a_buffer[count++] = { first->id.c_str(), dest->id.c_str(), navText[navText.size() - 1].c_str(), navText[navText.size() - 2].c_str(),
						nav->border.c_str(), first->IsTransition() };
				}
				return count;
			}
			R NavigateToScene(std::uint32_t a_id, const char* a_scene) noexcept override
			{
				if (!a_scene) {
					return R::Invalid;
				}
				if (!FindThread(static_cast<std::int32_t>(a_id))) {
					return R::Invalid;
				}
				Natives::FourThread::ThreadGoTo(static_cast<std::int32_t>(a_id), a_scene, false);
				return R::OK;
			}
			bool IsTransition(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				const auto scene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
				return scene && scene->IsTransition();
			}
			bool IsInSequence(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				return active && active->sequence;
			}
			bool IsAutoMode(std::uint32_t a_id) noexcept override { return Natives::FourThread::IsInAutoMode({}, static_cast<std::int32_t>(a_id)); }
			bool IsPlayerControlDisabled(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				return active && active->noPlayerControl;
			}
			void RegisterEventCallback(NG::Thread::ThreadEventCallback a_callback, void* a_data) noexcept override
			{
				if (a_callback) {
					std::scoped_lock lock(callbackLock);
					events.emplace_back(a_callback, a_data);
				}
			}
			void UnregisterEventCallback(NG::Thread::ThreadEventCallback a_callback) noexcept override
			{
				std::scoped_lock lock(callbackLock);
				std::erase_if(events, [&](const auto& e) { return e.first == a_callback; });
			}
			void RegisterControlCallback(NG::Thread::ControlEventCallback a_callback, void* a_data) noexcept override
			{
				if (a_callback) {
					std::scoped_lock lock(callbackLock);
					controls.emplace_back(a_callback, a_data);
				}
			}
			void UnregisterControlCallback(NG::Thread::ControlEventCallback a_callback) noexcept override
			{
				std::scoped_lock lock(callbackLock);
				std::erase_if(controls, [&](const auto& e) { return e.first == a_callback; });
			}
			void SetExternalUIEnabled(bool a_enabled) noexcept override
			{
				g_externalUI = a_enabled;
				HUD::SetExternalUI(a_enabled);
				REX::INFO("Thread API: external UI {}", a_enabled ? "on (4Stim's HUD stays closed)" : "off");
			}
			void GetKeyData(NG::Thread::KeyData* a_out) noexcept override
			{
				if (!a_out) {
					return;
				}
				const auto& st = g_settings;
				const auto k = [](std::uint32_t a_key) { return static_cast<std::int32_t>(a_key); };
				*a_out = { k(st.navUpKey), k(st.navDownKey), k(st.navLeftKey), k(st.navRightKey), k(st.navYesKey), k(st.endKey), k(st.navToggleKey),
					k(st.searchKey), k(st.alignmentKey), k(st.hotkey), k(st.npcSceneKey), k(st.speedUpKey), k(st.speedDownKey), k(st.pullOutKey),
					k(g_autoConfig.key), k(st.freeCamKey), k(st.hideUIKey) };
			}

			const char* GetCurrentNodeName(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				nameScene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
				return nameScene ? nameScene->name.c_str() : nullptr;
			}
			std::int32_t GetCurrentSpeed(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				return active ? active->speed : 0;
			}
			std::int32_t GetMaxSpeed(std::uint32_t a_id) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				const auto scene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
				return scene && !scene->speeds.empty() ? static_cast<std::int32_t>(scene->speeds.size()) - 1 : 0;
			}
			R SetSpeed(std::uint32_t a_id, std::int32_t a_speed) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				const auto scene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
				if (!scene) {
					return R::Invalid;
				}
				if (a_speed < 0 || a_speed >= static_cast<std::int32_t>(scene->speeds.size())) {
					return R::Failed;
				}
				Natives::FourThread::SetSpeed({}, static_cast<std::int32_t>(a_id), a_speed);
				return R::OK;
			}

			bool GetActorAlignment(std::uint32_t a_id, std::uint32_t a_index, NG::Thread::ActorAlignmentData* a_out) noexcept override
			{
				FourStimAPI::AlignmentOffset o;
				if (!a_out || !alignment->getOffset(static_cast<std::int32_t>(a_id), a_index, &o)) {
					return false;
				}
				*a_out = { o.x, o.y, o.z, o.scale, o.rot, o.bend };
				return true;
			}
			R SetActorAlignment(std::uint32_t a_id, std::uint32_t a_index, const NG::Thread::ActorAlignmentData* a_data) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				if (!a_data || !active) {
					return R::Invalid;
				}
				if (a_index >= active->actors.size()) {
					return R::Failed;
				}
				const FourStimAPI::AlignmentOffset o{ a_data->offsetX, a_data->offsetY, a_data->offsetZ, a_data->rotation, a_data->scale, a_data->sosBend };
				return alignment->setOffset(static_cast<std::int32_t>(a_id), a_index, &o) ? R::OK : R::Failed;
			}

			// OStim's: scenes whose name has a_query, sorted by name, that the
			// player's thread could play (actor count, furniture, no transitions).
			std::uint32_t SearchScenes(const char* a_query, NG::Thread::SceneSearchResult* a_buffer, std::uint32_t a_size) noexcept override
			{
				const auto active = FindThread(0);
				if (!a_query || !a_buffer || a_size == 0 || !active) {
					return 0;
				}
				const auto query = LowerCopy(a_query);
				searchKeep.clear();
				for (const auto& summary : SceneRegistry::List(active->actors.size())) {
					const auto scene = SceneRegistry::Find(summary.id);
					if (scene && !scene->IsTransition() && SceneFitsFurniture(*scene, active->furnitureType) &&
						LowerCopy(scene->name).find(query) != std::string::npos) {
						searchKeep.push_back(scene);
					}
				}
				std::ranges::sort(searchKeep, [](const auto& x, const auto& y) { return x->name < y->name; });
				std::uint32_t count = 0;
				for (const auto& scene : searchKeep) {
					if (count >= a_size) {
						break;
					}
					a_buffer[count++] = { scene->id.c_str(), scene->name.c_str(), static_cast<std::uint32_t>(scene->actors.size()) };
				}
				return count;
			}
			bool GetSceneInfo(const char* a_scene, NG::Thread::SceneSearchResult* a_out) noexcept override
			{
				infoScene = a_scene ? SceneRegistry::Find(a_scene) : nullptr;
				if (!infoScene || !a_out) {
					return false;
				}
				*a_out = { infoScene->id.c_str(), infoScene->name.c_str(), static_cast<std::uint32_t>(infoScene->actors.size()) };
				return true;
			}
			R NavigateToSearchResult(std::uint32_t a_id, const char* a_scene) noexcept override
			{
				if (!a_scene || !FindThread(static_cast<std::int32_t>(a_id))) {
					return R::Invalid;
				}
				if (!SceneRegistry::Find(a_scene)) {
					return R::Failed;
				}
				Natives::FourThread::ThreadGoTo(static_cast<std::int32_t>(a_id), a_scene, true);  // OStim's ChangeNode: straight there
				return R::OK;
			}

			// The options menu: 4Stim's Utility tab's options pages
			// (Options.h), for the player's (or the watched) scene.
			void RebuildOptionsTree() noexcept override { Options::Reload(); }
			std::uint32_t GetOptionsItemCount() noexcept override { return static_cast<std::uint32_t>(OptionItems().size()); }
			std::vector<Options::Entry> OptionItems()
			{
				const auto ids = GetPlayerScene().ActorIDs();
				auto rows = Options::Rows(ids);
				if (Options::InPage(ids) && !rows.empty()) {
					rows.erase(rows.begin());  // its "Back"
				}
				return rows;
			}
			std::uint32_t GetOptionsItems(NG::Thread::OptionsMenuItem* a_buffer, std::uint32_t a_size) noexcept override
			{
				if (!a_buffer || a_size == 0) {
					return 0;
				}
				const auto ids = GetPlayerScene().ActorIDs();
				const bool inPage = Options::InPage(ids);
				options = OptionItems();
				optionIDs.clear();
				for (std::size_t i = 0; i < options.size(); ++i) {
					optionIDs.push_back(std::to_string(i));
				}
				static const std::string back = "-1";
				static const std::string backTitle = "back";
				static const std::string exitTitle = "exit";
				static const std::string backIcon = "4Stim/symbols/return";
				static const std::string exitIcon = "4Stim/symbols/x";
				static const std::string red = "ff0000";
				static const std::string none;
				std::uint32_t count = 0;
				a_buffer[count++] = { back.c_str(), inPage ? backTitle.c_str() : exitTitle.c_str(), inPage ? backIcon.c_str() : exitIcon.c_str(), red.c_str(), none.c_str() };
				for (std::size_t i = 0; i < options.size() && count < a_size; ++i) {
					a_buffer[count++] = { optionIDs[i].c_str(), options[i].label.c_str(), options[i].icon.c_str(), options[i].border.c_str(), none.c_str() };
				}
				return count;
			}
			bool SelectOptionsItem(std::int32_t a_index) noexcept override
			{
				const auto ids = GetPlayerScene().ActorIDs();
				if (a_index < 0) {
					if (!Options::InPage(ids)) {
						return false;  // exit, as OStim's at the root
					}
					const auto rows = Options::Rows(ids);
					if (!rows.empty()) {
						Options::Pick(rows.front().id, ids);  // its "Back"
					}
					HUD::RefreshUtility();
					return true;
				}
				const auto items = OptionItems();
				if (static_cast<std::size_t>(a_index) < items.size()) {
					Options::Pick(items[a_index].id, ids);
					HUD::RefreshUtility();
				}
				return true;
			}
			bool IsOptionsAtRoot() noexcept override { return !Options::InPage(GetPlayerScene().ActorIDs()); }

			bool IsActorInAnyThread(std::uint32_t a_actor) noexcept override { return FindActiveScene(a_actor) != nullptr; }
			bool HasCompatibleNode(std::uint32_t a_id, const std::uint32_t* a_actors, std::uint32_t a_count) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				if (!active || !a_actors || a_count == 0) {
					return false;
				}
				std::vector<std::uint32_t> cast(a_actors, a_actors + a_count);
				if (std::ranges::any_of(cast, [](std::uint32_t a) { return !RE::TESForm::GetFormByID<RE::Actor>(a); })) {
					return false;
				}
				return std::ranges::any_of(ScenesForActors(cast, active->furnitureType), [](const auto& s) { return !s->IsTransition(); });
			}
			std::int32_t MigrateThread(std::uint32_t a_id, const std::uint32_t* a_actors, std::uint32_t a_count,
				void (*a_onComplete)(std::int32_t, void*), void* a_context, int a_delayMs) noexcept override;
			bool         IsUnrestrictedNavigation() noexcept override { return SceneRegistry::Rules().unrestricted; }
			bool         IsIntendedSexOnly() noexcept override { return SceneRegistry::Rules().intendedSexOnly; }
			std::int32_t GetActorPosition(std::uint32_t a_id, std::uint32_t a_actor) noexcept override
			{
				const auto active = FindThread(static_cast<std::int32_t>(a_id));
				if (!active) {
					return -1;
				}
				const auto it = std::ranges::find(active->actors, a_actor);
				return it != active->actors.end() ? static_cast<std::int32_t>(it - active->actors.begin()) : -1;
			}

			AlignmentInterfaceImpl* alignment = nullptr;

		private:
			std::mutex                                                       callbackLock;
			std::vector<std::pair<NG::Thread::ThreadEventCallback, void*>>   events;
			std::vector<std::pair<NG::Thread::ControlEventCallback, void*>>  controls;
			std::shared_ptr<const SceneRegistry::Scene>                      navScene, nameScene, infoScene;
			std::vector<std::shared_ptr<const SceneRegistry::Scene>>         navKeep, searchKeep;
			std::vector<std::string>                                         navText;
			std::vector<Options::Entry>                                      options;
			std::vector<std::string>                                         optionIDs;
		};
		NGThreadInterface* NGThreads();
		NGSceneInterface* NGScenes();
		void NotifyNG(FourStimNG_API::Thread::ThreadEvent a_event, std::uint32_t a_threadID);
		std::string SelfTest();
		void NotifyNGControl(FourStimNG_API::Thread::Controls a_control);
	}
	std::string TestPluginAPI(std::monostate);
	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm);
	void OnGameSaved(const F4SE::SerializationInterface* a_intfc);
	void OnGameLoadedData(const F4SE::SerializationInterface* a_intfc);
	void OnGameReverted(const F4SE::SerializationInterface*);
	void ResumeSavedScenes();
}
