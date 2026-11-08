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

namespace
{
	using namespace std::literals;

	bool g_lockScenes = true;  // bLockScenes: see "Scene lock"

	// Where actors were last placed for a scene, and when: the scene that
	// starts with them takes its spot and heading from here (the actors'
	// own positions can already be a little off). Main thread.
	struct PlacedSpot
	{
		RE::NiPoint3                          position;
		float                                 heading = 0.0F;
		std::chrono::steady_clock::time_point when;
	};
	std::unordered_map<std::uint32_t, PlacedSpot> g_placedSpots;

	// Where NPCs stood before their scene (OStim's positionBefore), to put
	// them back when it ends (bResetPosition, OStim's "Reset position").
	std::unordered_map<std::uint32_t, PlacedSpot> g_positionBefore;
	bool                                          g_resetPosition = true;
	bool                                          g_endOnDamage = true;         // bEndOnDamage (4Stim's)
	bool                                          g_stayWhenInterrupted = true;  // bStayWhenInterrupted (4Stim's)

	// Turns a_actor to a_heading (radians). Like OStim: Actor::SetHeading
	// only works on the player; NPCs get their reference angle set.
	void FaceHeading(RE::Actor* a_actor, float a_heading)
	{
		if (!a_actor) {
			return;
		}
		if (a_actor == RE::PlayerCharacter::GetSingleton()) {
			a_actor->SetHeading(a_heading);
		} else {
			a_actor->SetAngleOnReference(RE::NiPoint3{ 0.0F, 0.0F, a_heading });
		}
	}

	// A drawn weapon put away, as OStim's lock does for the player
	// (sheatheWeaponry: "rae WeaponSheathe"), here for every scene actor: the
	// game's own sheathe (what Skyrim's Actor.SheatheWeapon calls; Fallout
	// 4's Papyrus has no SheatheWeapon). Taking the weapon off isn't enough:
	// an NPC who drew stays in the drawn state with bare fists, and that
	// state's arm and body pose shows over the scene's animations (the
	// owner's log: both scenes where an actor "barely responded" and was
	// turned 90 degrees in a chair had her weapon out the whole time, put
	// away by unequipping three times to no effect). Main thread.
	bool SheatheNow(RE::Actor* a_actor, std::string_view a_why)
	{
		if (!a_actor || !a_actor->GetWeaponMagicDrawn()) {
			return false;
		}
		a_actor->DrawWeaponMagicHands(false);
		REX::INFO("Weapons: {:08X} puts their weapon away ({})", a_actor->GetFormID(), a_why);
		return true;
	}

	bool g_guardScenes = true;  // bGuardScenes: see GuardScenes

	// 4Stim's own fade to black (SetUseFades) is up: the scene camera doesn't
	// wait for that one, so the camera change happens while it's black.
	std::atomic<bool> g_sceneFade = false;

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
	std::unordered_map<std::uint32_t, GuardKicks> g_guardKicks;
	std::unordered_map<std::uint32_t, GuardKicks> g_holsterKicks;  // the scene guard's weapons put away, per actor, this scene

	// Papyrus-facing script name. Must be "FourStim", not "4Stim" -- Papyrus
	// identifiers can't start with a digit. The plugin/project itself is
	// still called 4Stim everywhere else (xmake project name, log text).
	// Must match the .psc script name exactly.
	constexpr auto SCRIPT_NAME = "FourStim"sv;

	constexpr float PI_F = 3.14159265358979323846f;

	// Stashed at Papyrus registration time so we can dispatch calls INTO
	// Papyrus-native engine functions (e.g. Actor.SetRestrained) later,
	// not just receive calls Papyrus makes to us.
	RE::BSScript::IVirtualMachine* g_vm = nullptr;

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
	Settings g_settings;

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
	AutoConfig g_autoConfig;

	// OStim's setting names -> the 4Stim names LoadSettings reads (all
	// lowercase). 4Stim.ini uses OStim's names wherever OStim has the
	// setting, so a Skyrim user can look it up in OStim's docs; the older
	// 4Stim names keep working. The settings OStim has that 4Stim didn't
	// map to internal names only (the right-hand side), which aren't
	// documented.
	enum class OStimUnit
	{
		kSame,
		kMilliseconds,    // OStim's value is ms, 4Stim's seconds
		kFurnitureSteps,  // OStim's SetFurnitureSearchDistance: (value + 1) * 100 units
	};
	const std::unordered_map<std::string, std::pair<std::string_view, OStimUnit>> OSTIM_SETTING_NAMES{
		// Keys (4Stim's are Windows virtual-key codes, OStim's DirectX scan codes).
		{ "setkeymap", { "ihotkey", OStimUnit::kSame } },
		{ "keynpcscenestart", { "inpcscenekey", OStimUnit::kSame } },
		{ "setkeyup", { "ispeedupkey", OStimUnit::kSame } },
		{ "setkeydown", { "ispeeddownkey", OStimUnit::kSame } },
		{ "setcontroltoggle", { "iautomodekey", OStimUnit::kSame } },
		{ "setpullout", { "ipulloutkey", OStimUnit::kSame } },
		{ "setosaendkey", { "iendkey", OStimUnit::kSame } },
		{ "keysearch", { "isearchkey", OStimUnit::kSame } },
		{ "keyalignment", { "ialignmentkey", OStimUnit::kSame } },
		{ "setfreecamtogglekey", { "ifreecamkey", OStimUnit::kSame } },
		{ "keyhideui", { "ihideuikey", OStimUnit::kSame } },
		{ "setosaupkey", { "inavupkey", OStimUnit::kSame } },
		{ "setosadownkey", { "inavdownkey", OStimUnit::kSame } },
		{ "setosaleftkey", { "inavleftkey", OStimUnit::kSame } },
		{ "setosarightkey", { "inavrightkey", OStimUnit::kSame } },
		{ "setosayeskey", { "inavyeskey", OStimUnit::kSame } },
		{ "setosatogkey", { "inavtogglekey", OStimUnit::kSame } },
		// General, camera, furniture.
		{ "setresetposition", { "bresetposition", OStimUnit::kSame } },
		{ "setonlygayanimsingayscenes", { "bmatchsex", OStimUnit::kSame } },
		{ "setfurnituresearchdistance", { "ffurnitureradius", OStimUnit::kFurnitureSteps } },
		{ "setusefreecam", { "busefreecam", OStimUnit::kSame } },
		{ "setcameraspeed", { "fcameraspeed", OStimUnit::kSame } },
		{ "setfreecamfov", { "ffreecamfov", OStimUnit::kSame } },
		{ "setforcefirstperson", { "bforcefirstperson", OStimUnit::kSame } },
		{ "setusefades", { "busefades", OStimUnit::kSame } },
		{ "setuseintroscenes", { "buseintroscenes", OStimUnit::kSame } },
		{ "setscaling", { "bdisablescaling", OStimUnit::kSame } },
		{ "setschlongbending", { "bdisablepenisbend", OStimUnit::kSame } },
		{ "nofacialexpressions", { "bnofacialexpressions", OStimUnit::kSame } },
		{ "setexpressiondurationmin", { "fexpressiondurationmin", OStimUnit::kMilliseconds } },
		{ "setexpressiondurationmax", { "fexpressiondurationmax", OStimUnit::kMilliseconds } },
		{ "moanintervalmin", { "fmoanintervalmin", OStimUnit::kMilliseconds } },
		{ "moanintervalmax", { "fmoanintervalmax", OStimUnit::kMilliseconds } },
		{ "moanvolume", { "fmoanvolume", OStimUnit::kSame } },
		{ "soundvolume", { "fsoundvolume", OStimUnit::kSame } },
		{ "setonlylightindark", { "bonlylightindark", OStimUnit::kSame } },
		{ "setuseautofades", { "buseautofades", OStimUnit::kSame } },
		{ "npcsceneduration", { "fnpcsceneduration", OStimUnit::kMilliseconds } },
		// Excitement and climax.
		{ "setsexexcitementmult", { "fmaleexcitementmult", OStimUnit::kSame } },
		{ "setfemalesexexcitementmult", { "ffemaleexcitementmult", OStimUnit::kSame } },
		{ "excitementdecayrate", { "fexcitementdecayrate", OStimUnit::kSame } },
		{ "excitementdecaygraceperiod", { "fexcitementdecaygrace", OStimUnit::kMilliseconds } },
		{ "postorgasmexcitement", { "fpostclimaxexcitement", OStimUnit::kSame } },
		{ "postorgasmexcitementmax", { "fpostclimaxexcitementmax", OStimUnit::kSame } },
		{ "setautoclimaxanims", { "bclimaxscenes", OStimUnit::kSame } },
		{ "endonplayerorgasm", { "bendonplayerclimax", OStimUnit::kSame } },
		{ "setendonorgasm", { "bendonmaleclimax", OStimUnit::kSame } },
		{ "setendonsuborgasm", { "bendonfemaleclimax", OStimUnit::kSame } },
		{ "setendonbothorgasm", { "bendonallclimax", OStimUnit::kSame } },
		{ "endnpcsceneonorgasm", { "bendnpcscenesonclimax", OStimUnit::kSame } },
		{ "setuserumble", { "bclimaxrumble", OStimUnit::kSame } },
		{ "setblurorgasms", { "bblurorgasms", OStimUnit::kSame } },
		{ "setslowmoorgasms", { "bslowmoorgasms", OStimUnit::kSame } },
		// Auto mode.
		{ "setaicontrol", { "bautomodeplayer", OStimUnit::kSame } },
		{ "setforceaiformasturbation", { "bautomodesolo", OStimUnit::kSame } },
		{ "setforceaiinconsensualscenes", { "bautomodepartner", OStimUnit::kSame } },
		// Excitement bars.
		{ "enableplayerbar", { "benableplayerbar", OStimUnit::kSame } },
		{ "enablenpcbar", { "benablenpcbar", OStimUnit::kSame } },
		{ "setautohidebar", { "bautohidebars", OStimUnit::kSame } },
		{ "setmatchcolortogender", { "bmatchcolortogender", OStimUnit::kSame } },
		{ "automodeanimdurationmin", { "fautomodescenemin", OStimUnit::kMilliseconds } },
		{ "automodeanimdurationmax", { "fautomodescenemax", OStimUnit::kMilliseconds } },
		{ "automodeforeplaychance", { "iforeplaychance", OStimUnit::kSame } },
		{ "automodeforeplaythresholdmin", { "fforeplayendmin", OStimUnit::kSame } },
		{ "automodeforeplaythresholdmax", { "fforeplayendmax", OStimUnit::kSame } },
		{ "automodepulloutchance", { "ipulloutchance", OStimUnit::kSame } },
		{ "automodepulloutthresholdmin", { "fpulloutmin", OStimUnit::kSame } },
		{ "automodepulloutthresholdmax", { "fpulloutmax", OStimUnit::kSame } },
		{ "navigationdistancemax", { "iautomodemaxsteps", OStimUnit::kSame } },
		{ "automodelimittonavigationdistance", { "bautomodelimittonavigation", OStimUnit::kSame } },
		{ "setactorspeedcontrol", { "bautospeed", OStimUnit::kSame } },
		{ "autospeedcontrolintervalmin", { "fautospeedintervalmin", OStimUnit::kMilliseconds } },
		{ "autospeedcontrolintervalmax", { "fautospeedintervalmax", OStimUnit::kMilliseconds } },
		{ "autospeedcontrolexcitementmin", { "fautospeedexcitementmin", OStimUnit::kSame } },
		{ "autospeedcontrolexcitementmax", { "fautospeedexcitementmax", OStimUnit::kSame } },
		// Undressing.
		{ "setalwaysundressatstart", { "bundressatstart", OStimUnit::kSame } },
		{ "setpartialundressing", { "bpartialundress", OStimUnit::kSame } },
		{ "setundressifneed", { "bfullundressmidscene", OStimUnit::kSame } },
		{ "setanimateredress", { "banimateredress", OStimUnit::kSame } },
		// Alignment.
		{ "alignmentgroupbysex", { "balignbysex", OStimUnit::kSame } },
		{ "alignmentgroupbyheight", { "balignbyheight", OStimUnit::kSame } },
		{ "alignmentgroupbyheels", { "balignbyheels", OStimUnit::kSame } },
	};

	// 4Stim.ini, then the MCM's file over it (docs/MCM.md). Again whenever
	// the MCM changes a setting (FourStim.ReloadSettings).
	// A controller button by its name in 4Stim.ini's [Controller] (the MCM's
	// choices): the game's gamepad code, 0 for none. The HUD keeps the d-pad,
	// A and B.
	std::uint32_t PadButton(std::string_view a_name)
	{
		std::string name(a_name);
		std::ranges::transform(name, name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		constexpr std::array<std::pair<std::string_view, std::uint32_t>, 9> BUTTONS{ {
			{ "lb", 0x100 }, { "rb", 0x200 }, { "lt", 0x9 }, { "rt", 0xA }, { "x", 0x4000 }, { "y", 0x8000 },
			{ "ls", 0x40 }, { "rs", 0x80 }, { "back", 0x20 },
		} };
		for (const auto& [n, code] : BUTTONS) {
			if (name == n) {
				return code;
			}
		}
		return 0;
	}

	void LoadSettings()
	{
		std::ifstream base("Data/F4SE/Plugins/4Stim.ini");
		if (!base) {
			REX::INFO("Settings: no 4Stim.ini, using defaults");
		}
		std::ifstream mcm(MCM::SettingsPath());
		auto trim = [](std::string s) {
			const auto b = s.find_first_not_of(" \t\r");
			const auto e = s.find_last_not_of(" \t\r");
			return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
		};
		std::string line;
		for (int pass = 0; pass < 2; ++pass) {
		auto& in = pass == 0 ? base : mcm;
		while (in && std::getline(in, line)) {
			line = trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto key = trim(line.substr(0, eq));
			auto value = trim(line.substr(eq + 1));
			if (const auto c = value.find(';'); c != std::string::npos) {
				value = trim(value.substr(0, c));
			}
			std::ranges::transform(key, key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			// The MCM's names (b4S_SetEndOnOrgasm, a dropdown's index...) as 4Stim.ini's.
			if (pass == 1) {
				const auto ini = MCM::ToIni(key, value);
				if (!ini) {
					continue;
				}
				key = ini->first;
				value = ini->second;
			}
			try {
				// OStim's setting names (its MCM export keys, MCMTable.h) are read
				// as the 4Stim names they match. OStim's durations are in ms,
				// 4Stim's in seconds; its furniture search distance is in OStim's
				// own steps ((value + 1) * 100 units).
				if (const auto alias = OSTIM_SETTING_NAMES.find(key); alias != OSTIM_SETTING_NAMES.end()) {
					const auto& [name, unit] = alias->second;
					key = std::string(name);
					if (unit == OStimUnit::kMilliseconds) {
						value = std::to_string(std::stof(value) / 1000.0F);
					} else if (unit == OStimUnit::kFurnitureSteps) {
						value = std::to_string((std::stof(value) + 1.0F) * 100.0F);
					}
				}
				if (key == "ihotkey") {
					g_settings.hotkey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "inpcscenekey") {
					g_settings.npcSceneKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "bresumescenes") {
					g_settings.resumeScenes = std::stoi(value) != 0;
				} else if (key == "stargetmode") {
					auto v = value;
					std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
					g_settings.targetMode = (v == "proximity") ? 1 : 0;
				} else if (key == "fmaxdistance") {
					g_settings.maxDistance = std::stof(value);
				} else if (key == "fcrosshaircone") {
					g_settings.crosshairCone = std::stof(value);
				} else if (key == "fproximityradius") {
					g_settings.proximityRadius = std::stof(value);
				} else if (key == "ffreecameraspeed") {
					g_settings.freeCamSpeedMult = std::max(std::stof(value), 0.01F);  // older 4Stim setting: a multiplier
				} else if (key == "fcameraspeed") {
					g_settings.freeCamSpeed = std::max(std::stof(value), 0.01F);
					g_settings.freeCamSpeedMult = -1.0F;
				} else if (key == "busefreecam") {
					g_settings.useFreeCam = std::stoi(value) != 0;
				} else if (key == "ffreecamfov") {
					g_settings.freeCamFOV = std::clamp(std::stof(value), 0.0F, 150.0F);
				} else if (key == "bforcefirstperson") {
					g_settings.forceFirstPerson = std::stoi(value) != 0;
				} else if (key == "busefades") {
					g_settings.useFades = std::stoi(value) != 0;
				} else if (key == "bnofacialexpressions") {
					Expressions::Settings().disabled = std::stoi(value) != 0;
				} else if (key == "fexpressiondurationmin") {
					Expressions::Settings().durationMin = std::max(std::stof(value), 0.1F);
				} else if (key == "fexpressiondurationmax") {
					Expressions::Settings().durationMax = std::max(std::stof(value), 0.1F);
				} else if (key == "fmoanintervalmin") {
					Sounds::Settings().moanIntervalMin = std::max(std::stof(value), 0.2F);
				} else if (key == "fmoanintervalmax") {
					Sounds::Settings().moanIntervalMax = std::max(std::stof(value), 0.2F);
				} else if (key == "fmoanvolume") {
					Sounds::Settings().moanVolume = std::clamp(std::stof(value), 0.0F, 1.0F);
				} else if (key == "bsounds") {
					Sounds::Settings().disabled = std::stoi(value) == 0;
				} else if (key == "fsoundvolume") {
					Sounds::Settings().soundVolume = std::clamp(std::stof(value), 0.0F, 1.0F);
				} else if (key == "bonlylightindark") {
					Lights::Settings().onlyInDark = std::stoi(value) != 0;
				} else if (key == "flightleveldark") {
					Lights::Settings().darkLevel = std::stof(value);
				} else if (key == "smalelight") {
					Lights::Settings().maleLight = value;
				} else if (key == "sfemalelight") {
					Lights::Settings().femaleLight = value;
				} else if (key == "splayerlight") {
					Lights::Settings().playerLight = value;
				} else if (key == "isoundflags") {
					Sounds::Settings().flags = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "isoundpriority") {
					Sounds::Settings().priority = static_cast<std::uint8_t>(std::clamp(std::stoi(value, nullptr, 0), 0, 255));
				} else if (key == "bdisablescaling") {
					Scaling::Settings().disabled = std::stoi(value) != 0;
				} else if (key == "buseintroscenes") {
					g_settings.useIntroScenes = std::stoi(value) != 0;
				} else if (key == "setcustomtimescale") {
					g_settings.customTimescale = std::max(std::stof(value), 0.0F);
				} else if (key == "setusescreenshake") {
					Excitement::Settings().useScreenShake = std::stoi(value) != 0;
				} else if (key == "addactorsatstart") {
					g_settings.addActorsAtStart = std::stoi(value) != 0;
				} else if (key == "buseautofades") {
					g_settings.useAutoFades = std::stoi(value) != 0;
				} else if (key == "fnpcsceneduration") {
					g_settings.npcSceneDuration = std::max(std::stof(value), 0.0F);
				} else if (key == "ipulloutkey") {
					g_settings.pullOutKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "iendkey") {
					g_settings.endKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "isearchkey") {
					g_settings.searchKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "ialignmentkey") {
					g_settings.alignmentKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "ifreecamkey") {
					g_settings.freeCamKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "ihideuikey") {
					g_settings.hideUIKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "ispeedupkey") {
					g_settings.speedUpKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "ispeeddownkey") {
					g_settings.speedDownKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "stheme") {
					g_settings.hudTheme = value;
				}
				// Controller buttons (a chain of their own, C1061).
				if (key == "spadscheme" || key == "fhudfadetime") {
					if (key == "spadscheme") {
						auto v = value;
						std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
						g_settings.padClassic = v == "classic";
					} else {
						g_settings.hudFadeTime = std::clamp(std::stof(value), 0.0F, 600.0F);
					}
				} else if (key.starts_with("spad")) {
					const auto button = PadButton(value);
					if (key == "spadhotkey") {
						g_settings.padHotkey = button;
					} else if (key == "spadnpcscene") {
						g_settings.padNpcScene = button;
					} else if (key == "spadspeedup") {
						g_settings.padSpeedUp = button;
					} else if (key == "spadspeeddown") {
						g_settings.padSpeedDown = button;
					} else if (key == "spadautomode") {
						g_settings.padAutoMode = button;
					} else if (key == "spadpullout") {
						g_settings.padPullOut = button;
					} else if (key == "spadend") {
						g_settings.padEnd = button;
					} else if (key == "spadsearch") {
						g_settings.padSearch = button;
					} else if (key == "spadalign") {
						g_settings.padAlign = button;
					} else if (key == "spadfreecam") {
						g_settings.padFreeCam = button;
					} else if (key == "spadhideui") {
						g_settings.padHideUI = button;
					}
				}
				// OStim's bar settings (a chain of their own, C1061).
				if (key == "benableplayerbar") {
					g_settings.playerBar = std::stoi(value) != 0;
				} else if (key == "benablenpcbar") {
					g_settings.npcBar = std::stoi(value) != 0;
				} else if (key == "bautohidebars") {
					g_settings.autoHideBars = std::stoi(value) != 0;
				} else if (key == "bmatchcolortogender") {
					g_settings.matchBarColor = std::stoi(value) != 0;
				}
				// OStim's navigation keys (a chain of their own, C1061).
				if (key == "inavupkey") {
					g_settings.navUpKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "inavdownkey") {
					g_settings.navDownKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "inavleftkey") {
					g_settings.navLeftKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "inavrightkey") {
					g_settings.navRightKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "inavyeskey") {
					g_settings.navYesKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "inavtogglekey") {
					g_settings.navToggleKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				}
				// (A new chain: MSVC allows only so many else-ifs in one, C1061.)
				if (key == "shudcontrols") {
					auto v = value;
					std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
					g_settings.hudColumns = v == "columns";
				} else if (key == "benablehud") {
					g_settings.hudEnabled = std::stoi(value) != 0;
				} else if (key == "factorradius") {
					g_settings.actorRadius = std::stof(value);
				} else if (key == "ffurnitureradius") {
					g_settings.furnitureRadius = std::stof(value);
				} else if (key == "ffurnitureheight") {
					g_settings.furnitureHeight = std::stof(value);
				} else if (key == "blogfurniture") {
					g_settings.logFurniture = std::stoi(value) != 0;
				} else if (key == "setenablefurniture") {
					g_settings.useFurniture = std::stoi(value) != 0;
				} else if (key == "setbedrealignment") {
					Furniture::Settings().bedRealignment = std::stof(value);
				} else if (key == "bedoffset") {
					Furniture::Settings().bedOffset = std::stof(value);
				} else if (key == "setselectfurniture") {
					g_settings.selectFurniture = std::stoi(value) != 0;
				} else if (key == "setresetclutter") {
					g_settings.resetClutter = std::stoi(value) != 0;
				} else if (key == "setresetclutterradius") {
					g_settings.resetClutterRadius = std::stof(value) * 100.0F;  // OStim's steps of 100 units
				} else if (key == "bmatchsex") {
					g_settings.matchSex = std::stoi(value) != 0;
				} else if (key == "playeralwaysdomstraight") {
					g_settings.playerAlwaysDomStraight = std::stoi(value) != 0;
				} else if (key == "playeralwayssubstraight") {
					g_settings.playerAlwaysSubStraight = std::stoi(value) != 0;
				} else if (key == "playeralwaysdomgay") {
					g_settings.playerAlwaysDomGay = std::stoi(value) != 0;
				} else if (key == "playeralwayssubgay") {
					g_settings.playerAlwaysSubGay = std::stoi(value) != 0;
				} else if (key == "playerselectrolestraight") {
					g_settings.playerSelectRoleStraight = std::stoi(value) != 0;
				} else if (key == "playerselectrolegay") {
					g_settings.playerSelectRoleGay = std::stoi(value) != 0;
				} else if (key == "playerselectrolethreesome") {
					g_settings.playerSelectRoleThreesome = std::stoi(value) != 0;
				} else if (key == "unrestrictednavigation") {
					SceneRegistry::Rules().unrestricted = std::stoi(value) != 0;
				} else if (key == "futausemalerole") {
					ActorProperties::Settings().futaUseMaleRole = std::stoi(value) != 0;
				} else if (key == "futausemaleexcitement") {
					ActorProperties::Settings().futaUseMaleExcitement = std::stoi(value) != 0;
				} else if (key == "futausemaleclimax") {
					ActorProperties::Settings().futaUseMaleClimax = std::stoi(value) != 0;
				} else if (key == "bdisablepenisbend") {
					PenisBend::Settings().disabled = std::stoi(value) != 0;
				} else if (key == "fpenisbendstep") {
					PenisBend::Settings().stepDegrees = std::clamp(std::stof(value), -45.0F, 45.0F);
				} else if (key == "spenisbendnode" || key == "spenisbendup" || key == "spenisbenddown" || key == "spenisflaccid") {
					// Comma lists: nodes, base first; their shares of a bend up / down.
					std::vector<std::string> parts;
					for (std::size_t from = 0; from <= value.size();) {
						const auto to = std::min(value.find(',', from), value.size());
						std::string part = value.substr(from, to - from);
						std::erase_if(part, [](unsigned char c) { return std::isspace(c); });
						if (!part.empty()) {
							parts.push_back(std::move(part));
						}
						from = to + 1;
					}
					auto& pb = PenisBend::Settings();
					if (key == "spenisbendnode") {
						if (!parts.empty()) {
							pb.nodes = parts;
						}
					} else {
						std::vector<float> shares;
						for (const auto& part : parts) {
							try {
								shares.push_back(std::stof(part));
							} catch (...) {
								shares.push_back(0.0F);
							}
						}
						(key == "spenisbendup" ? pb.shareUp : key == "spenisbenddown" ? pb.shareDown : pb.flaccid) = std::move(shares);
					}
				} else if (key == "ipenisbendaxis") {
					PenisBend::Settings().axis = std::clamp(std::stoi(value), 0, 2);
				} else if (key == "berections") {
					Erection::Settings().enabled = std::stoi(value) != 0;
				} else if (key == "unequipstraponifnotneeded") {
					EquipObjects::Settings().unequipStrapOnIfNotNeeded = std::stoi(value) != 0;
				} else if (key == "sdefaultstrapon") {
					EquipObjects::Settings().defaults[EquipObjects::FEMALE]["strapon"] = value;  // OStim's Default Strap-On (0x1)
				} else if (key == "splayerstrapon") {
					EquipObjects::Settings().defaults[EquipObjects::PLAYER]["strapon"] = value;
				} else if (key == "sdefaulttonguemale") {
					EquipObjects::Settings().defaults[EquipObjects::MALE]["tongue"] = value;
				} else if (key == "sdefaulttonguefemale") {
					EquipObjects::Settings().defaults[EquipObjects::FEMALE]["tongue"] = value;
				} else if (key == "splayertongue") {
					EquipObjects::Settings().defaults[EquipObjects::PLAYER]["tongue"] = value;
				} else if (key == "maledialoguecountdownmin") {
					Sounds::Settings().maleDialogueCountdownMin = std::max(std::stoi(value), 0);
				} else if (key == "maledialoguecountdownmax") {
					Sounds::Settings().maleDialogueCountdownMax = std::max(std::stoi(value), 0);
				} else if (key == "femaledialoguecountdownmin") {
					Sounds::Settings().femaleDialogueCountdownMin = std::max(std::stoi(value), 0);
				} else if (key == "femaledialoguecountdownmax") {
					Sounds::Settings().femaleDialogueCountdownMax = std::max(std::stoi(value), 0);
				} else if (key == "playerdialogue") {
					Sounds::Settings().playerDialogue = std::stoi(value) != 0;
				} else if (key == "preventsamevoicecrosstalk") {
					Sounds::Settings().preventSameVoiceCrossTalk = std::stoi(value) != 0;
				} else if (key == "splayervoice") {
					Sounds::Settings().playerVoice = value;
				} else if (key == "bfurnitureoutoftheway") {
					g_settings.furnitureOutOfWay = std::stoi(value) != 0;
				} else if (key == "bresetphysicsafterscene") {
					Physics::Settings().resetAfterScene = std::stoi(value) != 0;
				} else if (key == "bpassthroughwalls") {
					g_settings.passThroughWalls = std::stoi(value) != 0;
				} else if (key == "bcollisionwatch") {
					g_settings.collisionWatch = std::stoi(value) != 0;
				} else if (key == "bplayeraidriven") {
					g_settings.playerAIDriven = std::stoi(value) != 0;
				} else if (key == "btranslationlock") {
					g_settings.translationLock = std::stoi(value) != 0;
				} else if (key == "btimeanimcues") {
					g_settings.timeAnimCues = std::stoi(value) != 0;
				} else if (key == "bloganimevents") {
					g_settings.logAnimEvents = std::stoi(value) != 0;
				} else if (key == "benableexcitement") {
					Excitement::Settings().enabled = std::stoi(value) != 0;
				} else if (key == "fmaleexcitementmult") {
					Excitement::Settings().maleMult = std::max(std::stof(value), 0.0F);
				} else if (key == "ffemaleexcitementmult") {
					Excitement::Settings().femaleMult = std::max(std::stof(value), 0.0F);
				} else if (key == "fexcitementdecayrate") {
					Excitement::Settings().decayRate = std::max(std::stof(value), 0.0F);
				} else if (key == "fexcitementdecaygrace") {
					Excitement::Settings().decayGrace = std::max(std::stof(value), 0.0F);
				} else if (key == "fpostclimaxexcitement") {
					Excitement::Settings().postClimax = std::clamp(std::stof(value), 0.0F, 99.0F);
				} else if (key == "fpostclimaxexcitementmax") {
					Excitement::Settings().postClimaxMax = std::clamp(std::stof(value), 0.0F, 99.0F);
				} else if (key == "bclimaxscenes") {
					Excitement::Settings().climaxScenes = std::stoi(value) != 0;
				} else if (key == "bendonplayerclimax") {
					Excitement::Settings().endOnPlayer = std::stoi(value) != 0;
				} else if (key == "bendonmaleclimax") {
					Excitement::Settings().endOnMale = std::stoi(value) != 0;
				} else if (key == "bendonfemaleclimax") {
					Excitement::Settings().endOnFemale = std::stoi(value) != 0;
				} else if (key == "bendonallclimax") {
					Excitement::Settings().endOnAll = std::stoi(value) != 0;
				}
				// (A new chain: MSVC allows only so many else-ifs in one, C1061.)
				if (key == "bendnpcscenesonclimax") {
					Excitement::Settings().endNPCScenes = std::stoi(value) != 0;
				} else if (key == "fclimaxenddelay") {
					Excitement::Settings().endDelay = std::clamp(std::stof(value), 0.0F, 60.0F);
				} else if (key == "fclimaxshake") {
					Excitement::Settings().shake = std::clamp(std::stof(value), 0.0F, 5.0F);
				} else if (key == "fclimaxblur") {
					Excitement::Settings().blur = std::clamp(std::stof(value), 0.0F, 1.0F);
				} else if (key == "fclimaxflash") {
					Excitement::Settings().flash = std::clamp(std::stof(value), 0.0F, 1.0F);
				} else if (key == "fclimaxglowsize") {
					Excitement::Settings().glowSize = std::clamp(std::stof(value), 0.01F, 0.5F);
				} else if (key == "bblurorgasms") {
					Excitement::Settings().blurOn = std::stoi(value) != 0;
				} else if (key == "bslowmoorgasms") {
					Excitement::Settings().slowMo = std::stoi(value) != 0;
				} else if (key == "bclimaxrumble") {
					Excitement::Settings().rumble = std::stoi(value) != 0;
				} else if (key == "balignbysex") {
					Alignment::Settings().groupBySex = std::stoi(value) != 0;
				} else if (key == "balignbyheight") {
					Alignment::Settings().groupByHeight = std::stoi(value) != 0;
				} else if (key == "balignbyheels") {
					Alignment::Settings().groupByHeels = std::stoi(value) != 0;
				} else if (key == "bresetposition") {
					g_resetPosition = std::stoi(value) != 0;
				} else if (key == "bendondamage") {
					g_endOnDamage = std::stoi(value) != 0;
				} else if (key == "bstaywheninterrupted") {
					g_stayWhenInterrupted = std::stoi(value) != 0;
				} else if (key == "blockscenes") {
					g_lockScenes = std::stoi(value) != 0;
				} else if (key == "bguardscenes") {
					g_guardScenes = std::stoi(value) != 0;
				} else if (key == "bundress") {
					Undress::Settings().enabled = std::stoi(value) != 0;
				} else if (key == "bundressatstart") {
					Undress::Settings().atStart = std::stoi(value) != 0;
				} else if (key == "bpartialundress") {
					Undress::Settings().partial = std::stoi(value) != 0;
				} else if (key == "bfullundressmidscene") {
					Undress::Settings().fullMidScene = std::stoi(value) != 0;
				} else if (key == "bundressplayer") {
					Undress::Settings().player = std::stoi(value) != 0;
				} else if (key == "bredress") {
					Undress::Settings().redress = std::stoi(value) != 0;
				} else if (key == "fundressitemdelay") {
					Undress::Settings().itemDelay = std::clamp(std::stof(value), 0.0F, 5.0F);
				} else if (key == "banimateredress") {
					Undress::Settings().animateRedress = std::stoi(value) != 0;
				} else if (key == "setremoveweaponsatstart") {
					Undress::Settings().removeWeaponsAtStart = std::stoi(value) != 0;
				} else if (key == "setremoveweaponswithslot") {
					Undress::Settings().removeWeaponsWithSlot = std::stoi(value);
				} else if (key == "setundressingslotmask") {
					// OStim's name: a mask, bit 0 = slot 30.
					const auto       mask = static_cast<std::uint32_t>(std::stoll(value, nullptr, 0));
					std::vector<int> slots;
					for (int slot = 30; slot <= 61; ++slot) {
						if (mask & (1u << (slot - 30))) {
							slots.push_back(slot);
						}
					}
					Undress::Settings().slots = std::move(slots);
				} else if (key.starts_with("bundressslot") && key.size() == 14) {
					// One slot each (the MCM's Undressing slots page).
					const int slot = std::stoi(key.substr(12));
					if (slot >= 30 && slot <= 61) {
						auto&      slots = Undress::Settings().slots;
						const bool on = std::stoi(value) != 0;
						std::erase(slots, slot);
						if (on) {
							slots.push_back(slot);
							std::ranges::sort(slots);
						}
					}
				} else if (key == "sundressslots") {
					std::vector<int> slots;
					std::string      token;
					for (const char ch : value + ",") {
						if (ch == ',' || ch == ' ') {
							if (!token.empty()) {
								const int slot = std::stoi(token);
								if (slot >= 30 && slot <= 61) {
									slots.push_back(slot);
								}
								token.clear();
							}
						} else {
							token += ch;
						}
					}
					Undress::Settings().slots = std::move(slots);
				}
				// (A new chain, C1061 again.)
				if (key == "bautomodeplayer") {
					g_autoConfig.player = std::stoi(value) != 0;
				} else if (key == "bautomodesolo") {
					g_autoConfig.playerSolo = std::stoi(value) != 0;
				} else if (key == "bautomodepartner") {
					g_autoConfig.playerPartner = std::stoi(value) != 0;
				} else if (key == "bautomodenpc") {
					g_autoConfig.npc = std::stoi(value) != 0;
				} else if (key == "iautomodekey") {
					g_autoConfig.key = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "fautomodescenemin") {
					g_autoConfig.sceneMin = std::max(std::stof(value), 2.0F);
				} else if (key == "fautomodescenemax") {
					g_autoConfig.sceneMax = std::max(std::stof(value), 2.0F);
				} else if (key == "iforeplaychance") {
					g_autoConfig.foreplayChance = std::clamp(std::stoi(value), 0, 100);
				} else if (key == "fforeplayendmin") {
					g_autoConfig.foreplayMin = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "fforeplayendmax") {
					g_autoConfig.foreplayMax = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "ipulloutchance") {
					g_autoConfig.pulloutChance = std::clamp(std::stoi(value), 0, 100);
				} else if (key == "fpulloutmin") {
					g_autoConfig.pulloutMin = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "fpulloutmax") {
					g_autoConfig.pulloutMax = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "iautomodemaxsteps") {
					g_autoConfig.maxSteps = std::clamp(std::stoi(value), 1, 50);
				} else if (key == "bautomodelimittonavigation") {
					g_autoConfig.limitToNavigation = std::stoi(value) != 0;
				} else if (key == "bautomodestandingonfloor") {
					g_autoConfig.standingOnFloor = std::stoi(value) != 0;
				} else if (key == "bautospeed") {
					g_autoConfig.autoSpeed = std::stoi(value) != 0;
				} else if (key == "fautospeedintervalmin") {
					g_autoConfig.speedIntervalMin = std::max(std::stof(value), 0.5F);
				} else if (key == "fautospeedintervalmax") {
					g_autoConfig.speedIntervalMax = std::max(std::stof(value), 0.5F);
				}
				// (A new chain: MSVC allows only so many else-ifs in one, C1061.)
				if (key == "fautospeedexcitementmin") {
					g_autoConfig.speedExcitementMin = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "fautospeedexcitementmax") {
					g_autoConfig.speedExcitementMax = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "ftransitionlead") {
					g_settings.transitionLead = std::clamp(std::stof(value), -2.0F, 5.0F);
				}
				// Sex toys (SexToys.h) and playbackSpeed.
				if (SexToys::ReadSetting(key, value) || Intiface::ReadSetting(key, value)) {
				} else if (key == "buseplaybackspeed") {
					g_settings.usePlaybackSpeed = std::stoi(value) != 0;
				}
				// OStim's look directions (EyeLook.h).
				if (key == "bdisableeyelook") {
					EyeLook::Settings().disabled = std::stoi(value) != 0;
				} else if (key == "flookmaxside") {
					EyeLook::Settings().maxSide = std::clamp(std::stof(value), 0.0F, 60.0F);
				} else if (key == "flookmaxup") {
					EyeLook::Settings().maxUp = std::clamp(std::stof(value), 0.0F, 60.0F);
				} else if (key == "slefteyenode") {
					EyeLook::Settings().leftEye = value;
				} else if (key == "srighteyenode") {
					EyeLook::Settings().rightEye = value;
				}
			} catch (...) {
				REX::WARN("Settings: couldn't read \"{}\" for {}", value, key);
			}
		}
		}
		REX::INFO("Settings: hotkey 0x{:X}, target mode {}, max distance {}, cone {}, radius {}, speed keys 0x{:X}/0x{:X}",
			g_settings.hotkey, g_settings.targetMode == 1 ? "Proximity" : "Crosshair",
			g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius,
			g_settings.speedUpKey, g_settings.speedDownKey);
		REX::INFO("Settings: keys pull out 0x{:X}, end 0x{:X}, search 0x{:X}, align 0x{:X}, free camera 0x{:X}, hide HUD 0x{:X}",
			g_settings.pullOutKey, g_settings.endKey, g_settings.searchKey, g_settings.alignmentKey, g_settings.freeCamKey, g_settings.hideUIKey);
		if (g_settings.freeCamSpeedMult > 0.0F) {
			REX::INFO("Settings: free camera {}, speed x{} (fFreeCameraSpeed), FOV {}, first person after {}, fades {}, NPC scenes {}s, resume on load {}",
				g_settings.useFreeCam, g_settings.freeCamSpeedMult, g_settings.freeCamFOV, g_settings.forceFirstPerson, g_settings.useFades,
				g_settings.npcSceneDuration, g_settings.resumeScenes);
		} else {
			REX::INFO("Settings: free camera {}, speed {}, FOV {}, first person after {}, fades {}, NPC scenes {}s, resume on load {}",
				g_settings.useFreeCam, g_settings.freeCamSpeed, g_settings.freeCamFOV, g_settings.forceFirstPerson, g_settings.useFades,
				g_settings.npcSceneDuration, g_settings.resumeScenes);
		}
		REX::INFO("Settings: HUD {}, theme \"{}\", transition lead {}s, match sex {}", g_settings.hudEnabled ? "on" : "off", g_settings.hudTheme, g_settings.transitionLead, g_settings.matchSex ? "on" : "off");
		SceneRegistry::Rules().intendedSexOnly = g_settings.matchSex;
		// The HUD's theme, on / off and layout, here so a reload (the MCM's
		// changes) applies them too; HUD::LoadConfig then reads the theme.
		HUD::SetOptions(g_settings.hudTheme, g_settings.hudEnabled, g_settings.hudColumns);
		HUD::SetControllerOptions(g_settings.padClassic, g_settings.hudFadeTime);
		HUD::SetBarOptions(g_settings.playerBar, g_settings.npcBar, g_settings.autoHideBars, g_settings.matchBarColor);
		HUD::SetNavigationKeys({ g_settings.navUpKey, g_settings.navDownKey, g_settings.navLeftKey, g_settings.navRightKey,
			g_settings.navYesKey, g_settings.navToggleKey });
		const auto& pb = PenisBend::Settings();
		REX::INFO("Settings: penis bend {}, {} degree(s) per step over {} node(s) from {}, axis {}; erections {}", pb.disabled ? "off" : "on", pb.stepDegrees, pb.nodes.size(),
			pb.nodes.empty() ? std::string{} : pb.nodes.front(), "xyz"[pb.axis],
			Erection::Settings().enabled ? "on" : "off");
		const auto& eyes = EyeLook::Settings();
		REX::INFO("Settings: eye directions {}, {} degree(s) sideways, {} up / down, bones {} / {}", eyes.disabled ? "off" : "on", eyes.maxSide, eyes.maxUp,
			eyes.leftEye, eyes.rightEye);
		const auto& fu = ActorProperties::Settings();
		REX::INFO("Settings: roles: player dom/sub straight {}/{}, gay {}/{}, choose straight {} gay {} threesome {}, unrestricted {}; futa male role {} excitement {} climax {}; strap-on off when not needed {}",
			g_settings.playerAlwaysDomStraight, g_settings.playerAlwaysSubStraight, g_settings.playerAlwaysDomGay, g_settings.playerAlwaysSubGay,
			g_settings.playerSelectRoleStraight, g_settings.playerSelectRoleGay, g_settings.playerSelectRoleThreesome, SceneRegistry::Rules().unrestricted,
			fu.futaUseMaleRole, fu.futaUseMaleExcitement, fu.futaUseMaleClimax, EquipObjects::Settings().unequipStrapOnIfNotNeeded);
		REX::INFO("Settings: furniture {} within {} (height {}), {}, clutter reset {} ({}), log {}", g_settings.useFurniture ? "on" : "off",
			g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.selectFurniture ? "asked" : "the nearest", g_settings.resetClutter ? "on" : "off",
			g_settings.resetClutterRadius, g_settings.logFurniture ? "on" : "off");
		const auto& ex = Excitement::Settings();
		REX::INFO("Settings: excitement {}, x{} male / x{} female, decay {}/s after {}s, climax scenes {}, end on climax: player {} male {} female {} all {} NPC scenes {} (after {}s)",
			ex.enabled ? "on" : "off", ex.maleMult, ex.femaleMult, ex.decayRate, ex.decayGrace, ex.climaxScenes,
			ex.endOnPlayer, ex.endOnMale, ex.endOnFemale, ex.endOnAll, ex.endNPCScenes, ex.endDelay);
		const auto& un = Undress::Settings();
		REX::INFO("Settings: undress {} (at start {}, partial {}, full mid-scene {}, player {}, redress {}), {} slot(s), {}s per item, animated redress {}",
			un.enabled, un.atStart, un.partial, un.fullMidScene, un.player, un.redress, un.slots.size(), un.itemDelay, un.animateRedress);
		auto& ac = g_autoConfig;
		ac.sceneMax = std::max(ac.sceneMax, ac.sceneMin);
		ac.foreplayMax = std::max(ac.foreplayMax, ac.foreplayMin);
		ac.pulloutMax = std::max(ac.pulloutMax, ac.pulloutMin);
		ac.speedIntervalMax = std::max(ac.speedIntervalMax, ac.speedIntervalMin);
		REX::INFO("Settings: auto mode player {} NPC {} key 0x{:X}, {}-{}s per scene, foreplay {}% (to {}-{}), pull-out {}% (at {}-{}), {} steps, auto speed {}",
			ac.player, ac.npc, ac.key, ac.sceneMin, ac.sceneMax, ac.foreplayChance, ac.foreplayMin, ac.foreplayMax,
			ac.pulloutChance, ac.pulloutMin, ac.pulloutMax, ac.maxSteps, ac.autoSpeed);
	}


	// ---- The focused scene ----
	// The scene the player is in, or an NPC scene the player started to watch
	// with the free camera (see FourStim::FocusedScene). Lets the hotkey open
	// the HUD instead of starting a new scene, lets speed/navigation replay the
	// right idles, and is what the HUD shows. Set when the scene's animation
	// starts, cleared when it stops. Not save-persisted yet (that's the claim
	// registry's job). Still called "player scene" in names Papyrus sees.

	using PlayerScene = FourStim::FocusedScene;

	std::mutex  g_playerSceneLock;
	PlayerScene g_playerScene;

	// Set by SaveStartView (the first step of a scene the player watches with
	// the free camera): the next scene to start becomes the focused scene even
	// without the player in it.
	std::atomic<bool> g_focusNextScene = false;

	PlayerScene GetPlayerScene()
	{
		std::scoped_lock lock(g_playerSceneLock);
		return g_playerScene;
	}

	void SetPlayerScene(PlayerScene a_scene)
	{
		{
			std::scoped_lock lock(g_playerSceneLock);
			g_playerScene = std::move(a_scene);
		}
		HUD::OnFocusedSceneChanged();
	}

	// Clears the focused scene if a_actorID is in it.
	void ClearFocusIfIn(std::uint32_t a_actorID)
	{
		const auto focused = GetPlayerScene();
		const auto ids = focused.ActorIDs();
		if (focused.Active() && std::ranges::find(ids, a_actorID) != ids.end()) {
			SetPlayerScene({});
		}
	}

	// ---- Roles ----
	// Who may take each role of a scene (docs/ACTORS.md), as OStim: the
	// actor's type, what they have for the role's requirements (their actor
	// properties), and, with SetOnlyGayAnimsInGayScenes, the role's sex.

	std::vector<SceneRegistry::ActorCondition> ConditionsOf(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<SceneRegistry::ActorCondition> conditions;
		for (const auto id : a_ids) {
			conditions.push_back(ActorProperties::Cached(id));
		}
		return conditions;
	}

	std::optional<std::size_t> PlayerPin(const std::vector<std::uint32_t>& a_cast);

	// The filter for the picker's lists: these actors, roles open (a new
	// scene, the player's place kept if it's chosen) or already given (a
	// running one).
	SceneRegistry::ListFilter SexFilter(const std::vector<std::uint32_t>& a_ids, bool a_fixedOrder)
	{
		SceneRegistry::ListFilter filter;
		filter.fixedOrder = a_fixedOrder;
		filter.actors = ConditionsOf(a_ids);
		if (!a_fixedOrder) {
			filter.pinned = PlayerPin(a_ids);
		}
		return filter;
	}

	// Whether the actors of a running scene (role order) may play a_scene.
	bool SexesFit(const SceneRegistry::Scene& a_scene, const std::vector<std::uint32_t>& a_ids)
	{
		return SceneRegistry::Fits(a_scene.actors, ConditionsOf(a_ids));
	}

	// ---- Furniture ----
	// A scene with "furniture" plays on a piece of furniture of that type (or
	// a subtype); one without plays anywhere but on furniture.

	bool FurnitureFits(const std::string& a_sceneFurniture, const std::string& a_here);

	// FurnitureFits, plus the furniture's own rules for floor scenes played
	// on it (a mattress only takes lying scenes).
	bool SceneFitsFurniture(const SceneRegistry::Scene& a_scene, const std::string& a_here)
	{
		if (!FurnitureFits(a_scene.furniture, a_here)) {
			return false;
		}
		return !a_scene.furniture.empty() || a_here.empty() || Furniture::AllowsFloorScene(a_here, a_scene);
	}

	bool FurnitureFits(const std::string& a_sceneFurniture, const std::string& a_here)
	{
		if (a_sceneFurniture.empty()) {
			// Off furniture; or on furniture whose type chain reaches "none"
			// (beds, like OStim): scenes without furniture play there too.
			return a_here.empty() || Furniture::IsA(a_here, "none");
		}
		return !a_here.empty() && Furniture::IsA(a_here, a_sceneFurniture);
	}

	// Furniture near the player when the picker opened (new scenes only).
	std::mutex                      g_pickerFurnitureLock;
	std::vector<Furniture::Found>   g_pickerFurniture;

	std::vector<Furniture::Found> PickerFurniture()
	{
		std::scoped_lock lock(g_pickerFurnitureLock);
		return g_pickerFurniture;
	}

	// The nearest found piece a scene for a_type can be played on.
	const Furniture::Found* FurnitureFor(const std::vector<Furniture::Found>& a_found, const std::string& a_type)
	{
		for (const auto& f : a_found) {  // nearest first
			if (Furniture::IsA(f.type, a_type)) {
				return &f;
			}
		}
		return nullptr;
	}

	// Scenes a running scene's actors (in their roles) can move to on
	// furniture of type a_furniture ("" = off furniture).
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForRunning(const std::vector<std::uint32_t>& a_ids, const std::string& a_furniture)
	{
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> out;
		for (const auto& summary : SceneRegistry::List(a_ids.size(), SexFilter(a_ids, true))) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && SceneFitsFurniture(*scene, a_furniture)) {
				out.push_back(scene);
			}
		}
		return out;
	}

	// The scene a scene starts with on a_here ("" = off furniture), as
	// OStim's thread starters and changeFurniture pick it: a random one of
	// a_scenes (no transitions, nothing marked noRandomSelection), tagged
	// "intro" (SetUseIntroScenes) or else "idle" when a_tagged; off furniture
	// with someone standing, on a bed with no one standing. Departure from
	// OStim, which gives up when nothing qualifies: 4Stim falls back step by
	// step (intro -> idle -> any scene, then without the standing rule),
	// since most converted packs have no intro scenes yet. a_why: which step.
	std::shared_ptr<const SceneRegistry::Scene> PickStartScene(std::vector<std::shared_ptr<const SceneRegistry::Scene>> a_scenes, const std::string& a_here,
		bool a_tagged, std::string& a_why)
	{
		std::erase_if(a_scenes, [](const auto& a_scene) { return a_scene->IsTransition() || a_scene->noRandomSelection; });
		const bool onBed = !a_here.empty() && Furniture::IsA(a_here, "bed");
		auto standingRule = [&](const SceneRegistry::Scene& a_scene) {
			const bool standing = std::ranges::any_of(a_scene.actors, [](const SceneRegistry::SceneActor& a) { return a.HasTag("standing"); });
			return a_here.empty() ? standing : onBed ? !standing : true;
		};
		auto tagged = [](const SceneRegistry::Scene& a_scene, const std::string& a_tag) {
			return std::ranges::any_of(a_scene.tags, [&](const std::string& t) { return _stricmp(t.c_str(), a_tag.c_str()) == 0; });
		};
		std::vector<std::string> tags;
		if (a_tagged) {
			if (g_settings.useIntroScenes) {
				tags.emplace_back("intro");
			}
			tags.emplace_back("idle");
		}
		tags.emplace_back("");  // any scene
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
		for (const bool rule : { true, false }) {
			for (const auto& tag : tags) {
				pool.clear();
				std::ranges::copy_if(a_scenes, std::back_inserter(pool), [&](const auto& a_scene) {
					return (tag.empty() || tagged(*a_scene, tag)) && (!rule || standingRule(*a_scene));
				});
				if (!pool.empty()) {
					a_why = std::format("{}{}: {} {} scene(s)", tag.empty() ? std::string("any") : tag, rule ? "" : ", without the standing rule", pool.size(),
						a_here.empty() ? "floor" : a_here);
					static std::mt19937 rng{ std::random_device{}() };
					return pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)];
				}
			}
		}
		return nullptr;
	}

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
	std::mutex                    g_pendingClutterLock;
	ActorSlots<PendingClutter>    g_pendingClutter;

	// Furniture whose activation 4Stim blocked (FourStimFurniture.Block, which
	// leaves furniture blocked by someone else alone): only these are
	// unblocked again.
	std::mutex                        g_blockedFurnitureLock;
	std::unordered_set<std::uint32_t> g_blockedFurniture;

	// OStim's disableUse: no one else can use the furniture a scene is on.
	void BlockFurniture(std::uint32_t a_ref)
	{
		if (!a_ref || !g_vm) {
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimFurniture"sv, "Block"sv, callback, static_cast<std::int32_t>(a_ref));
	}

	// OStim's enableUse, for furniture 4Stim blocked.
	void UnblockFurniture(std::uint32_t a_ref)
	{
		{
			std::scoped_lock lock(g_blockedFurnitureLock);
			if (!a_ref || g_blockedFurniture.erase(a_ref) == 0) {
				return;
			}
		}
		if (g_vm) {
			REX::INFO("Furniture: {:08X} free to use again", a_ref);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimFurniture"sv, "Unblock"sv, callback, static_cast<std::int32_t>(a_ref));
		}
	}

	bool FurnitureBlocked(std::uint32_t a_ref)
	{
		std::scoped_lock lock(g_blockedFurnitureLock);
		return a_ref && g_blockedFurniture.contains(a_ref);
	}

	// Notes the clutter around a_spot for the scene a_actors are about to
	// start (main thread, before they're moved there).
	void NoteClutterFor(std::vector<std::uint32_t> a_actors, const RE::NiPoint3& a_spot)
	{
		if (!g_settings.resetClutter) {
			return;
		}
		std::ranges::sort(a_actors);
		{
			std::scoped_lock lock(g_pendingClutterLock);
			if (const auto noted = g_pendingClutter.Peek(a_actors); noted && noted->fromSave) {
				return;  // a scene from a save: its clutter was noted before the save
			}
		}
		auto items = Furniture::SnapshotClutter(a_spot, g_settings.resetClutterRadius);
		std::scoped_lock lock(g_pendingClutterLock);
		g_pendingClutter.Put(a_actors, PendingClutter{ a_actors, std::move(items), false });
	}

	// Edge furniture (beds): the spot for scenes made for it and the spot
	// for floor scenes played on it (see Furniture::SpotFor), worked out
	// when the actors are placed, for the scene that starts with them.
	struct FurnitureSpots
	{
		std::vector<std::uint32_t> actors;  // sorted
		Furniture::Spot            furniture;
		Furniture::Spot            floor;
	};
	ActorSlots<FurnitureSpots>     g_furnitureSpots;  // main thread
	std::mutex                     g_pendingFurnitureLock;
	ActorSlots<PendingFurniture>   g_pendingFurniture;

	std::optional<PendingFurniture> PendingFurnitureFor(const std::vector<std::uint32_t>& a_actors, bool a_take)
	{
		std::scoped_lock lock(g_pendingFurnitureLock);
		if (a_take) {
			return g_pendingFurniture.Take(a_actors);
		}
		const auto found = g_pendingFurniture.Peek(a_actors);
		return found ? std::optional<PendingFurniture>(*found) : std::nullopt;
	}

	// Furniture for the scene a_actors are about to start.
	void SetPendingFurniture(PendingFurniture a_pending)
	{
		std::scoped_lock lock(g_pendingFurnitureLock);
		const auto actors = a_pending.actors;
		g_pendingFurniture.Put(actors, std::move(a_pending));
	}

	void ClearPendingFurniture(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_pendingFurnitureLock);
		g_pendingFurniture.Erase(a_actors);
	}

	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot);

	bool IsPlayer(const RE::Actor* a_actor);

	// As OStim's ThreadActor positionBefore for the player: where they stood
	// before a scene first moved them onto furniture, so the end can put them
	// back (SetResetPosition). Kept until then; a later move doesn't replace
	// it. Main thread.
	void NotePlayerSpotBefore(RE::Actor* a_actor)
	{
		if (a_actor && IsPlayer(a_actor) && !g_positionBefore.contains(a_actor->GetFormID())) {
			g_positionBefore[a_actor->GetFormID()] = { a_actor->data.location, a_actor->data.angle.z, std::chrono::steady_clock::now() };
			REX::INFO("Furniture: the player's spot before the scene noted, to put them back at its end");
		}
	}

	// Moves a_actors onto the pending furniture's spot, if the next scene for
	// them has one. False if it doesn't.
	bool PlaceOnPendingFurniture(const std::vector<RE::Actor*>& a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		const auto pending = PendingFurnitureFor(ids, false);
		if (!pending) {
			return false;
		}
		const auto ref = pending->ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(pending->ref) : nullptr;
		if (!ref && !pending->fixedSpot) {
			REX::WARN("Furniture: {:08X} is gone, the scene is played where the actors are", pending->ref);
			return false;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto near = player ? player->GetPosition() : ref ? ref->GetPosition() : pending->fixedSpot->position;
		const Furniture::Found piece{ pending->type, {}, pending->ref, 0.0F, pending->fixedSpot };
		auto       spot = Furniture::SpotOf(piece, near, pending->floorScene);
		if (const auto& o = pending->offset; o[0] != 0.0F || o[1] != 0.0F || o[2] != 0.0F || o[3] != 0.0F) {
			// In the spot's frame: x to its right, y forward.
			const float c = std::cos(spot.heading), s = std::sin(spot.heading);
			spot.position.x += o[0] * c + o[1] * s;
			spot.position.y += -o[0] * s + o[1] * c;
			spot.position.z += o[2];
			spot.heading += o[3] * PI_F / 180.0F;
			REX::INFO("Furniture: scene offset ({}, {}, {}, {} deg)", o[0], o[1], o[2], o[3]);
		}
		// On a bed, the scene can later move between bed scenes and floor
		// scenes: keep both spots.
		g_furnitureSpots.Erase(ids);
		if (ref && Furniture::IsEdgeAnchored(pending->type)) {
			const auto other = Furniture::SpotFor(ref, pending->type, near, !pending->floorScene);
			g_furnitureSpots.Put(ids, FurnitureSpots{ ActorSlots<FurnitureSpots>::Key(ids), pending->floorScene ? other : spot, pending->floorScene ? spot : other });
		}
		// The clutter there, before anyone is moved onto it (tasks run in order).
		F4SE::GetTaskInterface()->AddTask([ids, at = spot.position]() { NoteClutterFor(ids, at); });
		for (const auto actor : a_actors) {
			NotePlayerSpotBefore(actor);
			PlaceActorAt(actor, spot);
		}
		return true;
	}

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

	// OStim's Thread::SetSpeed: a speed played outside a transition sets the
	// relative speed (speed / number of speeds, OStim's formula as it is).
	void NoteSpeed(ActiveScene& a_active, const std::string& a_sceneID, int a_speed)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (scene && !scene->IsTransition()) {
			a_active.relativeSpeed = scene->speeds.empty() ? -1.0F : static_cast<float>(a_speed) / static_cast<float>(scene->speeds.size());
		}
	}

	// OStim's Thread::ChangeNode: the speed a scene moved to starts at. One
	// speed: it; no relative speed yet: the scene's defaultSpeed; else the
	// relative speed over its speeds, rounded.
	int MappedSpeed(const ActiveScene& a_active, const SceneRegistry::Scene& a_to)
	{
		const int count = static_cast<int>(a_to.speeds.size());
		if (count <= 1) {
			return 0;
		}
		if (a_active.relativeSpeed < 0.0F) {
			return std::clamp(a_to.defaultSpeed, 0, count - 1);
		}
		return std::clamp(static_cast<int>(a_active.relativeSpeed * static_cast<float>(count - 1) + 0.5F), 0, count - 1);
	}

	// The speed a scene starts at: its defaultSpeed (OStim: a new thread's
	// relative speed is -1, so ChangeNode plays defaultSpeed).
	int StartSpeed(const SceneRegistry::Scene& a_scene)
	{
		return std::clamp(a_scene.defaultSpeed, 0, std::max(static_cast<int>(a_scene.speeds.size()) - 1, 0));
	}
	std::vector<ActiveScene> g_activeScenes;
	// Each scene actor's last visibility summary and when it was checked (LogVisibility).
	std::unordered_map<std::uint32_t, std::pair<std::string, std::chrono::steady_clock::time_point>> g_visibilityLast;

	// Picker IDs of sequences (vs scenes) start with this.
	constexpr std::string_view SEQUENCE_PREFIX = "seq:"sv;

	// A sequence asked for (StartSequence / the picker) that attaches to the
	// next scene started with these actors, if that scene is its first.
	struct PendingSequence
	{
		std::shared_ptr<const SceneRegistry::Sequence> sequence;
		std::vector<std::uint32_t>                     actors;  // any order
	};
	std::mutex                     g_pendingSequenceLock;
	ActorSlots<PendingSequence>    g_pendingSequence;

	// A scene from a loaded save, being started again (see "Save data"):
	// what to restore once it's running.
	struct PendingResume
	{
		std::vector<std::uint32_t> actors;  // sorted
		int                        speed = 0;
		bool                       autoMode = false;
		std::vector<std::pair<std::uint32_t, float>> excitement;
	};
	std::mutex                   g_pendingResumeLock;
	ActorSlots<PendingResume>    g_pendingResume;

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
	std::mutex                                    g_threadsLock;
	std::unordered_map<std::int32_t, ThreadRecord> g_threads;
	std::int32_t                                  g_nextThreadID = 1;  // NPC scenes; the player's is 0

	// A thread that's been asked for but hasn't started in this long has
	// failed somewhere the plugin didn't hear about: dropped.
	constexpr auto STARTING_TIMEOUT = std::chrono::seconds(30);

	// g_threadsLock held.
	void PruneStartingLocked()
	{
		const auto now = std::chrono::steady_clock::now();
		std::erase_if(g_threads, [&](const auto& a_entry) {
			const auto& r = a_entry.second;
			if (r.status == ThreadRecord::Status::kStarting && r.flow == 0 && now - r.since > STARTING_TIMEOUT) {
				REX::WARN("Thread {}: never started (its actors weren't placed within {} s), dropped", r.id, STARTING_TIMEOUT.count());
				return true;
			}
			return false;
		});
	}

	// The thread a_id, if it's starting or running. g_threadsLock held.
	ThreadRecord* ThreadRecordLocked(std::int32_t a_id)
	{
		const auto it = g_threads.find(a_id);
		return it != g_threads.end() ? &it->second : nullptr;
	}

	// Whether a_actor is in a thread that's starting (not running yet).
	bool InStartingThread(std::uint32_t a_actor)
	{
		std::scoped_lock lock(g_threadsLock);
		PruneStartingLocked();
		return std::ranges::any_of(g_threads, [&](const auto& a_entry) {
			return a_entry.second.status == ThreadRecord::Status::kStarting && std::ranges::find(a_entry.second.actors, a_actor) != a_entry.second.actors.end();
		});
	}

	// A start that failed after the thread was handed out: it never runs.
	void DropStartingThread(std::int32_t a_id, std::string_view a_why)
	{
		std::scoped_lock lock(g_threadsLock);
		const auto it = g_threads.find(a_id);
		if (it != g_threads.end() && it->second.status == ThreadRecord::Status::kStarting) {
			REX::WARN("Thread {}: not started ({})", a_id, a_why);
			g_threads.erase(it);
		}
	}

	// The same, by its actors (FourStimMenu.ReleaseFailedStart knows only those).
	void DropStartingThreadOf(std::vector<std::uint32_t> a_actors, std::string_view a_why)
	{
		std::ranges::sort(a_actors);
		std::scoped_lock lock(g_threadsLock);
		std::erase_if(g_threads, [&](const auto& a_entry) {
			if (a_entry.second.status == ThreadRecord::Status::kStarting && a_entry.second.actors == a_actors) {
				REX::WARN("Thread {}: not started ({})", a_entry.first, a_why);
				return true;
			}
			return false;
		});
	}

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
	void EndBrokenScenes(float a_seconds);
	void ForgetActorMeta(const std::vector<std::uint32_t>& a_ids);
	void SetCameraFadeOff(bool a_off);  // the camera's actor fade, off while scenes run
	std::thread::id g_mainThread;  // the game's main thread (set at kGameLoaded)
	void StartAutoMode(ActiveScene& a_active);
	void StopAutoMode(ActiveScene& a_active);
	void AutoModeTick(ActiveScene& a_active, float a_seconds);
	bool IsPlayer(const RE::Actor* a_actor);
	void StartAutoplayTicks();
	void HandleTimedCue(const AnimCues::Due& a_due);  // AnimEventHook::Handle, for AnimCues' cues

	// AnimCues: a_actor started a_idle as a scene's animation (a_sceneID's).
	void CuePlayed(std::uint32_t a_actor, const RE::TESIdleForm* a_idle, const std::string& a_sceneID)
	{
		if (!g_settings.timeAnimCues) {
			return;
		}
		const auto scene = SceneRegistry::Find(a_sceneID);
		AnimCues::Played(a_actor, a_idle, !scene || !scene->IsTransition());
	}
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

	struct SavedBody
	{
		RE::NiPointer<RE::bhkNPCollisionObject> object;
		std::uint32_t                           filter = 0;
		std::uint32_t                           ref = 0;
	};
	std::mutex                                                g_furnitureFiltersLock;
	std::unordered_map<std::uint32_t, std::vector<SavedBody>> g_furnitureFilters;  // scene furniture -> bodies changed for it

	constexpr float FURNITURE_CLEAR_RADIUS = 120.0F;  // other furniture this close to the scene's joins too

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

	void CollectBodies(RE::NiAVObject* a_object, std::vector<RE::bhkNPCollisionObject*>& a_out, int a_depth)
	{
		if (!a_object) {
			return;
		}
		if (const auto object = netimmerse_cast<RE::bhkNPCollisionObject*>(a_object->collisionObject.get())) {
			a_out.push_back(object);
		}
		if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
			for (const auto& child : node->children) {
				CollectBodies(child.get(), a_out, a_depth - 1);
			}
		}
	}

	void IgnoreFurnitureCollision(std::uint32_t a_furniture, std::vector<std::uint32_t> a_actors, const std::string& a_type)
	{
		if (!a_furniture || !g_lockScenes || !g_settings.furnitureOutOfWay || !Compat::CanReadCollisionBodies()) {
			return;  // without Getbhk / GetBody (1.10.163 if the Runtime Database lacks them): left solid
		}
		// Beds stay solid: scenes lie on them, and without the bed under him
		// the owner's player sank through it (drawn inside the bed, his head
		// at the rail) while the NPC, held more firmly, lay on top. Solid,
		// both lay on it (1 November).
		if (!a_type.empty() && Furniture::IsA(a_type, "bed")) {
			REX::INFO("Furniture collision: {:08X} is a bed: it stays solid for the scene's actors", a_furniture);
			return;
		}
		// A task after IgnorePairCollision's (queued earlier): the actors
		// already share their group.
		F4SE::GetTaskInterface()->AddTask([a_furniture, a_actors]() {
			const auto furniture = RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_furniture);
			if (!furniture) {
				return;
			}
			// The furniture next to it first: the cell's references, not the
			// physics world (outside its lock).
			std::vector<RE::TESObjectREFR*> pieces{ furniture };
			Compat::ForEachReferenceInRange(furniture->data.location, FURNITURE_CLEAR_RADIUS, [&](RE::TESObjectREFR* a_ref) {
				const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
				if (a_ref != furniture && base && base->GetFormType() == RE::ENUM_FORM_ID::kFURN && !a_ref->IsDisabled() && !a_ref->IsDeleted() && a_ref->Get3D()) {
					pieces.push_back(a_ref);
				}
				return pieces.size() < 8 ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop;
			});
			const PhysicsLock physics(furniture);
			std::uint32_t     group = 0;
			for (const auto id : a_actors) {
				const auto controller = GetCharController(RE::TESForm::GetFormByID<RE::Actor>(id));
				const auto body = controller ? controller->GetBodyImpl() : nullptr;
				if (body) {
					RE::CFilter filter{};
					filter.filter = body->m_collisionFilterInfo;
					group = filter.GetSystemGroup();
					break;
				}
			}
			if (group == 0) {
				REX::INFO("Furniture collision: the actors on {:08X} have no collision group; left as it is", a_furniture);
				return;
			}
			std::scoped_lock lock(g_furnitureFiltersLock);
			auto&            saved = g_furnitureFilters[a_furniture];
			for (const auto piece : pieces) {
				std::vector<RE::bhkNPCollisionObject*> objects;
				CollectBodies(piece->Get3D(), objects, 16);
				std::size_t changed = 0;
				for (const auto object : objects) {
					const auto body = object->GetBody();
					if (!body) {
						continue;
					}
					const auto own = body->m_collisionFilterInfo;
					if (std::ranges::any_of(saved, [&](const SavedBody& a_saved) { return a_saved.object.get() == object; })) {
						continue;  // already done (a scene change on the same furniture)
					}
					RE::CFilter filter{};
					filter.filter = own;
					filter.SetSystemGroup(group);
					if (object->SetCollisionFilterInfo(filter)) {
						saved.push_back({ RE::NiPointer<RE::bhkNPCollisionObject>(object), own, piece->GetFormID() });
						++changed;
					}
				}
				REX::INFO("Furniture collision: {:08X}{} out of the scene actors' way ({} of {} collision bodies, group {:04X})", piece->GetFormID(),
					piece == furniture ? " (the scene's)" : " (next to it)", changed, objects.size(), group);
			}
		});
	}

	void RestoreFurnitureCollision(std::uint32_t a_furniture)
	{
		if (!a_furniture) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([a_furniture]() {
			std::vector<SavedBody> bodies;
			{
				std::scoped_lock lock(g_furnitureFiltersLock);
				const auto       it = g_furnitureFilters.find(a_furniture);
				if (it == g_furnitureFilters.end()) {
					return;
				}
				bodies = std::move(it->second);
				g_furnitureFilters.erase(it);
			}
			const PhysicsLock physics(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_furniture));
			std::size_t       restored = 0;
			for (const auto& body : bodies) {
				RE::CFilter filter{};
				filter.filter = body.filter;
				if (body.object && body.object->SetCollisionFilterInfo(filter)) {
					++restored;
				}
			}
			REX::INFO("Furniture collision: {:08X} and what was next to it collide with the actors again ({} of {} bodies)", a_furniture, restored, bodies.size());
		});
	}

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
	std::unordered_map<std::uint32_t, std::vector<TouchedBody>>        g_touched;      // actor -> bodies changed for them (main thread)
	std::unordered_map<std::uint32_t, std::unordered_set<const void*>> g_touchLogged;  // actor -> bodies logged this scene

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
	std::unordered_map<std::uint32_t, NearbyBodies> g_nearbyBodies;  // actor -> what's around them

	constexpr float WATCH_RADIUS = 160.0F;  // references this close to the actor are looked through

	// The references around a_actor and their collision objects, looked up
	// again at most every 5 seconds. Outside the physics lock: it walks the
	// cell's references and their 3D, not the physics world.
	void RefreshNearby(RE::Actor* a_actor)
	{
		auto&      nearby = g_nearbyBodies[a_actor->GetFormID()];
		const auto now = std::chrono::steady_clock::now();
		if (!nearby.entries.empty() && now - nearby.built < std::chrono::seconds(5)) {
			return;
		}
		nearby.built = now;
		nearby.entries.clear();
		std::size_t refs = 0;
		Compat::ForEachReferenceInRange(a_actor->data.location, WATCH_RADIUS, [&](RE::TESObjectREFR* a_ref) {
			const auto base = a_ref ? a_ref->GetObjectReference() : nullptr;
			if (!base || a_ref->IsDisabled() || a_ref->IsDeleted() || base->GetFormType() == RE::ENUM_FORM_ID::kNPC_) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			std::vector<RE::bhkNPCollisionObject*> objects;
			CollectBodies(a_ref->Get3D(), objects, 16);
			for (const auto object : objects) {
				nearby.entries.push_back({ RE::NiPointer<RE::bhkNPCollisionObject>(object), a_ref->GetFormID(), base->GetFormID(), static_cast<std::uint8_t>(base->GetFormType()) });
			}
			return ++refs < 96 ? RE::BSContainer::ForEachResult::kContinue : RE::BSContainer::ForEachResult::kStop;
		});
	}

	// Which of those owns a_body (under the physics lock).
	const NearbyBodies::Entry* OwnerOf(RE::Actor* a_actor, const RE::hknpBody* a_body)
	{
		for (const auto& entry : g_nearbyBodies[a_actor->GetFormID()].entries) {
			if (entry.object && entry.object->GetBody() == a_body) {
				return &entry;
			}
		}
		return nullptr;
	}

	// Each scene actor's controller filter before the scene (the shared
	// group, a wall's no collision), restored at scene end
	// (RestoreCollisionNow). Not save-persisted: character controllers are
	// rebuilt on load anyway. Main thread.
	std::unordered_map<std::uint32_t, std::uint32_t> g_savedFilters;

	void WatchCollisions(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		if (!Compat::CanReadCollisionBodies()) {
			return;  // 1.10.163 without Getbhk / GetBody: no watch (compat/Fallbacks.h)
		}
		if (g_lockScenes && g_settings.furnitureOutOfWay) {
			RefreshNearby(a_actor);
		}
		const PhysicsLock physics(a_actor);
		const auto        controller = GetCharController(a_actor);
		const auto        own = controller ? controller->GetBodyImpl() : nullptr;
		if (!own) {
			return;
		}
		// Where the controller and the drawn body are, against where the
		// actor is (the lock's spot): a shake that the lock watchdog never
		// saw (no "Scene lock:" in the owner's log) is one of these. At most
		// once a second, ten times a scene.
		if (g_settings.collisionWatch) {
			struct Seen
			{
				std::chrono::steady_clock::time_point last{};
				int                                   count = 0;
			};
			static std::unordered_map<std::uint32_t, Seen> seen;
			auto&                                          entry = seen[a_actor->GetFormID()];
			if (!g_touchLogged.contains(a_actor->GetFormID())) {
				entry.count = 0;  // a new scene (TrackSceneStart forgets the actor's entry)
				g_touchLogged[a_actor->GetFormID()];
			}
			const auto now = std::chrono::steady_clock::now();
			if (entry.count < 20 && now - entry.last > std::chrono::milliseconds(200)) {
				RE::hkVector4f at{};
				controller->GetPositionImpl(at, false);
				constexpr float HAVOK_TO_GAME = 69.99125F;  // Fallout 4's Havok scale (1 / 0.0142875)
				const RE::NiPoint3 controllerAt{ at[0] * HAVOK_TO_GAME, at[1] * HAVOK_TO_GAME, at[2] * HAVOK_TO_GAME };
				const auto         root = a_actor->Get3D();
				const auto         drawnAt = root ? root->world.translate : a_actor->data.location;
				// The controller's position is its capsule's middle, about half
				// the actor's height up (64-71 units in a tester's log): only
				// across the ground counts.
				const auto         dx = controllerAt.x - a_actor->data.location.x;
				const auto         dy = controllerAt.y - a_actor->data.location.y;
				const float        offController = std::sqrt(dx * dx + dy * dy);
				const float        offDrawn = drawnAt.GetDistance(a_actor->data.location);
				// Every check while it lasts (20 a scene), not only when off: the
				// owner's chair shake (4 November) bounced the player a few
				// pixels up and down with nothing past 4 units here. With what
				// the controller stands on and how much it collides with.
				(void)offController;
				(void)offDrawn;
				{
					entry.last = now;
					++entry.count;
					const auto support = controller->supportBody.get();
					const auto supportNode = support ? support->GetSceneGraphObject() : nullptr;
					const auto supportRef = supportNode ? RE::TESObjectREFR::FindReferenceFor3D(supportNode) : nullptr;
					REX::INFO("Collision watch: {:08X} in \"{}\" at ({:.1f}, {:.1f}, {:.1f}); controller at ({:.1f}, {:.1f}, {:.1f}) ({:.1f} off across the ground), body drawn at ({:.1f}, {:.1f}, {:.1f}) ({:.1f} off); stands on {:08X} (layer {}), {} collision(s), state {}, fall time {:.2f}",
						a_actor->GetFormID(), a_sceneID, a_actor->data.location.x, a_actor->data.location.y, a_actor->data.location.z, controllerAt.x, controllerAt.y,
						controllerAt.z, offController, drawnAt.x, drawnAt.y, drawnAt.z, offDrawn, supportRef ? supportRef->GetFormID() : 0,
						std::to_underlying(controller->supportLayer.get()), controller->numCollisions, std::to_underlying(controller->wantState.get()), controller->fallTime);
				}
			}
		}
		if (!g_lockScenes || !g_settings.furnitureOutOfWay) {
			return;
		}
		RE::CFilter ownFilter{};
		ownFilter.filter = own->m_collisionFilterInfo;
		const auto                            group = ownFilter.GetSystemGroup();
		RE::BSScrapArray<const RE::hknpBody*> bodies;
		if (group == 0 || !controller->GetCurrentCollisionsImpl(bodies)) {
			return;
		}
		const auto id = a_actor->GetFormID();
		const auto support = controller->supportBody.get();
		const auto supportBody = support ? support->GetBody() : nullptr;
		for (const auto body : bodies) {
			if (!body || body == own || body == supportBody) {
				continue;
			}
			RE::CFilter filter{};
			filter.filter = body->m_collisionFilterInfo;
			const auto layer = filter.GetCollisionLayer();
			if (layer == RE::COL_LAYER::kCharController || layer == RE::COL_LAYER::kBiped || layer == RE::COL_LAYER::kTerrain || layer == RE::COL_LAYER::kGround) {
				continue;  // actors (the scene's share a group already), and the ground
			}
			const auto owner = OwnerOf(a_actor, body);
			if (!owner) {
				const bool first = g_touchLogged[id].insert(body).second;
				if (!g_settings.passThroughWalls) {
					if (first) {
						REX::INFO("Collision watch: {:08X} in \"{}\" touches a body no reference near them owns (layer {}, filter {:08X}; a precombined wall?): left as it is",
							id, a_sceneID, std::to_underlying(layer), filter.filter);
					}
					continue;
				}
				// A wall (precombined: no reference, so it can't join the
				// group). Wall scenes' spot is 15 units out from it, inside the
				// actors' capsules, so the wall pushed them about 10 units off
				// it and the lock put them back, every frame: the owner's wall
				// shake (6 November; with bTranslationLock=0 they sat 10-11
				// units off the spot instead). The actor's own controller then
				// collides with nothing for the scene: their gravity is 0 and
				// the lock holds them, so they don't need the floor. Back with
				// their filter (RestoreCollisionNow). Not in OStim (Skyrim's
				// lock leaves the capsule out of it).
				if (!ownFilter.QNoCollision()) {
					g_savedFilters.try_emplace(id, own->m_collisionFilterInfo);
					RE::CFilter none = ownFilter;
					none.SetNoCollision(true);
					const auto ok = controller->SetCollisionFilterInfo(none);
					REX::INFO("Collision watch: {:08X} in \"{}\" touches a body no reference near them owns (layer {}, filter {:08X}; a wall): {:08X} collides with nothing this scene ({})",
						id, a_sceneID, std::to_underlying(layer), filter.filter, id, ok);
					if (ok) {
						return;
					}
				}
				continue;
			}
			const auto object = owner->object.get();
			if (g_touchLogged[id].insert(object).second) {
				REX::INFO("Collision watch: {:08X} in \"{}\" touches {:08X} (base {:08X}, type {}, layer {}, filter {:08X})", id, a_sceneID, owner->ref, owner->base,
					owner->type, std::to_underlying(layer), filter.filter);
			}
			// Furniture already out of the way and still touched: the group
			// isn't enough there; no collision at all (its own filter is the
			// furniture's saved one).
			{
				std::scoped_lock lock(g_furnitureFiltersLock);
				bool             furniture = false;
				for (const auto& [ref, saved] : g_furnitureFilters) {
					if (std::ranges::any_of(saved, [&](const SavedBody& a_saved) { return a_saved.object.get() == object; })) {
						furniture = true;
						if (!filter.QNoCollision()) {
							filter.SetNoCollision(true);
							const auto ok = object->SetCollisionFilterInfo(filter);
							REX::WARN("Collision watch: {:08X} still touches furniture {:08X} in its collision group: no collision for it this scene ({})", id,
								owner->ref, ok);
						}
						break;
					}
				}
				if (furniture) {
					continue;
				}
			}
			auto&      list = g_touched[id];
			const auto it = std::ranges::find_if(list, [&](const TouchedBody& a_touched) { return a_touched.object.get() == object; });
			if (it == list.end()) {
				RE::CFilter changed = filter;
				changed.SetSystemGroup(group);
				if (object->SetCollisionFilterInfo(changed)) {
					list.push_back({ owner->object, filter.filter, false });
					REX::INFO("Collision watch: {:08X} ({:08X}) put in {:08X}'s collision group for the scene", owner->ref, filter.filter, id);
				}
			} else if (!it->noCollision) {
				filter.SetNoCollision(true);
				it->noCollision = true;
				const auto ok = object->SetCollisionFilterInfo(filter);
				REX::WARN("Collision watch: {:08X} still touches {:08X} in its collision group: no collision for it this scene ({})", id, owner->ref, ok);
			}
		}
	}

	// ---- No gravity in scenes ----
	// Not in OStim (Skyrim's lock holds actors in the air without help).
	// The lock's translation puts the actor back on their spot every frame,
	// but the player isn't restrained (SetRestrained also freezes the look
	// input the free camera needs), so their character controller still
	// falls under gravity between frames wherever the spot is above the
	// floor: a chair scene's spot, with its alignment, holds them some units
	// up (the owner's log, 5 November: both held at 1509.1 over a floor at
	// about 1504, the player standing on nothing). Fall, put back, fall: the
	// player's whole body bouncing about six times a second, in chair scenes
	// only and whichever role he had, while the restrained NPC stayed still.
	// So scene actors' controllers have no gravity while the lock holds
	// them (the guard keeps it at 0), and get their own back with their
	// collision (RestoreCollisionNow).
	std::unordered_map<std::uint32_t, float> g_savedGravity;  // actor -> their controller's gravity before the scene (main thread)

	void HoldGravity(RE::Actor* a_actor)
	{
		const auto controller = g_lockScenes ? GetCharController(a_actor) : nullptr;
		if (!controller || controller->gravity == 0.0F) {
			return;
		}
		const PhysicsLock physics(a_actor);
		const auto        id = a_actor->GetFormID();
		if (g_savedGravity.try_emplace(id, controller->gravity).second) {
			REX::INFO("Scene lock: {:08X}'s controller gravity {:.2f} -> 0 for the scene", id, controller->gravity);
		}
		controller->gravity = 0.0F;
		controller->fallTime = 0.0F;
	}

	void RestoreGravity(RE::Actor* a_actor)
	{
		const auto it = g_savedGravity.find(a_actor->GetFormID());
		if (it == g_savedGravity.end()) {
			return;
		}
		const PhysicsLock physics(a_actor);
		if (const auto controller = GetCharController(a_actor)) {
			controller->gravity = it->second;
			REX::INFO("Scene lock: {:08X}'s controller gravity back to {:.2f}", a_actor->GetFormID(), it->second);
		}
		g_savedGravity.erase(it);
	}

	// The actor's collision back (RestoreCollisionNow): what the watch
	// changed for them, back as it was. Main thread.
	void RestoreTouched(std::uint32_t a_actor)
	{
		g_touchLogged.erase(a_actor);
		g_nearbyBodies.erase(a_actor);
		const auto it = g_touched.find(a_actor);
		if (it == g_touched.end()) {
			return;
		}
		const PhysicsLock physics(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_actor));
		std::size_t       restored = 0;
		for (const auto& touched : it->second) {
			RE::CFilter filter{};
			filter.filter = touched.filter;
			if (touched.object && touched.object->SetCollisionFilterInfo(filter)) {
				++restored;
			}
		}
		REX::INFO("Collision watch: {} of {} thing(s) {:08X} touched collide with them again", restored, it->second.size(), a_actor);
		g_touched.erase(it);
	}

	// A scene leaves its furniture (it ends, or moves to other furniture):
	// the furniture free to use again and, with SetResetClutter, the loose
	// items around it back where they lay (as OStim's changeFurniture and
	// thread end). Main thread.
	void FreeFurniture(ActiveScene& a_scene)
	{
		RestoreFurnitureCollision(a_scene.furnitureRef);
		UnblockFurniture(a_scene.furnitureRef);
		if (!a_scene.clutter.empty()) {
			if (g_settings.resetClutter) {
				Furniture::RestoreClutter(a_scene.clutter);
			}
			a_scene.clutter.clear();
		}
	}

	std::vector<Furniture::Found> FurnitureNear(const RE::NiPoint3& a_center, RE::TESObjectREFR* a_from)
	{
		auto found = Furniture::FindNear(a_center, g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.logFurniture, a_from);
		std::erase_if(found, [](const Furniture::Found& f) {
			return f.ref && std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a_scene) { return a_scene.furnitureRef == f.ref; });
		});
		for (const auto& f : found) {
			REX::INFO("Furniture: nearest {}: {:08X}, {:.0f} away", f.type, f.ref, f.distance);
		}
		return found;
	}

	// The scales furniture a_ref of type a_type gives a scene on it.
	void SetFurnitureScales(ActiveScene& a_scene, std::uint32_t a_ref, const std::string& a_type)
	{
		a_scene.furnitureScaleMult = a_type.empty() ? 1.0F : Furniture::MultiplyScale(a_type);
		const auto ref = a_ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_ref) : nullptr;
		a_scene.furnitureRefScale = ref && ref->refScale > 0 ? ref->refScale / 100.0F : 1.0F;
	}

	ActiveScene* FindActiveScene(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		return it != g_activeScenes.end() ? &*it : nullptr;
	}

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
	std::vector<EndedThread> g_endedThreads;
	// How long the end event waits for Papyrus's release (EndPairScene /
	// EndGroupScene take 1.5 s) before it's sent anyway.
	constexpr auto RELEASE_WAIT = std::chrono::seconds(4);

	// Sends the end event for the ended threads with any of a_actors (their
	// release done, or a_late: it never came), and forgets their actors'
	// metadata (OStim's ends with the thread). Main thread.
	void SendEndedFor(const std::vector<std::uint32_t>& a_actors, bool a_late)
	{
		std::vector<EndedThread> due;
		std::erase_if(g_endedThreads, [&](EndedThread& a_ended) {
			const bool mine = std::ranges::any_of(a_ended.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
			if (mine) {
				due.push_back(std::move(a_ended));
			}
			return mine;
		});
		for (auto& ended : due) {
			{
				std::scoped_lock lock(g_threadsLock);
				if (const auto it = g_threads.find(ended.threadID); it != g_threads.end() && it->second.status == ThreadRecord::Status::kEnded) {
					g_threads.erase(it);
				}
			}
			if (a_late) {
				REX::INFO("Thread {}: end event sent without hearing the actors were released", ended.threadID);
			}
			// Actors already in a new scene keep that scene's metadata.
			std::vector<std::uint32_t> free;
			std::ranges::copy_if(ended.actors, std::back_inserter(free), [](std::uint32_t id) { return !FindActiveScene(id); });
			ForgetActorMeta(free);
			SceneEvents::SceneEnded(ended.actors, ended.sceneID, ended.threadID, ended.metadata);
		}
	}

	void TrackSceneStart(std::vector<std::uint32_t> a_actors, const std::string& a_sceneID, int a_speed)
	{
		PenisBend::NoteRestPose(a_actors);  // before the scene's animation poses them
		for (const auto id : a_actors) {
			g_visibilityLast.erase(id);  // the visibility check logs them afresh
			g_touchLogged.erase(id);     // and the collision watch
			g_nearbyBodies.erase(id);
			if (!IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id))) {
				ScenePackage::Add(id);  // scenes resumed from a save, too (SuppressInteraction adds the rest earlier)
			}
		}
		// An actor can only be in one scene: a new scene replaces an old one
		// (which leaves its furniture first).
		for (auto& old : g_activeScenes) {
			if (std::ranges::any_of(old.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); })) {
				FreeFurniture(old);
			}
		}
		std::erase_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			const bool replaced = std::ranges::any_of(a_scene.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
			if (replaced) {
				std::vector<std::uint32_t> left;
				std::ranges::copy_if(a_scene.actors, std::back_inserter(left), [&](std::uint32_t id) { return std::ranges::find(a_actors, id) == a_actors.end(); });
				Excitement::Leave(left);
				AnimCues::Stop(left);
				Expressions::Leave(left);
				Sounds::Leave(left);
				Peaks::Leave(left);
				SexToys::Leave(left);
				PlaybackSpeed::Set(left, 1.0F);
				Lights::Leave(left);
				ApiForms::SceneEnded(left);
				EquipObjects::Leave(left);
				PenisBend::Leave(left);
				PenisBend::RestoreRestPose(left);
				Erection::Leave(left);
				EyeLook::Leave(left);
				UnlockActors(left);
				RestoreScales(a_scene);
				for (const auto id : left) {
					Undress::Redress(id, false, true);
				}
				// Its thread is over (replaced, not ended: no end event, as before).
				std::scoped_lock lock(g_threadsLock);
				g_threads.erase(a_scene.threadID);
			}
			return replaced;
		});
		ActiveScene scene{ a_actors, a_sceneID };
		scene.speed = a_speed;
		NoteSpeed(scene, a_sceneID, a_speed);
		const bool hasPlayer = std::ranges::any_of(a_actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		// Its thread: the one a mod asked for with these actors (thread
		// builder), else a new one (the picker, a save's scene...).
		std::optional<ThreadRecord> start;  // a copy of what was asked for
		{
			const auto sorted = ActorSlots<int>::Key(a_actors);
			std::scoped_lock lock(g_threadsLock);
			PruneStartingLocked();
			auto it = std::ranges::find_if(g_threads, [&](const auto& a_entry) {
				return a_entry.second.status == ThreadRecord::Status::kStarting && a_entry.second.actors == sorted;
			});
			if (it == g_threads.end()) {
				ThreadRecord record;
				record.id = hasPlayer ? 0 : g_nextThreadID++;
				record.actors = sorted;
				it = g_threads.insert_or_assign(record.id, std::move(record)).first;
			} else {
				start = it->second;
			}
			it->second.status = ThreadRecord::Status::kRunning;
			it->second.roleActors = a_actors;
			scene.threadID = it->first;
			if (start) {
				// What was asked for while it was starting is done below; not twice.
				it->second.stopWhenStarted = false;
				it->second.stallWhenStarted.reset();
				it->second.autoWhenStarted.reset();
				it->second.speedWhenStarted.reset();
				it->second.sceneWhenStarted.clear();
			}
		}
		if (start) {
			scene.noAutoMode = start->noAutoMode;
			scene.noPlayerControl = start->noPlayerControl;
			scene.noUndress = start->noUndress;
			scene.noPostDialogue = start->noPostDialogue;
			scene.endAfterSequence = start->endAfterSequence;
			for (const auto id : a_actors) {
				Undress::Block(id, start->noUndress);
			}
		}
		if (const auto furniture = PendingFurnitureFor(a_actors, true)) {
			scene.furnitureRef = furniture->ref;
			scene.furnitureType = furniture->type;
			scene.furnitureFixedSpot = furniture->fixedSpot;
			SetFurnitureScales(scene, furniture->ref, furniture->type);
			{
				std::scoped_lock lock(g_pendingClutterLock);
				if (auto clutter = g_pendingClutter.Take(a_actors)) {
					scene.clutter = std::move(clutter->items);
				}
			}
			BlockFurniture(furniture->ref);
			IgnoreFurnitureCollision(furniture->ref, a_actors, furniture->type);
			scene.furnitureSpots = g_furnitureSpots.Take(a_actors);
			REX::INFO("Scene \"{}\": on furniture {:08X} ({})", a_sceneID, furniture->ref, furniture->type);
		}
		{
			std::scoped_lock lock(g_pendingSequenceLock);
			if (const auto pending = g_pendingSequence.Take(a_actors)) {
				const auto& first = pending->sequence->entries.front();
				if (_stricmp(first.scene.c_str(), a_sceneID.c_str()) == 0) {
					scene.sequence = pending->sequence;
					REX::INFO("Sequence \"{}\": started ({} scene(s))", scene.sequence->id, scene.sequence->entries.size());
				}
			}
		}
		// The spot it's played on: where role 0 was placed (everyone was
		// placed on the same spot, or the player's / first actor's own).
		if (const auto first = RE::TESForm::GetFormByID<RE::Actor>(a_actors.front())) {
			scene.center = first->data.location;
			scene.heading = first->data.angle.z;
		}
		// Better: the spot they were just placed on, if they were (the
		// player isn't moved: then the player's own spot and facing).
		const auto now = std::chrono::steady_clock::now();
		bool       fromPlacement = false;
		for (const auto id : a_actors) {
			const auto it = g_placedSpots.find(id);
			if (it != g_placedSpots.end() && now - it->second.when < std::chrono::seconds(15)) {
				if (!fromPlacement) {
					scene.center = it->second.position;
					scene.heading = it->second.heading;
					fromPlacement = true;
				}
			}
			if (it != g_placedSpots.end()) {
				g_placedSpots.erase(it);
			}
		}
		// The player is moved only onto furniture (the picker's, placed
		// above): otherwise the scene is built on their own spot. Not after
		// a placement: the game had pushed the player up out of the chair's
		// collision by then (9 units in the owner's log, 5 November, before
		// the chair was out of the way), and his spot held both actors in the
		// air over the chair for the whole scene, the player bouncing.
		for (const auto id : a_actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (IsPlayer(actor) && !fromPlacement) {
				scene.center = actor->data.location;  // the player is never moved: the scene is built on their spot
				scene.heading = actor->data.angle.z;
			}
		}
		for (const auto id : a_actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			scene.baseScale.push_back(actor && actor->refScale ? actor->refScale / 100.0F : 1.0F);
			scene.appliedScale.push_back(0.0F);
		}
		// LockScene (below) reads heels and builds the alignment key.
		g_activeScenes.push_back(std::move(scene));
		LockScene(g_activeScenes.back());
		SceneEvents::SceneStarted(a_actors, a_sceneID, g_activeScenes.back().threadID, g_activeScenes.back().speed);
		Erection::SceneStarted(a_actors);  // after the physics swaps (SceneEvents): a swapped body's erection is its swap
		WatchAnimEvents(a_actors, a_sceneID);
		auto& added = g_activeScenes.back();
		RefreshExcitement(added);
		if (const auto first = SceneRegistry::Find(added.sceneID)) {
			ApiForms::SceneStarted(added.actors, *first, hasPlayer, added.furnitureType);  // OStim's API factions (4Stim.esl)
		}
		if (const auto first = SceneRegistry::Find(added.sceneID)) {
			Undress::SceneEntered(added.actors, *first, true);
			EquipObjects::SceneEntered(added.actors, *first, true);
		}
		if (added.sequence && added.sequence->entries.front().speed != 0) {
			PlayOnActiveScene(added, added.sequence->entries.front().scene, added.sequence->entries.front().speed);
		}
		ArmAutoplay(added);
		const bool withPlayer = hasPlayer;
		if (!withPlayer && g_settings.npcSceneDuration > 0.0F) {
			added.stopTimer = g_settings.npcSceneDuration;
			StartAutoplayTicks();
		}
		if (start && start->duration > 0.0F) {
			added.stopTimer = start->duration;  // OStim's SetDuration, the player's scene too
			StartAutoplayTicks();
		}
		if (start && start->undressAtStart) {
			for (const auto id : added.actors) {
				Undress::StripAll(id, true);
			}
		}
		// OStim's Thread::evaluateAutoMode: NPC scenes always (4Stim:
		// bAutoModeNPC), the player's with SetAIControl, else solo ones with
		// SetForceAIForMasturbation and the rest with
		// SetForceAIInConsensualScenes. NoAutoMode keeps it off (StartAutoMode).
		if (!withPlayer ? g_autoConfig.npc :
		    g_autoConfig.player || (added.actors.size() == 1 ? g_autoConfig.playerSolo : g_autoConfig.playerPartner)) {
			StartAutoMode(added);
		}
		// Started again from a save: back to where it was.
		std::optional<PendingResume> resume;
		{
			std::scoped_lock lock(g_pendingResumeLock);
			resume = g_pendingResume.Take(added.actors);
		}
		if (resume) {
			for (const auto& [id, value] : resume->excitement) {
				Excitement::Set(id, value);
			}
			if (resume->autoMode != added.autoMode.on) {
				resume->autoMode ? StartAutoMode(added) : StopAutoMode(added);
			}
			if (resume->speed > 0) {
				PlayOnActiveScene(added, added.sceneID, resume->speed);
			}
			REX::INFO("Save data: \"{}\" running again (speed {}, auto mode {})", added.sceneID, resume->speed + 1, resume->autoMode);
		}
		// What a mod asked of the thread while it was starting.
		if (start) {
			if (start->stallWhenStarted) {
				added.stallClimax = *start->stallWhenStarted;
				for (const auto id : added.actors) {
					Excitement::SetThreadStalled(id, added.stallClimax);
				}
			}
			if (start->autoWhenStarted && *start->autoWhenStarted != added.autoMode.on) {
				*start->autoWhenStarted ? StartAutoMode(added) : StopAutoMode(added);
			}
			if (!start->sceneWhenStarted.empty() || start->speedWhenStarted) {
				const auto to = SceneRegistry::Find(start->sceneWhenStarted.empty() ? added.sceneID : start->sceneWhenStarted);
				if (to && to->actors.size() == added.actors.size()) {
					const int speed = std::clamp(start->speedWhenStarted.value_or(added.speed), 0, std::max(static_cast<int>(to->speeds.size()) - 1, 0));
					if (to->id != added.sceneID) {
						added.sequence.reset();
					}
					if (PlayOnActiveScene(added, to->id, speed)) {
						ArmAutoplay(added);
					}
				}
			}
			if (start->stopWhenStarted) {
				EndActiveScene(added, "asked by another mod while it was starting (FourThread.Stop)");
			}
		}
	}

	void TrackSceneStop(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		if (it == g_activeScenes.end()) {
			return;
		}
		auto ended = std::move(*it);
		g_activeScenes.erase(it);
		if (g_activeScenes.empty()) {
			SetCameraFadeOff(false);
		}
		for (const auto id : ended.actors) {
			Undress::Block(id, false);
		}
		FreeFurniture(ended);
		UnlockActors(ended.actors);
		MoveApart(ended);
		RestoreScales(ended);
		Excitement::Leave(ended.actors);
		AnimCues::Stop(ended.actors);
		Expressions::Leave(ended.actors);
		Sounds::Leave(ended.actors, !ended.noPostDialogue);  // OStim's post-scene dialogue, unless NoPostDialogue
		Peaks::Leave(ended.actors);
		SexToys::Leave(ended.actors);
		PlaybackSpeed::Set(ended.actors, 1.0F);
		Lights::Leave(ended.actors);
		EquipObjects::Leave(ended.actors);
		PenisBend::Leave(ended.actors);
		PenisBend::RestoreRestPose(ended.actors);
		Erection::Leave(ended.actors);  // before dressing: a bodysuit comes off first
		Actor3D::Leave(ended.actors);
		EyeLook::Leave(ended.actors);
		for (const auto id : ended.actors) {
			Undress::Redress(id, false, true);
		}
		SceneEvents::SceneStopped(ended.actors);
		ApiForms::SceneEnded(ended.actors);  // OStim's Core::free: out of the scene's factions
		// The thread ends; its end event waits until Papyrus has let its
		// actors go (SceneReleased, FourStimScene's EndPairScene /
		// EndGroupScene), or RELEASE_WAIT if that never comes. Until then
		// FourThread.GetMetadata and the actors' metadata still answer, as
		// OStim's do inside its end event.
		EndedThread done{ ended.threadID, ended.sceneID, ended.actors, {} };
		{
			std::scoped_lock lock(g_threadsLock);
			if (const auto record = g_threads.find(ended.threadID); record != g_threads.end() && record->second.status == ThreadRecord::Status::kRunning) {
				done.metadata = record->second.metadata;
				record->second.status = ThreadRecord::Status::kEnded;
			}
		}
		g_endedThreads.push_back(std::move(done));
		Scheduler::AfterOnMain(RELEASE_WAIT, [actors = ended.actors]() { SendEndedFor(actors, true); });
	}

	// As OStim's clearNodeQueue: a sequence interrupted (navigation, another
	// mod moving the scene) stops, and with EndAfterSequence, the scene ends
	// too. True if the scene is ending. Auto mode's routes just stop.
	bool InterruptSequence(ActiveScene& a_active, std::string_view a_why)
	{
		if (!a_active.sequence) {
			return false;
		}
		const auto sequence = std::exchange(a_active.sequence, nullptr);
		REX::INFO("Sequence \"{}\": stopped, {}", sequence->id, a_why);
		HUD::OnFocusedSceneChanged();
		if (a_active.endAfterSequence && !sequence->route) {
			EndActiveScene(a_active, "its sequence was interrupted (EndAfterSequence)");
			a_active.remaining = -1.0F;
			return true;
		}
		return false;
	}

	// A scene moved on by someone else (navigation, a speed key). Leaving
	// the scene a sequence is on ends the sequence: the player took over.
	void TrackSceneChange(std::uint32_t a_actorID, const std::string& a_sceneID, int a_speed)
	{
		const auto scene = FindActiveScene(a_actorID);
		if (!scene) {
			return;
		}
		const bool moved = _stricmp(scene->sceneID.c_str(), a_sceneID.c_str()) != 0;
		NoteSpeed(*scene, a_sceneID, a_speed);
		if (moved) {
			scene->queuedScene.clear();
			WatchAnimEvents(scene->actors, a_sceneID);
		}
		scene->sceneID = a_sceneID;
		scene->speed = a_speed;
		scene->lastPlayed = std::chrono::steady_clock::now();
		scene->checkedAfterPlay = false;
		RefreshExcitement(*scene);
		if (const auto entered = moved ? SceneRegistry::Find(a_sceneID) : nullptr) {
			Undress::SceneEntered(scene->actors, *entered, false);
			EquipObjects::SceneEntered(scene->actors, *entered, false);
			ApiForms::SceneChanged(scene->actors, *entered, scene->threadID == 0);
			for (const auto id : scene->actors) {
				PenisBend::Offset(id, 0);  // OStim's changeNode: the annotation's bend ends with the scene
				PenisBend::Aim(id, false);  // until the new animation's 4StimPenisAim says so
			}
			LockScene(*scene);  // this scene's alignment
		}
		if (moved) {
			if (InterruptSequence(*scene, std::format("the scene was moved to \"{}\"", a_sceneID))) {
				return;
			}
			ArmAutoplay(*scene);
			if (scene->autoMode.on) {
				// Picked by the player: auto mode stays a while before moving on.
				scene->autoMode.cooldown = std::max(scene->autoMode.cooldown, g_autoConfig.sceneMin);
			}
		}
	}

	// ---- Autoplay: transitions and sequences ----
	// A transition scene (one with a "destination") plays for its "length"
	// and then moves on to its destination; a sequence plays its scenes for
	// their durations, one after another. Time is counted in frames the game
	// isn't paused, so a pause menu doesn't skip ahead.
	//
	// The clock: a background thread queues one AutoplayTick task every
	// ~10 ms while anything is counting down. A task can't simply re-queue
	// itself: the game runs tasks added during its task pass in that same
	// pass, so that loops without the frame ever advancing (the game froze
	// for the whole transition, and the destination idle was lost).

	std::atomic<bool>                     g_autoplayTicking = false;   // the ticker thread is wanted
	std::atomic<bool>                     g_autoplayTickQueued = false;  // a tick task is waiting to run
	std::chrono::steady_clock::time_point g_autoplayLast;

	// Sets how long a_scene stays where it is before it moves on.
	void ArmAutoplay(ActiveScene& a_scene)
	{
		a_scene.remaining = -1.0F;
		if (a_scene.sequence) {
			const auto& entry = a_scene.sequence->entries[a_scene.step];
			a_scene.remaining = entry.duration;
		} else if (const auto scene = SceneRegistry::Find(a_scene.sceneID); scene && scene->IsTransition()) {
			// At the motion's end by default: the clip holds its last pose
			// past that, and the game starts blending a one-shot clip back to
			// the base pose ~0.35 s before the clip ends ("IdleStop", see
			// AnimEventHook, which moves on then if the timer hasn't).
			a_scene.remaining = g_settings.transitionLead >= 0.0F ?
			                        std::max(scene->length - g_settings.transitionLead, scene->length * 0.5F) :
			                        scene->length - g_settings.transitionLead;
		}
		if (a_scene.remaining >= 0.0F) {
			StartAutoplayTicks();
		}
	}

	// Plays a_sceneID at a_speed on a running scene's actors (all roles on
	// this frame), and records it: the running-scene list, the focused scene
	// if it's this one, and a SceneChanged event.
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed)
	{
		if (a_active.ending) {
			REX::INFO("Autoplay: \"{}\" not played, \"{}\" is ending", a_sceneID, a_active.sceneID);
			return false;
		}
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() != a_active.actors.size()) {
			REX::WARN("Autoplay: can't play \"{}\" on this scene's {} actor(s)", a_sceneID, a_active.actors.size());
			return false;
		}
		// a_speed -1: OStim's ChangeNode speed (MappedSpeed).
		const int  speed = a_speed < 0 ? MappedSpeed(a_active, *scene) : std::clamp(a_speed, 0, static_cast<int>(scene->speeds.size()) - 1);
		const auto& idles = scene->speeds[speed];
		std::string results;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role]);
			const auto process = actor ? actor->currentProcess : nullptr;
			const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
			if (ok) {
				CuePlayed(a_active.actors[role], idles[role], scene->id);
			}
			results += std::format("{}{:08X} {}", role ? ", " : "", a_active.actors[role], ok ? "ok" : "refused");
		}
		REX::INFO("Autoplay: \"{}\" speed {}: PlayIdle {}", scene->id, speed + 1, results);
		WatchAnimEvents(a_active.actors, scene->id);
		const auto previous = a_active.sceneID;
		const int  previousSpeed = a_active.speed;
		NoteSpeed(a_active, scene->id, speed);
		a_active.sceneID = scene->id;
		a_active.speed = speed;
		a_active.lastPlayed = std::chrono::steady_clock::now();
		a_active.checkedAfterPlay = false;
		RefreshExcitement(a_active);
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			Undress::SceneEntered(a_active.actors, *scene, false);
			EquipObjects::SceneEntered(a_active.actors, *scene, false);
			for (const auto id : a_active.actors) {
				PenisBend::Offset(id, 0);  // OStim's changeNode: the annotation's bend ends with the scene
				PenisBend::Aim(id, false);  // until the new animation's 4StimPenisAim says so
			}
			LockScene(a_active);  // this scene's alignment
		}

		auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_active.actors[0]) {
			focused.sceneID = scene->id;
			focused.speed = speed;
			SetPlayerScene(focused);
		}
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			SceneEvents::SceneChanged(a_active.actors, previous, scene->id, a_active.threadID, speed);
		} else if (speed != previousSpeed) {
			// Every speed change, whoever made it (auto mode, a climax,
			// FourThread.SetSpeed), as OStim's Thread::SetSpeed sends its event.
			SceneEvents::SpeedChanged(a_active.actors, scene->id, speed, static_cast<int>(scene->speeds.size()), a_active.threadID);
		}
		return true;
	}

	// a_active's time is up: on to the next scene.
	void AdvanceAutoplay(ActiveScene& a_active)
	{
		if (!a_active.queuedScene.empty()) {
			const auto queued = std::exchange(a_active.queuedScene, std::string{});
			if (InterruptSequence(a_active, std::format("the player picked \"{}\"", queued))) {
				return;
			}
			REX::INFO("Transition \"{}\" -> \"{}\" (picked during the transition)", a_active.sceneID, queued);
			if (PlayOnActiveScene(a_active, queued, -1)) {
				ArmAutoplay(a_active);
			} else {
				a_active.remaining = -1.0F;
			}
			return;
		}
		if (a_active.sequence) {
			const auto sequence = a_active.sequence;
			if (a_active.step + 1 < sequence->entries.size()) {
				++a_active.step;
				const auto& entry = sequence->entries[a_active.step];
				REX::INFO("Sequence \"{}\": step {}/{}: \"{}\"", sequence->id, a_active.step + 1, sequence->entries.size(), entry.scene);
				if (!PlayOnActiveScene(a_active, entry.scene, entry.speed)) {
					a_active.sequence.reset();
				}
				ArmAutoplay(a_active);
				return;
			}
			REX::INFO("Sequence \"{}\": finished on \"{}\"", sequence->id, a_active.sceneID);
			a_active.sequence.reset();
			SceneEvents::SequenceEnded(a_active.actors, sequence->id, a_active.threadID);
			HUD::OnFocusedSceneChanged();  // navigation back, as OStim's NodeChanged when its sequence ends
			if (a_active.endAfterSequence && !sequence->route) {
				EndActiveScene(a_active, "its sequence ended (EndAfterSequence)");
				a_active.remaining = -1.0F;
				return;
			}
			// Ended on a transition: let it finish moving on.
			const auto scene = SceneRegistry::Find(a_active.sceneID);
			if (!scene || !scene->IsTransition()) {
				a_active.remaining = -1.0F;
				return;
			}
		}
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene || !scene->IsTransition()) {
			a_active.remaining = -1.0F;
			return;
		}
		REX::INFO("Transition \"{}\" -> \"{}\"", scene->id, scene->destination);
		if (PlayOnActiveScene(a_active, scene->destination, -1)) {
			ArmAutoplay(a_active);
		} else {
			a_active.remaining = -1.0F;
		}
	}

	// A corner message, through Papyrus (Debug.Notification).
	void Notify(const std::string& a_text)
	{
		if (g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "Debug"sv, "Notification"sv, callback, a_text);
		}
	}

	// ---- Auto mode (OStim's) ----
	// Moves a scene on by itself: a random fitting scene every so often,
	// foreplay first in some scenes, then intercourse, sometimes pulling out
	// before a man's climax, and speeding up as excitement rises. Scenes are
	// reached by walking the navigation links (through transitions) when
	// they're close enough, else played directly. Main thread (scene clock).

	using AutoStage = ActiveScene::Auto::Stage;
	std::mt19937 g_autoRandom{ std::random_device{}() };

	float AutoRandom(float a_min, float a_max)
	{
		return a_max <= a_min ? a_min : std::uniform_real_distribution<float>(a_min, a_max)(g_autoRandom);
	}

	bool AutoChance(int a_percent)
	{
		return std::uniform_int_distribution<int>(1, 100)(g_autoRandom) <= a_percent;
	}

	bool HasAnyAction(const SceneRegistry::Scene& a_scene, std::initializer_list<std::string_view> a_types)
	{
		return std::ranges::any_of(a_types, [&](std::string_view a_type) { return a_scene.HasAction(a_type); });
	}

	bool IsIntercourse(const SceneRegistry::Scene& a_scene)
	{
		return HasAnyAction(a_scene, { "analsex", "tribbing", "vaginalsex" });
	}

	float MaxExcitement(const ActiveScene& a_active, bool a_menOnly)
	{
		float most = 0.0F;
		for (const auto id : a_active.actors) {
			if (a_menOnly && !ActorProperties::HasPenis(RE::TESForm::GetFormByID<RE::Actor>(id))) {  // OStim: those with a penis
				continue;
			}
			most = std::max(most, Excitement::Get(id));
		}
		return most;
	}

	// Whether a_active's actors, in their roles, may play a_scene where they are.
	bool ActiveCanPlay(const ActiveScene& a_active, const SceneRegistry::Scene& a_scene)
	{
		return a_scene.actors.size() == a_active.actors.size() && SexesFit(a_scene, a_active.actors) &&
		       SceneFitsFurniture(a_scene, a_active.furnitureType);
	}

	// Scenes reachable from a_from in up to a_steps navigations, each with
	// the scenes on the way there (transitions included), the scene last.
	std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, std::vector<std::string>>> Reachable(
		const ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_from, int a_steps)
	{
		using Route = std::vector<std::string>;
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> found;
		std::vector<std::string> visited{ a_from->id };
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> frontier{ { a_from, {} } };
		for (int step = 0; step < a_steps && !frontier.empty(); ++step) {
			std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> next;
			for (const auto& [scene, route] : frontier) {
				for (const auto& nav : scene->navigations) {
					auto dest = SceneRegistry::Find(nav.to);
					Route path = route;
					// Through transitions to where they settle.
					for (int hop = 0; dest && hop < 8; ++hop) {
						path.push_back(dest->id);
						if (!dest->IsTransition()) {
							break;
						}
						dest = SceneRegistry::Find(dest->destination);
					}
					if (!dest || dest->IsTransition() || !ActiveCanPlay(a_active, *dest) ||
						std::ranges::find(visited, dest->id) != visited.end()) {
						continue;
					}
					visited.push_back(dest->id);
					found.emplace_back(dest, path);
					next.emplace_back(dest, std::move(path));
				}
			}
			frontier = std::move(next);
		}
		return found;
	}

	// Moves a_active to a_target: along a_route if there is one (a short
	// sequence: transitions for their length, other scenes in between for
	// half a second), else directly. Returns the seconds the way takes.
	float AutoGoToNow(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route);

	// As OStim's navigateTo / warpTo: with the player in the scene, and
	// SetUseAutoFades on or the first scene marked fadeOnEntry, it fades to
	// black, moves on 0.7 s in, and fades back 0.55 s later.
	float AutoGoTo(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route)
	{
		const auto first = a_route.empty() ? a_target : SceneRegistry::Find(a_route.front());
		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		if (!withPlayer || !g_vm || g_sceneFade || !first || !(g_settings.useAutoFades || first->fadeOnEntry)) {
			return AutoGoToNow(a_active, a_target, a_route);
		}
		REX::INFO("Auto mode: fading to \"{}\"", a_target->id);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeToBlack"sv, callback);
		Scheduler::AfterOnMain(std::chrono::milliseconds(700), [role0 = a_active.actors[0], target = a_target, route = a_route]() {
			if (const auto active = FindActiveScene(role0)) {
				AutoGoToNow(*active, target, route);
			}
		});
		Scheduler::After(std::chrono::milliseconds(700 + 550), []() {
			if (g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> after;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeFromBlack"sv, after, 0.0F);
			}
		});
		return 1.25F;
	}

	float AutoGoToNow(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route)
	{
		const int finalSpeed = a_target->defaultSpeed;
		if (a_route.size() <= 1) {
			REX::INFO("Auto mode: \"{}\" -> \"{}\"{}", a_active.sceneID, a_target->id, a_route.empty() ? " (no route, played directly)" : "");
			if (PlayOnActiveScene(a_active, a_target->id, finalSpeed)) {
				ArmAutoplay(a_active);
			}
			return 0.0F;
		}
		auto  sequence = std::make_shared<SceneRegistry::Sequence>();
		float total = 0.0F;
		sequence->id = "auto mode";
		sequence->name = "auto mode";
		sequence->route = true;
		sequence->actorCount = a_active.actors.size();
		for (std::size_t i = 0; i < a_route.size(); ++i) {
			const auto scene = SceneRegistry::Find(a_route[i]);
			if (!scene) {
				return 0.0F;
			}
			const bool last = i + 1 == a_route.size();
			const float duration = scene->IsTransition() && scene->length > 0.0F ? scene->length : last ? 0.1F : 0.5F;
			const int   speed = last ? finalSpeed : MappedSpeed(a_active, *scene);
			sequence->entries.push_back({ scene->id, duration, speed });
			total += duration;
		}
		REX::INFO("Auto mode: \"{}\" -> \"{}\" in {} step(s)", a_active.sceneID, a_target->id, a_route.size());
		const auto& first = sequence->entries.front();
		if (!PlayOnActiveScene(a_active, first.scene, first.speed)) {
			return 0.0F;
		}
		a_active.sequence = std::move(sequence);
		a_active.step = 0;
		ArmAutoplay(a_active);
		return total;
	}

	// Moves a_active on to a random scene for auto mode's stage.
	void AutoProgress(ActiveScene& a_active)
	{
		auto& state = a_active.autoMode;
		state.cooldown = AutoRandom(g_autoConfig.sceneMin, g_autoConfig.sceneMax);
		const auto current = SceneRegistry::Find(a_active.sceneID);
		if (!current) {
			return;
		}

		std::function<bool(const SceneRegistry::Scene&)> wanted;
		if (a_active.actors.size() == 1) {
			const bool male = ActorProperties::HasPenis(RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[0]));  // OStim's hasSchlong
			wanted = [male](const SceneRegistry::Scene& s) { return s.HasAction(male ? "malemasturbation" : "femalemasturbation"); };
		} else if (state.stage == AutoStage::kForeplay) {
			wanted = [](const SceneRegistry::Scene& s) { return !IsIntercourse(s) && s.HasActionTag("sexual"); };
		} else {
			wanted = [](const SceneRegistry::Scene& s) { return IsIntercourse(s); };
		}

		const auto reachable = Reachable(a_active, current, g_autoConfig.maxSteps);
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, std::vector<std::string>>> candidates;
		auto consider = [&](const std::shared_ptr<const SceneRegistry::Scene>& a_scene, std::vector<std::string> a_route) {
			if (a_scene->id != current->id && !a_scene->noRandomSelection && !a_scene->IsTransition() && wanted(*a_scene)) {
				candidates.emplace_back(a_scene, std::move(a_route));
			}
		};
		if (current->HasActionTag("sexual") && g_autoConfig.limitToNavigation) {
			for (const auto& [scene, route] : reachable) {
				consider(scene, route);
			}
		} else {
			for (const auto& summary : SceneRegistry::List(a_active.actors.size())) {
				const auto scene = SceneRegistry::Find(summary.id);
				if (!scene || !ActiveCanPlay(a_active, *scene)) {
					continue;
				}
				const auto it = std::ranges::find_if(reachable, [&](const auto& a_entry) { return a_entry.first->id == scene->id; });
				consider(scene, it != reachable.end() ? it->second : std::vector<std::string>{});
			}
		}
		// As OStim: off furniture standing scenes, on a bed lying ones, when there are any.
		if (g_autoConfig.standingOnFloor && a_active.actors.size() > 1) {
			const bool onBed = !a_active.furnitureType.empty() && Furniture::IsA(a_active.furnitureType, "bed");
			const bool offFurniture = a_active.furnitureType.empty();
			if (onBed || offFurniture) {
				auto preferred = candidates;
				std::erase_if(preferred, [&](const auto& a_entry) {
					const bool standing = std::ranges::any_of(a_entry.first->actors, [](const SceneRegistry::SceneActor& a) { return a.HasTag("standing"); });
					return offFurniture ? !standing : standing;
				});
				if (!preferred.empty()) {
					candidates = std::move(preferred);
				}
			}
		}
		if (candidates.empty()) {
			REX::INFO("Auto mode: nothing to move \"{}\" on to ({})", a_active.sceneID,
				state.stage == AutoStage::kForeplay ? "foreplay" : a_active.actors.size() == 1 ? "solo" : "intercourse");
			return;
		}
		const auto& pick = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(g_autoRandom)];
		state.cooldown += AutoGoTo(a_active, pick.first, pick.second);
	}

	// A man is about to climax: out to his "pullout" scene, else to a nearby
	// scene with no intercourse where he finishes by hand.
	bool AutoPullOut(ActiveScene& a_active)
	{
		const auto current = SceneRegistry::Find(a_active.sceneID);
		if (!current) {
			return false;
		}
		for (const auto& role : current->actors) {
			if (const auto dest = role.AutoTransition("pullout"); !dest.empty()) {
				if (const auto scene = SceneRegistry::Find(dest); scene && ActiveCanPlay(a_active, *scene)) {
					REX::INFO("Auto mode: pulling out to \"{}\"", dest);
					AutoGoTo(a_active, scene, {});
					return true;
				}
			}
		}
		auto near = Reachable(a_active, current, 3);
		std::erase_if(near, [](const auto& a_entry) {
			return a_entry.first->noRandomSelection || IsIntercourse(*a_entry.first) || !a_entry.first->HasAction("malemasturbation");
		});
		if (near.empty()) {
			return false;
		}
		const auto& pick = near[std::uniform_int_distribution<std::size_t>(0, near.size() - 1)(g_autoRandom)];
		REX::INFO("Auto mode: pulling out");
		AutoGoTo(a_active, pick.first, pick.second);
		return true;
	}

	void StartAutoMode(ActiveScene& a_active)
	{
		auto& state = a_active.autoMode;
		if (state.on) {
			return;
		}
		if (a_active.noAutoMode) {
			// OStim's Thread::startAutoMode: a thread built with NoAutoMode
			// never runs it, whoever asks (the key, FourThread, the C++ API).
			REX::INFO("Auto mode: not for \"{}\", its thread was built with NoAutoMode", a_active.sceneID);
			return;
		}
		if (state.stage == AutoStage::kNone) {
			if (a_active.actors.size() == 1) {
				state.stage = AutoStage::kMain;
			} else {
				if (AutoChance(g_autoConfig.foreplayChance)) {
					state.stage = AutoStage::kForeplay;
					state.foreplayUntil = AutoRandom(g_autoConfig.foreplayMin, g_autoConfig.foreplayMax);
				} else {
					state.stage = AutoStage::kMain;
				}
				state.pulloutAt = AutoChance(g_autoConfig.pulloutChance) ? AutoRandom(g_autoConfig.pulloutMin, g_autoConfig.pulloutMax) : 0.0F;
			}
		}
		state.on = true;
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		// Already in a sexual scene: stay a while. Otherwise move on now.
		state.cooldown = scene && scene->HasActionTag("sexual") ? AutoRandom(g_autoConfig.sceneMin, g_autoConfig.sceneMax) : 0.0F;
		state.speedCooldown = AutoRandom(g_autoConfig.speedIntervalMin, g_autoConfig.speedIntervalMax);
		REX::INFO("Auto mode: on for \"{}\" ({}{})", a_active.sceneID,
			state.stage == AutoStage::kForeplay ? std::format("foreplay to {:.0f}", state.foreplayUntil) : std::string("intercourse"),
			state.pulloutAt > 0.0F ? std::format(", pull-out at {:.0f}", state.pulloutAt) : std::string{});
		StartAutoplayTicks();
	}

	void StopAutoMode(ActiveScene& a_active)
	{
		if (a_active.autoMode.on) {
			a_active.autoMode.on = false;
			REX::INFO("Auto mode: off for \"{}\"", a_active.sceneID);
		}
	}

	// OStim's speed control (Thread::loopAutoControl): faster as excitement
	// rises, every autoSpeedControlInterval, in every NPC scene and, with
	// SetActorSpeedControl (bAutoSpeed), the player's, auto mode or not.
	// The chance is 0 at autoSpeedControlExcitementMin, certain at Max.
	void SpeedControlTick(ActiveScene& a_active, float a_seconds)
	{
		if (a_active.ending || a_active.sequence || a_active.remaining >= 0.0F || !a_active.queuedScene.empty() || a_active.endIn >= 0.0F) {
			return;  // OStim: not while its node queue moves it (a sequence, a route, a transition)
		}
		const bool player = std::ranges::any_of(a_active.actors, [](std::uint32_t a) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a)); });
		if (player && !g_autoConfig.autoSpeed) {
			return;
		}
		auto& cooldown = a_active.autoMode.speedCooldown;
		if ((cooldown -= a_seconds) > 0.0F) {
			return;
		}
		cooldown = AutoRandom(g_autoConfig.speedIntervalMin, g_autoConfig.speedIntervalMax);
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene) {
			return;
		}
		const float excitement = MaxExcitement(a_active, false);
		const float span = std::max(g_autoConfig.speedExcitementMax - g_autoConfig.speedExcitementMin, 1.0F);
		const int   chance = static_cast<int>(std::clamp((excitement - g_autoConfig.speedExcitementMin) * 100.0F / span, 0.0F, 100.0F));
		if (a_active.speed + 1 < static_cast<int>(scene->speeds.size()) && AutoChance(chance)) {
			PlayOnActiveScene(a_active, scene->id, a_active.speed + 1);  // sends the speed event
		}
	}

	void AutoModeTick(ActiveScene& a_active, float a_seconds)
	{
		auto& state = a_active.autoMode;
		// Not while it's moving (a transition or a route), or ending.
		if (!state.on || a_active.sequence || a_active.remaining >= 0.0F || !a_active.queuedScene.empty() || a_active.endIn >= 0.0F) {
			return;
		}
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene) {
			return;
		}

		if (state.stage == AutoStage::kPullout) {
			return;  // until the climax (HandleClimax)
		}
		if (state.stage == AutoStage::kForeplay && MaxExcitement(a_active, false) > state.foreplayUntil) {
			REX::INFO("Auto mode: foreplay over for \"{}\"", a_active.sceneID);
			state.stage = AutoStage::kMain;
			state.cooldown = 0.0F;
		}
		if (state.stage == AutoStage::kMain && state.pulloutAt > 0.0F && IsIntercourse(*scene) && MaxExcitement(a_active, true) > state.pulloutAt) {
			state.stage = AutoStage::kPullout;
			AutoPullOut(a_active);
			return;
		}
		if ((state.cooldown -= a_seconds) < 0.0F) {
			AutoProgress(a_active);
		}
	}

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
	std::mutex                        g_heelsWaitingLock;
	std::unordered_set<std::uint32_t> g_heelsWaiting;

	void RestartHeels(std::uint32_t a_id, const char* a_why)
	{
		if (!g_vm) {
			return;
		}
		REX::INFO("Heels: {:08X} back on ({})", a_id, a_why);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "SetHeels"sv, callback, static_cast<std::int32_t>(a_id), true);
	}

	// Whether the High Heels System's plugin is there (its Papyrus API turns
	// an actor's height off and on).
	bool HHSInstalled()
	{
		static const bool installed = std::filesystem::exists("Data/F4SE/Plugins/HHS.dll");
		return installed;
	}

	// Turns role a_role's heel height off (or back on) through HHS. Main thread.
	void SetHeelsOff(ActiveScene& a_active, std::size_t a_role, bool a_off)
	{
		a_active.heelsOff.resize(a_active.actors.size(), false);
		if (!g_vm || a_role >= a_active.actors.size() || a_active.heelsOff[a_role] == a_off) {
			return;
		}
		a_active.heelsOff[a_role] = a_off;
		REX::INFO("Heels: {:08X} {} (High Heels System)", a_active.actors[a_role], a_off ? "off: feet not on the ground" : "back on");
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "SetHeels"sv, callback, static_cast<std::int32_t>(a_active.actors[a_role]), !a_off);
	}

	ActorSpot SpotOf(const ActiveScene& a_active, std::size_t a_role)
	{
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
		const auto id = settled ? settled->id : a_active.sceneID;
		const auto o = Alignment::Get(a_active.alignKey, id, a_role);
		// As OStim's alignActor: the role's offset from the scene file plus
		// the alignment.
		SceneRegistry::Position base;
		if (settled && a_role < settled->actors.size()) {
			base = settled->actors[a_role].offset;
			// OStim's scaleOffsetWithFurniture: on furniture, the offset grows
			// and shrinks with it.
			if (settled->scaleOffsetWithFurniture && !a_active.furnitureType.empty()) {
				base.x *= a_active.furnitureRefScale;
				base.y *= a_active.furnitureRefScale;
				base.z *= a_active.furnitureRefScale;
			}
		}
		const float x = o.x + base.x, y = o.y + base.y, z = o.z + base.z, r = o.rot + base.r;
		// On a bed: its own scenes at the edge spot, floor scenes along the middle.
		auto  center = a_active.center;
		float heading = a_active.heading;
		if (a_active.furnitureSpots && settled) {
			const auto& chosen = settled->furniture.empty() ? a_active.furnitureSpots->floor : a_active.furnitureSpots->furniture;
			center = chosen.position;
			heading = chosen.heading;
		}
		const float c = std::cos(heading), s = std::sin(heading);
		ActorSpot spot;
		// Facing heading h, forward is (sin h, cos h) and right is (cos h, -sin h).
		spot.position = { center.x + x * c + y * s, center.y - x * s + y * c, center.z + z };
		spot.heading = heading + r * PI_F / 180.0F;
		// As OStim: heels come off an actor whose feet aren't on the ground
		// (lying, kneeling...). With the High Heels System, its own height is
		// turned off (LockScene, HHS's StopHeight_HHS, as OStim takes the
		// NiOverride offset off); without it, their spot is lowered by it.
		if (!HHSInstalled() && a_role < a_active.heels.size() && a_active.heels[a_role] > 0.0F && settled && a_role < settled->actors.size() &&
			!settled->actors[a_role].feetOnGround) {
			spot.position.z -= a_active.heels[a_role];
		}
		spot.scale = o.scale;
		return spot;
	}

	// Reads each actor's heels and builds the alignment key for the scene
	// playing, as OStim's rebuildAlignmentKey: sex, NPC height, and heel
	// height unless the heels are off (feet not on the ground).
	void RefreshActors(ActiveScene& a_active)
	{
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
		a_active.heels.resize(a_active.actors.size(), 0.0F);
		std::vector<Alignment::ActorInfo> info;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role]);
			// As OStim's getActorConditions, read fresh: what they are and
			// have now (a penis put on or taken off mid-scene), for the
			// navigation and the picker, at every scene change.
			if (actor) {
				ActorProperties::Remember(actor);
			}
			a_active.heels[role] = Scaling::HeelHeight(actor);
			const bool feetOnGround = settled && role < settled->actors.size() && settled->actors[role].feetOnGround;
			info.push_back({ SceneRegistry::SexOf(actor), Scaling::NPCHeight(actor), feetOnGround ? a_active.heels[role] : 0.0F });
		}
		a_active.alignKey = Alignment::KeyFor(info);
	}

	void LockScene(ActiveScene& a_active)
	{
		if (!g_vm) {
			return;
		}
		RefreshActors(a_active);
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::RoleScene(scene) : nullptr;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto id = a_active.actors[role];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			const auto spot = SpotOf(a_active, role);
			// HHS on only while they wear heels with their feet on the ground.
			// HHS ignores shoes coming off while its height is off, and turning
			// it back on restores the old height: so with the shoes off it
			// stays off (else they float), and comes back on with the shoes.
			if (HHSInstalled() && role < a_active.heels.size()) {
				a_active.hadHeels.resize(a_active.actors.size(), false);
				if (a_active.heels[role] > 0.0F) {
					a_active.hadHeels[role] = true;
				}
				if (a_active.hadHeels[role]) {
					const bool feet = settled && role < settled->actors.size() && settled->actors[role].feetOnGround;
					SetHeelsOff(a_active, role, a_active.heels[role] <= 0.0F || !feet);
				}
			}
			// The penis bend (OStim's setSoSBend): the alignment's, else the scene's.
			{
				const auto found = settled ? Alignment::Find(a_active.alignKey, settled->id, role) : std::nullopt;
				PenisBend::Set(id, found ? found->bend : settled && role < settled->actors.size() ? settled->actors[role].penisBend : 0.0F, found.has_value());
			}
			// OStim's look directions (its eyeballModifierOverride), for this scene.
			if (settled && role < settled->actors.size()) {
				EyeLook::Set(id, settled->actors[role].lookUp, settled->actors[role].lookLeft);
			} else {
				EyeLook::Set(id, 0.0F, 0.0F);
			}
			// The actor's scale, only when it changes. As OStim's
			// ThreadActor::scale: the role's scale times the alignment's,
			// over their own NPC height, and with heels on, times
			// scaleHeight / (scaleHeight + heels); with SetScaling=1 their
			// own scale times the alignment's.
			if (role < a_active.baseScale.size()) {
				// OStim's setScaleMult(alignment.scale * furnitureScaleMult).
				const float mult = spot.scale * a_active.furnitureScaleMult;
				float       wanted = a_active.baseScale[role] * mult;
				if (!Scaling::Settings().disabled && settled && role < settled->actors.size()) {
					const auto& part = settled->actors[role];
					wanted = part.scale * mult / Scaling::NPCHeight(actor);
					if (role < a_active.heels.size() && a_active.heels[role] > 0.0F && part.feetOnGround) {
						wanted *= part.scaleHeight / (part.scaleHeight + a_active.heels[role]);
					}
				}
				const float current = a_active.appliedScale[role] > 0.0F ? a_active.appliedScale[role] : a_active.baseScale[role];
				if (std::fabs(wanted - current) > 0.001F) {
					CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, wanted);
					a_active.appliedScale[role] = wanted;
					// With what the height is made of, to check NPC height in game
					// (parity audit 2 §21: OStim scales by the base's height;
					// whether Fallout 4 also uses heightMax or the race's height
					// is unverified): the base's height and height max, and the
					// 3D's scale before this change.
					const auto npc = actor->GetNPC();
					const auto root = actor->Get3D();
					REX::INFO("Scaling: {:08X} (role {}) scaled to {:.3f} (heels {:.1f}, feet on the ground {}; NPC height {:.3f}, height max {:.3f}, drawn at scale {:.3f} before)", id, role, wanted,
						role < a_active.heels.size() ? a_active.heels[role] : 0.0F, settled && role < settled->actors.size() && settled->actors[role].feetOnGround,
						npc ? npc->height : 0.0F, npc ? npc->heightMax : 0.0F, root ? root->world.scale : 0.0F);
				}
			}
			if (!g_lockScenes) {
				continue;
			}
			FaceHeading(actor, spot.heading);  // face the right way first; the translation then holds it
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			if (!g_settings.translationLock) {
				// bTranslationLock=0 (finding a shake): put on the spot once, with
				// no translation holding them there every frame; the scene
				// guard's watchdog puts them back if they drift.
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "Unlock"sv, callback, static_cast<std::int32_t>(id));
				actor->SetPosition(spot.position, true);
				continue;
			}
			Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "LockInPlace"sv, callback, static_cast<std::int32_t>(id), spot.position.x, spot.position.y,
				spot.position.z, spot.heading * 180.0F / PI_F);
		}
	}

	// Actors leaving a scene: their own scale back (and their heels).
	void RestoreScales(const ActiveScene& a_active)
	{
		for (std::size_t role = 0; role < a_active.actors.size() && role < a_active.heelsOff.size(); ++role) {
			if (a_active.heelsOff[role]) {
				// Back on now if they wear heels; else once they're dressed
				// again (HeelsChanged, from the redress).
				const auto id = a_active.actors[role];
				if (Scaling::HeelHeight(RE::TESForm::GetFormByID<RE::Actor>(id)) > 0.0F) {
					RestartHeels(id, "scene over");
				} else {
					std::scoped_lock lock(g_heelsWaitingLock);
					g_heelsWaiting.insert(id);
					REX::INFO("Heels: {:08X} stay off until their shoes are back on", id);
				}
			}
		}
		for (std::size_t role = 0; role < a_active.actors.size() && role < a_active.appliedScale.size(); ++role) {
			if (a_active.appliedScale[role] > 0.0F) {
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role])) {
					CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, a_active.baseScale[role]);
				}
			}
		}
	}

	void RecenterFreeCamera();

	// ---- Changing furniture mid-scene (OStim's changeFurniture) ----
	// The scene leaves its furniture (free again, clutter put back) and goes
	// on a_piece with a_sceneID, or a random intro / idle scene that fits
	// there (OStim's: on a bed one with no one standing). Main thread.
	bool MoveSceneToFurniture(ActiveScene& a_active, const Furniture::Found& a_piece, const std::string& a_sceneID)
	{
		if (a_piece.ref && a_piece.ref == a_active.furnitureRef) {
			return false;  // as OStim: already on it
		}
		const auto ref = a_piece.ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_piece.ref) : nullptr;
		if (!ref && !a_piece.spot) {
			REX::WARN("Furniture change: {:08X} is gone", a_piece.ref);
			return false;
		}
		std::shared_ptr<const SceneRegistry::Scene> scene;
		std::string                                 why = "asked for";
		if (!a_sceneID.empty()) {
			scene = SceneRegistry::Find(a_sceneID);
			if (scene && (scene->actors.size() != a_active.actors.size() || !SexesFit(*scene, a_active.actors) || !SceneFitsFurniture(*scene, a_piece.type))) {
				REX::WARN("Furniture change: \"{}\" doesn't fit these actors on {}, picking one", a_sceneID, a_piece.type);
				scene.reset();
			}
		}
		if (!scene) {
			scene = PickStartScene(ScenesForRunning(a_active.actors, a_piece.type), a_piece.type, true, why);
		}
		if (!scene) {
			REX::WARN("Furniture change: no scene fits these {} actor(s) on {}", a_active.actors.size(), a_piece.type);
			Notify(TR("$4stim_no_scene_there", "No scene fits them there"));
			return false;
		}
		REX::INFO("Furniture change: \"{}\" from {} ({:08X}) to {} ({:08X}) with \"{}\" ({})", a_active.sceneID,
			a_active.furnitureType.empty() ? std::string("the floor") : a_active.furnitureType, a_active.furnitureRef, a_piece.type, a_piece.ref, scene->id, why);
		FreeFurniture(a_active);

		// The new spot, as for a scene started there (PlaceOnPendingFurniture).
		const auto  player = RE::PlayerCharacter::GetSingleton();
		const auto  near = player ? player->GetPosition() : a_active.center;
		const bool  floorScene = scene->furniture.empty();
		auto        spot = Furniture::SpotOf(a_piece, near, floorScene);
		const auto& o = scene->furnitureOffset;
		{
			const float c = std::cos(spot.heading), s = std::sin(spot.heading);
			spot.position.x += o[0] * c + o[1] * s;
			spot.position.y += -o[0] * s + o[1] * c;
			spot.position.z += o[2];
			spot.heading += o[3] * PI_F / 180.0F;
		}
		a_active.furnitureSpots.reset();
		if (ref && Furniture::IsEdgeAnchored(a_piece.type)) {
			const auto other = Furniture::SpotFor(ref, a_piece.type, near, !floorScene);
			auto       sorted = a_active.actors;
			std::ranges::sort(sorted);
			a_active.furnitureSpots = FurnitureSpots{ sorted, floorScene ? other : spot, floorScene ? spot : other };
		}
		if (g_settings.resetClutter) {
			a_active.clutter = Furniture::SnapshotClutter(spot.position, g_settings.resetClutterRadius);  // before anyone is moved there
		}
		a_active.furnitureRef = a_piece.ref;
		a_active.furnitureType = a_piece.type;
		a_active.furnitureFixedSpot = a_piece.spot;
		SetFurnitureScales(a_active, a_piece.ref, a_piece.type);
		a_active.center = spot.position;
		a_active.heading = spot.heading;
		BlockFurniture(a_piece.ref);
		IgnoreFurnitureCollision(a_piece.ref, a_active.actors, a_piece.type);

		auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_active.actors.front()) {
			focused.furniture = a_piece.type;
			SetPlayerScene(focused);
		}
		// A sequence or a picked transition destination doesn't carry over.
		if (InterruptSequence(a_active, "the scene changed furniture")) {
			return false;  // the scene is ending (EndAfterSequence)
		}
		a_active.queuedScene.clear();
		PlayOnActiveScene(a_active, scene->id, -1);
		// Everyone over there now (the scene lock follows; without it, they stay).
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role])) {
				const auto at = SpotOf(a_active, role);
				NotePlayerSpotBefore(actor);
				actor->SetPosition(at.position, true);
				FaceHeading(actor, at.heading);
			}
		}
		LockScene(a_active);
		// As OStim (endSceneMode / startSceneMode): the free camera starts
		// again where the player now is.
		if (std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); })) {
			RecenterFreeCamera();
		}
		ArmAutoplay(a_active);
		if (a_active.autoMode.on) {
			a_active.autoMode.cooldown = std::max(a_active.autoMode.cooldown, g_autoConfig.sceneMin);
		}
		SceneEvents::FurnitureChanged(a_active.actors, a_piece.ref, a_piece.type, a_active.sceneID, a_active.threadID);
		ApiForms::FurnitureChanged(a_active.actors, {}, a_piece.type);
		return true;
	}

	// A furniture change asked for (the HUD, the picker, Papyrus): done on
	// the main thread, behind a fade for a scene with the player
	// (SetUseFades, as OStim's changeFurniture).
	struct FurnitureChange
	{
		std::uint32_t    role0 = 0;  // the scene's
		Furniture::Found piece;
		std::string      sceneID;
	};
	std::mutex                     g_furnitureChangeLock;
	std::optional<FurnitureChange> g_furnitureChange;

	void ApplyFurnitureChange()
	{
		std::optional<FurnitureChange> change;
		{
			std::scoped_lock lock(g_furnitureChangeLock);
			change = std::exchange(g_furnitureChange, std::nullopt);
		}
		if (!change) {
			return;
		}
		if (const auto active = FindActiveScene(change->role0)) {
			MoveSceneToFurniture(*active, change->piece, change->sceneID);
		}
	}

	// Main thread.
	void RequestFurnitureChange(std::uint32_t a_role0, Furniture::Found a_piece, std::string a_sceneID)
	{
		const auto active = FindActiveScene(a_role0);
		if (!active) {
			return;
		}
		const bool withPlayer = std::ranges::any_of(active->actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		{
			std::scoped_lock lock(g_furnitureChangeLock);
			g_furnitureChange = FurnitureChange{ active->actors.front(), std::move(a_piece), std::move(a_sceneID) };
		}
		if (withPlayer && g_settings.useFades && g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "ChangeFurnitureFaded"sv, callback);  // FadeToBlack, ApplyFurnitureChange, FadeFromBlack
		} else {
			ApplyFurnitureChange();
		}
	}

	void UnlockActors(const std::vector<std::uint32_t>& a_ids)
	{
		ScenePackage::Remove(a_ids);  // before ReleaseFromScene's EvaluatePackage hands them back their routine
		if (!g_vm) {
			return;
		}
		for (const auto id : a_ids) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "Unlock"sv, callback, static_cast<std::int32_t>(id));
			// Back to their routine right away: end the do-nothing package the
			// scene gave them (LeaveFurniture) and the scene guard's hold on
			// their idles (packageIdleTimer, renewed to 30 s every check),
			// which left NPCs standing still for a long while after a scene.
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (actor && !IsPlayer(actor)) {
				actor->EndInterruptPackage(false);
				const auto process = actor->currentProcess;
				if (const auto data = process ? process->middleHigh : nullptr) {
					data->packageIdleTimer = 0.0F;
				}
			}
		}
	}

	// A scene ended. As OStim does with "Reset position" (on by default):
	// each NPC goes back to where they stood before the scene, facing the way
	// they faced (FourStimScene.ResetPosition: a translation there at a huge
	// speed, OStim's setPosition). The player goes back only if the scene
	// moved them onto furniture (NotePlayerSpotBefore; natively, as they were
	// placed); otherwise the scene was built on their own spot. That also takes them out of each other
	// before their collision with each other comes back (RestoreCollision
	// waits for this). NPCs with nowhere recorded (a scene started again
	// from a save) step 70 units off the spot instead, in the first clear
	// direction, so no one is left stuck inside someone else.
	void MoveApart(const ActiveScene& a_ended)
	{
		if (!g_vm) {
			return;
		}
		std::vector<std::uint32_t> unplaced;
		for (const auto id : a_ended.actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			const auto before = g_positionBefore.find(id);
			if (actor && IsPlayer(actor)) {
				if (before != g_positionBefore.end()) {
					if (g_resetPosition && !(a_ended.interrupted && g_stayWhenInterrupted)) {
						REX::INFO("Scene end: the player back to where they stood before the furniture scene");
						PlaceActorAt(actor, Furniture::Spot{ before->second.position, before->second.heading });
					}
					g_positionBefore.erase(before);
				}
				continue;
			}
			if (!actor) {
				continue;
			}
			if (before != g_positionBefore.end()) {
				if (g_resetPosition && !(a_ended.interrupted && g_stayWhenInterrupted)) {
					const auto& b = before->second;
					REX::INFO("Scene end: {:08X} back to where they stood before the scene", id);
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "ResetPosition"sv, callback, static_cast<std::int32_t>(id), b.position.x, b.position.y,
						b.position.z, b.heading * 180.0F / PI_F);
				} else {
					unplaced.push_back(id);
				}
				g_positionBefore.erase(before);
			} else {
				unplaced.push_back(id);
			}
		}
		if (a_ended.actors.size() < 2 || unplaced.empty()) {
			return;
		}
		std::size_t stay = 0;
		for (std::size_t role = 0; role < a_ended.actors.size(); ++role) {
			if (IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a_ended.actors[role]))) {
				stay = role;
			}
		}
		constexpr float DISTANCE = 70.0F;
		const std::array<float, 4> directions{ PI_F, -PI_F * 0.5F, PI_F * 0.5F, 0.0F };  // behind, left, right, ahead
		std::size_t                next = 0;
		for (std::size_t role = 0; role < a_ended.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_ended.actors[role]);
			if (role == stay || !actor || IsPlayer(actor) || std::ranges::find(unplaced, actor->GetFormID()) == unplaced.end()) {
				continue;
			}
			bool moved = false;
			for (std::size_t tries = 0; tries < directions.size() && !moved; ++tries) {
				const float        angle = a_ended.heading + directions[(next + tries) % directions.size()];
				const RE::NiPoint3 to{ a_ended.center.x + DISTANCE * std::sin(angle), a_ended.center.y + DISTANCE * std::cos(angle), a_ended.center.z };
				const RE::NiPoint3 up{ 0.0F, 0.0F, 40.0F };
				if (Furniture::ClearFraction(actor, a_ended.center + up, to + up) < 0.99F) {
					continue;
				}
				REX::INFO("Scene end: {:08X} steps off the spot, {:.0f} units away", actor->GetFormID(), DISTANCE);
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "MoveApart"sv, callback, static_cast<std::int32_t>(actor->GetFormID()), to.x, to.y, to.z);
				next = (next + tries + 1) % directions.size();
				moved = true;
			}
			if (!moved) {
				REX::INFO("Scene end: {:08X} has nowhere clear to step to; left on the spot", actor->GetFormID());
			}
		}
	}

	// The furniture or idle marker a_actor is using, if any (nullptr if none
	// or if the handle no longer resolves).
	RE::NiPointer<RE::TESObjectREFR> FurnitureOf(RE::Actor* a_actor)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		const auto data = process ? process->middleHigh : nullptr;
		if (!data) {
			return nullptr;
		}
		if (auto ref = data->occupiedFurniture.get()) {
			return ref;
		}
		return data->currentFurniture.get();
	}

	// An NPC using furniture or an idle marker (a workbench, a broom, a
	// chair...) stays in it under any idle we play, props and all: get them
	// out on the spot, and give their AI a do-nothing package so it doesn't
	// walk them back. Main thread. True if they were in something.
	bool LeaveFurniture(RE::Actor* a_actor, bool a_doNothing)
	{
		const auto furniture = FurnitureOf(a_actor);
		if (furniture) {
			const auto base = furniture->GetObjectReference();
			REX::INFO("Furniture use: {:08X} is using {:08X} (base {:08X} \"{}\"), taking them out", a_actor->GetFormID(), furniture->GetFormID(),
				base ? base->GetFormID() : 0, base && base->GetFormEditorID() ? base->GetFormEditorID() : "");
			g_guardKicks[a_actor->GetFormID()].takenOutOf = furniture->GetFormID();
			if (Compat::CanStopInteractingQuick()) {
				a_actor->StopInteractingQuick(true, false, true);
			} else if (g_vm) {
				// 1.10.163 (no StopInteractingQuick there, docs/OLDGEN.md): a
				// tester's NPC stayed in a mirror's idle marker under every
				// scene. Moved onto their own spot through Papyrus instead,
				// which takes an actor out of furniture.
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "LeaveFurnitureByID"sv, callback, static_cast<std::int32_t>(a_actor->GetFormID()));
			}
		}
		if (a_doNothing || furniture) {
			a_actor->InitiateDoNothingPackage();
		}
		return furniture != nullptr;
	}


	// Once a second: anyone still on the scene package who isn't in a scene
	// any more (ScenePackage::Prune) gets their own packages back now.
	void PruneScenePackage(float a_seconds)
	{
		static float clock = 0.0F;
		if ((clock += a_seconds) < 1.0F) {
			return;
		}
		clock = 0.0F;
		std::vector<std::uint32_t> inScenes;
		for (const auto& active : g_activeScenes) {
			inScenes.insert(inScenes.end(), active.actors.begin(), active.actors.end());
		}
		for (const auto id : ScenePackage::Prune(inScenes)) {
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
				CallActorMethod(actor, "Actor"sv, "EvaluatePackage"sv, false);
			}
		}
	}

	// ---- Visibility check (diagnostic) ----
	// Testers and the owner have had an actor placed and animated in a scene
	// (the log says so) but not drawn: the player once, an NPC on 1.10.163.
	// Every 2 s the scene guard looks at each scene actor's 3D and logs it at
	// the start and whenever it changes, with a warning when something would
	// hide them: no 3D, the root culled, faded out, no geometry or all of it
	// culled, or the 3D somewhere other than the actor. Logging only.
	struct Visibility
	{
		bool  has3D = false;
		bool  rootCulled = false;
		float fade = 1.0F;  // the root fade node's, 1 = fully shown
		int   geometry = 0;
		int   culledGeometry = 0;
		float offset = 0.0F;  // the 3D's distance from the actor's position
	};

	void CountGeometry(RE::NiAVObject* a_object, Visibility& a_out, int a_depth, bool a_culled)
	{
		if (!a_object) {
			return;
		}
		const bool culled = a_culled || a_object->GetAppCulled();
		if (a_object->IsGeometry()) {
			++a_out.geometry;
			if (culled) {
				++a_out.culledGeometry;
			}
			return;
		}
		if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
			for (const auto& child : node->children) {
				CountGeometry(child.get(), a_out, a_depth - 1, culled);
			}
		}
	}

	Visibility CheckVisibility(RE::Actor* a_actor)
	{
		Visibility v;
		const auto root = IsPlayer(a_actor) ? a_actor->Get3D(false) : a_actor->Get3D();  // the player's third-person 3D
		if (!root) {
			return v;
		}
		v.has3D = true;
		v.rootCulled = root->GetAppCulled();
		if (const auto fade = root->IsFadeNode()) {
			v.fade = fade->currentFade;
		}
		CountGeometry(root, v, 64, false);
		v.offset = root->GetWorldTranslate().GetDistance(a_actor->data.location);
		return v;
	}

	void LogVisibility(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		const auto id = a_actor->GetFormID();
		const auto now = std::chrono::steady_clock::now();
		auto& last = g_visibilityLast[id];
		if (!last.first.empty() && now - last.second < std::chrono::seconds(2)) {
			return;
		}
		last.second = now;
		const auto v = CheckVisibility(a_actor);
		const bool hidden = !v.has3D || v.rootCulled || v.fade < 0.1F || v.geometry == 0 || v.culledGeometry == v.geometry || v.offset > 100.0F;
		// What's compared for "changed": rounded, so a fade or a sway doesn't log every time.
		const auto summary = !v.has3D ? std::string("no 3D") :
		                                std::format("root {}, fade {:.1f}, {} geometry ({} culled), 3D {} the actor", v.rootCulled ? "CULLED" : "shown", v.fade,
											v.geometry, v.culledGeometry, v.offset > 100.0F ? std::format("{:.0f} units from", v.offset) : std::string("at"));
		if (summary == last.first) {
			return;
		}
		last.first = summary;
		if (hidden) {
			REX::WARN("Visibility: {:08X} in \"{}\" may not be drawn: {}", id, a_sceneID, summary);
		} else {
			REX::INFO("Visibility: {:08X} in \"{}\": {}", id, a_sceneID, summary);
		}
	}

	// Fallout 4 only (OStim has nothing like it): the player in a scene but
	// not drawn. In first person the game culls the player's third-person 3D;
	// a scene started from first person forces third person and the free
	// camera, and a first-person body mod (True First Person, in the owner's
	// log: "First-person body OFF", "Third person forced by the engine") can
	// leave it culled, so the player was in the scene, animated, and invisible
	// ("the player character is missing"). Shown again. Main thread.
	void ShowPlayerBody(RE::Actor* a_player, const std::string& a_sceneID)
	{
		static int shown = 0;
		const auto body = a_player ? a_player->Get3D(false) : nullptr;  // third person
		if (!body || !body->GetAppCulled()) {
			return;
		}
		// Not in first person: there the culling is the game's own, and the
		// third-person skeleton isn't animated. A scene ending put the camera
		// back in first person half a second before the scene was let go, and
		// this showed the body then (the owner's log, 5 November): a T-posed
		// player with his head bent, and FSMP crashed in its collider when
		// the physics swap went back on that body a moment later.
		if (const auto camera = RE::PlayerCamera::GetSingleton(); camera && camera->QCameraEquals(RE::CameraState::kFirstPerson)) {
			return;
		}
		body->SetAppCulled(false);
		if (++shown <= 5) {
			REX::INFO("Scene guard: the player's body was hidden in \"{}\" (third-person 3D culled, as first person leaves it): shown again", a_sceneID);
		}
	}

	// Fallout 4 only (OStim has nothing like it): an actor in a scene drawn
	// fully faded out. The third-person camera fades the player (and an NPC it
	// comes close to) out as it closes in on them, as it does against a wall;
	// once the scene switches to the free camera nothing fades them back in,
	// so the player was in the scene, animated, and invisible (the owner's
	// log, a wall scene: "fade 0.0" on the player's 3D, 1.0 in the next
	// scene). Faded back in. Main thread.
	// The actor's own fade, below the 3D's: Actor::UpdateAlpha (vtable slot
	// 0xFB) sets the 3D's fade every update from the high process's
	// fadeAlpha (its fade-in / fade-out / teleport-fade state, ID 2231941
	// steps it), times middleHigh's alphaMult (Papyrus SetAlpha), times
	// what Invisibility leaves. So an actor left in a fade-out
	// (kOut, alpha 0) is pushed back to invisible every frame, whatever the
	// scene guard does to the 3D: a 1.10.163 tester's male NPCs, "keeps
	// being faded out" 57 times a second even with the camera's fade off.
	// A plain fade-out on a scene actor (or a fade-in still under way) is
	// ended at once, fully shown, and an alphaMult below 1 is put back to
	// 1; each logged once a scene. Teleport fades (a MoveTo with a fade) and
	// fades before disabling or deleting are the game's and left alone.
	// Both fields are where AE has them on 1.10.163 too (checked against its
	// executable: the same fade functions write the same offsets).
	void RestoreActorFade(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		if (!process) {
			return;
		}
		static std::unordered_map<std::uint32_t, std::string> logged;
		const auto log = [&](const std::string& a_what) {
			if (auto& last = logged[a_actor->GetFormID()]; last != a_sceneID + a_what) {
				last = a_sceneID + a_what;
				REX::INFO("Scene guard: {:08X} in \"{}\" {}", a_actor->GetFormID(), a_sceneID, a_what);
			}
		};
		if (const auto high = process->high) {
			// Done here, not left to the game: turned into a fade-in (kIn),
			// the player's never got anywhere (the owner's log, 6 November:
			// a chair scene's start, the camera fading him out before its
			// fade was off; then "actor fade state 1, alpha 0.00" for the
			// whole scene, invisible). So a fade-out, or a fade-in still
			// short of 1, ends at once: no fade, fully shown.
			using FADE = RE::HighProcessData::FADE_STATE;
			const auto state = high->fadeState.get();
			if (state == FADE::kOut || (state == FADE::kIn && high->fadeAlpha < 0.99F)) {
				log(std::format("was fading {} (the game's own actor fade, alpha {:.2f}): shown again", state == FADE::kOut ? "out" : "in", high->fadeAlpha));
				high->fadeState = FADE::kNormal;
				high->fadeAlpha = 1.0F;
			}
		}
		if (const auto middle = process->middleHigh; middle && middle->alphaMult < 0.99F) {
			log(std::format("had alpha {:.2f} (SetAlpha, by a script or another mod): back to 1", middle->alphaMult));
			middle->alphaMult = 1.0F;
		}
	}

	void RestoreFade(RE::Actor* a_actor, const std::string& a_sceneID)
	{
		RestoreActorFade(a_actor, a_sceneID);
		const auto root = IsPlayer(a_actor) ? a_actor->Get3D(false) : a_actor->Get3D();
		const auto fade = root ? root->IsFadeNode() : nullptr;
		if (!fade || fade->currentFade >= 0.99F) {
			return;
		}
		// Logged once per actor and scene, then how many times it came back:
		// in the owner's log a single fade-in wasn't enough (the player was
		// still missing), so it's now done on every tick of the scene clock,
		// and the count says whether the game keeps fading them out.
		struct Seen
		{
			std::string                           scene;
			int                                   again = 0;
			std::chrono::steady_clock::time_point lastLog{};
		};
		static std::unordered_map<std::uint32_t, Seen> seen;
		auto&      entry = seen[a_actor->GetFormID()];
		const auto now = std::chrono::steady_clock::now();
		if (entry.scene != a_sceneID) {
			entry = { a_sceneID, 0, now };
			REX::INFO("Scene guard: {:08X} in \"{}\" was faded out ({:.2f}, the camera's fade as it closed in): faded back in", a_actor->GetFormID(), a_sceneID,
				fade->currentFade);
		} else if (++entry.again; now - entry.lastLog > std::chrono::seconds(5)) {
			entry.lastLog = now;
			// What the game's alpha is made of (Actor::UpdateAlpha): if these
			// are all 1, it's Invisibility or something else.
			const auto process = a_actor->currentProcess;
			const auto high = process ? process->high : nullptr;
			const auto middle = process ? process->middleHigh : nullptr;
			REX::WARN("Scene guard: {:08X} in \"{}\" keeps being faded out ({} times so far, now {:.2f}; actor fade state {}, alpha {:.2f}, alpha mult {:.2f}): fading them back in each tick",
				a_actor->GetFormID(), a_sceneID, entry.again, fade->currentFade, high ? static_cast<int>(std::to_underlying(high->fadeState.get())) : -1,
				high ? high->fadeAlpha : -1.0F, middle ? middle->alphaMult : -1.0F);
		}
		fade->currentFade = 1.0F;
		fade->currentDecalFade = 1.0F;
	}

	// Every tick of the scene clock (not the guard's quarter second): keeps
	// every scene actor faded in.
	// ---- No camera fade on scene actors ----
	// Not in OStim (Skyrim's camera doesn't fade actors this way). Fallout
	// 4's camera finds what's between it and the player with a shape cast
	// (ID 2214659 on 1.11.240) and hands back the actor it's inside or
	// near, for the game to fade out: within fActorFadeOutLimit:Camera, plus
	// the actor's bound up to fActorFadeOutBoundMax:Camera. A scene's free
	// camera sits right by the actors, so one of them was faded out every
	// frame, faster than the scene guard could fade them back in (a 1.10.163
	// tester's log: "keeps being faded out", 586 times in 10 seconds, and
	// the male actor never seen). With both at 0 the cast's distance check
	// never passes, so nothing is faded: set while any scene runs, the
	// player's own values back when the last one ends.
	struct CameraFadeSetting
	{
		const char* name;
		float       own = 0.0F;
	};
	CameraFadeSetting g_cameraFade[]{ { "fActorFadeOutLimit:Camera" }, { "fActorFadeOutBoundMax:Camera" } };
	bool              g_cameraFadeOff = false;

	void SetCameraFadeOff(bool a_off)
	{
		if (a_off == g_cameraFadeOff) {
			return;
		}
		const auto ini = RE::INISettingCollection::GetSingleton();
		if (!ini) {
			return;
		}
		for (auto& setting : g_cameraFade) {
			const auto found = ini->GetSetting(setting.name);
			if (!found || found->GetType() != RE::Setting::SETTING_TYPE::kFloat) {
				REX::WARN("Scene camera: {} not found; the camera may still fade scene actors", setting.name);
				continue;
			}
			if (a_off) {
				setting.own = found->GetFloat();
				found->SetFloat(0.0F);
			} else {
				found->SetFloat(setting.own);
			}
		}
		g_cameraFadeOff = a_off;
		REX::INFO("Scene camera: actor fade {} ({} {:.0f}, {} {:.0f})", a_off ? "off while scenes run" : "back on", g_cameraFade[0].name, g_cameraFade[0].own,
			g_cameraFade[1].name, g_cameraFade[1].own);
	}

	void KeepFadedIn()
	{
		SetCameraFadeOff(!g_activeScenes.empty());
		if (!g_guardScenes) {
			return;
		}
		for (const auto& active : g_activeScenes) {
			for (const auto id : active.actors) {
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
					RestoreFade(actor, active.sceneID);
				}
			}
		}
	}

	// The shake watch (bCollisionWatch): every frame, where each scene actor
	// is (the reference) and where they're drawn (their 3D root), against
	// their spot. Every 2 s (10 times a scene), a line with how far off they
	// are and how much they move frame to frame, and how often that movement
	// turns back on itself: a shake is a back-and-forth every frame (the
	// owner's wall scenes, 6 November, about 2 pixels each frame with the
	// camera still). A reference that turns back is being moved by
	// something fighting the lock (the animation's root motion, collision);
	// only the drawn root turning back is the animation itself.
	void WatchShaking(float a_seconds)
	{
		struct Track
		{
			RE::NiPoint3  lastLoc, lastRoot, lastLocStep, lastRootStep;
			bool          started = false;
			int           frames = 0, locTurns = 0, rootTurns = 0, lines = 0;
			float         locStepMax = 0.0F, rootStepMax = 0.0F, locOffMax = 0.0F, rootOffMax = 0.0F, time = 0.0F;
			std::string   scene;
		};
		static std::unordered_map<std::uint32_t, Track> tracks;
		if (!g_settings.collisionWatch) {
			tracks.clear();
			return;
		}
		auto turned = [](const RE::NiPoint3& a, const RE::NiPoint3& b) {
			// Two frame steps pointing opposite ways, both over a hundredth of a unit.
			const float la = a.Length(), lb = b.Length();
			return la > 0.01F && lb > 0.01F && (a.x * b.x + a.y * b.y + a.z * b.z) < -0.5F * la * lb;
		};
		std::unordered_set<std::uint32_t> seen;
		for (const auto& active : g_activeScenes) {
			for (std::size_t role = 0; role < active.actors.size(); ++role) {
				const auto id = active.actors[role];
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				const auto root = actor ? actor->Get3D() : nullptr;
				if (!root) {
					continue;
				}
				seen.insert(id);
				auto& t = tracks[id];
				if (t.scene != active.sceneID) {
					t = Track{};
					t.scene = active.sceneID;
				}
				const auto spot = SpotOf(active, role).position;
				const auto loc = actor->data.location;
				const auto drawn = root->world.translate;
				if (t.started) {
					const auto locStep = loc - t.lastLoc;
					const auto rootStep = drawn - t.lastRoot;
					t.locTurns += turned(locStep, t.lastLocStep) ? 1 : 0;
					t.rootTurns += turned(rootStep, t.lastRootStep) ? 1 : 0;
					t.locStepMax = std::max(t.locStepMax, locStep.Length());
					t.rootStepMax = std::max(t.rootStepMax, rootStep.Length());
					t.lastLocStep = locStep;
					t.lastRootStep = rootStep;
				}
				t.started = true;
				t.lastLoc = loc;
				t.lastRoot = drawn;
				t.locOffMax = std::max(t.locOffMax, loc.GetDistance(spot));
				t.rootOffMax = std::max(t.rootOffMax, drawn.GetDistance(spot));
				++t.frames;
				t.time += a_seconds;
				if (t.time >= 2.0F) {
					if (t.lines < 10) {
						++t.lines;
						REX::INFO("Shake watch: {:08X} in \"{}\", {} frames: reference up to {:.2f} off its spot, steps up to {:.2f}, {} turn(s) back; drawn root up to {:.2f} off, steps up to {:.2f}, {} turn(s) back",
							id, active.sceneID, t.frames, t.locOffMax, t.locStepMax, t.locTurns, t.rootOffMax, t.rootStepMax, t.rootTurns);
					}
					const auto scene = t.scene;
					const int  lines = t.lines;
					t = Track{};
					t.scene = scene;
					t.lines = lines;
				}
			}
		}
		std::erase_if(tracks, [&](const auto& a_entry) { return !seen.contains(a_entry.first); });
	}

	void GuardScenes(float a_seconds)
	{
		static float clock = 0.0F;
		if (!g_guardScenes || (clock += a_seconds) < 0.25F) {
			return;
		}
		clock = 0.0F;
		const auto now = std::chrono::steady_clock::now();
		for (auto& active : g_activeScenes) {
			const auto scene = SceneRegistry::Find(active.sceneID);
			if (!scene) {
				continue;
			}
			const bool settled = active.remaining < 0.0F && !scene->IsTransition() && now - active.lastPlayed > std::chrono::milliseconds(1500);
			const RE::TESIdleForm* intruder = nullptr;
			std::uint32_t          intruded = 0;
			bool                   forceReplay = false;
			// For the "an NPC didn't follow the scene change" reports: a second
			// after each play, what each NPC is doing, once (a WARN when it
			// isn't on one of the scene's idles or is talking: a companion's
			// comment can hold its graph in a dialogue animation).
			const bool checkNow = !active.checkedAfterPlay && now - active.lastPlayed > std::chrono::seconds(1);
			if (checkNow) {
				active.checkedAfterPlay = true;
			}
			for (const auto id : active.actors) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				Undress::Enforce(actor);  // what came off stays off (the player too)
				if (actor) {
					LogVisibility(actor, active.sceneID);
				}
				if (IsPlayer(actor)) {
					ShowPlayerBody(actor, active.sceneID);
					// As OStim's lock: the player's weapon put away. Not in
					// combat only, as for NPCs below.
					if (auto& kicks = g_holsterKicks[id]; kicks.count < 3 && actor->GetWeaponMagicDrawn() && !actor->IsInCombat() && now - kicks.last > std::chrono::seconds(3)) {
						kicks.last = now;
						++kicks.count;
						if (SheatheNow(actor, "scene guard")) {
							forceReplay = true;
						}
					}
				}
				if (actor) {
					HoldGravity(actor);  // see "No gravity in scenes"
				}
				// Without Getbhk since 0.5.0 (it crashed the owner's game from
				// here, 1 November): see WatchCollisions. Only in the first
				// seconds after each scene plays (from 1.5 to 8 s), not all
				// scene long: the guard runs alongside the game's own actor
				// and physics jobs, and two crashes on 7 November were Havok's
				// thread check tripping in another NPC's movement
				// (forceBodyPairCollisions) well into a scene. What touches
				// the actors does so once they're placed.
				if (actor && settled && now - active.lastPlayed < std::chrono::seconds(8)) {
					WatchCollisions(actor, active.sceneID);
				}
				if (!actor || IsPlayer(actor)) {
					continue;
				}
				const auto process = actor->currentProcess;
				const auto data = process ? process->middleHigh : nullptr;
				if (!data) {
					continue;
				}
				data->packageIdleTimer = 30.0F;  // no package idle for a while; renewed every check
				if (checkNow) {
					const auto idle = data->currentIdle;
					const bool ours = idle && std::ranges::any_of(scene->speeds, [&](const auto& a_idles) { return std::ranges::find(a_idles, idle) != a_idles.end(); });
					const bool talking = actor->IsTalking();
					const auto what = std::format("idle {:08X}{}, {}talking, weapon {}", idle ? idle->GetFormID() : 0, ours ? " (the scene's)" : idle ? " (NOT the scene's)" : " (none)",
						talking ? "" : "not ", actor->GetWeaponMagicDrawn() ? "out" : "away");
					// No current idle is normal: the process doesn't keep one for
					// idles played this way (every check in the owner's log).
					if ((idle && !ours) || talking) {
						REX::WARN("Scene check: {:08X} in \"{}\" a second after it played: {}", id, active.sceneID, what);
					} else {
						REX::INFO("Scene check: {:08X} in \"{}\": {}", id, active.sceneID, what);
					}
				}
				// Scene lock watchdog: moved off the spot (or turned) anyway?
				if (g_lockScenes && now - active.lastRelock > std::chrono::seconds(1)) {
					const auto  role = static_cast<std::size_t>(std::ranges::find(active.actors, id) - active.actors.begin());
					const auto  spot = SpotOf(active, role);
					const float drift = actor->data.location.GetDistance(spot.position);
					float       turn = std::fmod(std::fabs(actor->data.angle.z - spot.heading), 2.0F * PI_F);
					turn = std::fmin(turn, 2.0F * PI_F - turn);
					turn = std::fmin(turn, 2.0F * PI_F - turn);
					if (drift > 4.0F || turn > 0.06F) {
						REX::INFO("Scene lock: {:08X} in \"{}\" was {:.1f} units / {:.1f} deg off its spot, locking it again", id, active.sceneID, drift,
							turn * 180.0F / PI_F);
						active.lastRelock = now;
						LockScene(active);
					}
				}
				// A weapon out with no fight going on (an NPC drawing on an
				// alarm or a threat that's gone): put away and the scene played
				// again. In real combat (an attack on the scene), it's left to
				// the game: a scene can be broken by an attack.
				// At most 3 tries a scene, 3 s apart: an actor whose weapon stays
				// "drawn" whatever is done (a companion's graph mid-scene, in the
				// owner's log) was put away and the scene replayed every second,
				// restarting its animation each time.
				if (auto& kicks = g_holsterKicks[id]; kicks.count < 3 && actor->GetWeaponMagicDrawn() && !actor->IsInCombat()) {
					if (now - kicks.last > std::chrono::seconds(3)) {
						kicks.last = now;
						if (++kicks.count == 3) {
							REX::WARN("Scene guard: {:08X} still has a weapon out after 3 tries; leaving it be for this scene", id);
						}
						REX::INFO("Scene guard: {:08X} in \"{}\" drew a weapon (not in combat): putting it away", id, active.sceneID);
						SheatheNow(actor, "scene guard");
						if (g_vm) {
							RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
							Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "PutWeaponAwayByID"sv, callback, static_cast<std::int32_t>(id));
						}
						forceReplay = true;
					}
					continue;
				}
				// The same furniture again after StopInteractingQuick took them
				// out of it: the process keeps a stale handle (the owner's Ana
				// and a garden idle marker, 4 November: "taken out" three times,
				// she never went back, and each replay restarted the scene).
				// Not replayed for it; the lock holds them on their spot.
				const auto inFurniture = FurnitureOf(actor);
				if (auto& kicks = g_guardKicks[id]; inFurniture && Compat::CanStopInteractingQuick() && inFurniture->GetFormID() == kicks.takenOutOf) {
					if (!kicks.staleLogged) {
						kicks.staleLogged = true;
						REX::INFO("Scene guard: {:08X} still shows {:08X}, which they were taken out of: a stale handle, left be", id, kicks.takenOutOf);
					}
				} else if (inFurniture && kicks.count < 3 && now - kicks.last > std::chrono::seconds(5)) {
					kicks.last = now;
					if (++kicks.count == 3) {
						REX::WARN("Scene guard: {:08X} keeps going back into furniture; leaving it be for this scene", id);
					}
					REX::INFO("Scene guard: {:08X} in \"{}\" went into furniture or an idle marker", id, active.sceneID);
					LeaveFurniture(actor, false);
					forceReplay = true;
					continue;
				}
				const auto current = data->currentIdle;
				if (!settled || !current || intruder) {
					continue;
				}
				const bool ours = std::ranges::any_of(scene->speeds, [&](const auto& a_idles) {
					return std::ranges::find(a_idles, current) != a_idles.end();
				});
				if (!ours) {
					intruder = current;
					intruded = id;
				}
			}
			if (forceReplay) {
				PlayOnActiveScene(active, active.sceneID, active.speed);
			} else if (intruder) {
				REX::INFO("Scene guard: {:08X} in \"{}\" was playing a game idle ({:08X} \"{}\"), playing the scene again", intruded, active.sceneID,
					intruder->GetFormID(), intruder->GetFormEditorID() ? intruder->GetFormEditorID() : "");
				PlayOnActiveScene(active, active.sceneID, active.speed);
			}
		}
	}

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
	ClimaxShake  g_climaxShake;
	std::mt19937 g_shakeRandom{ std::random_device{}() };

	RE::FreeCameraState* FreeCamera()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->QCameraEquals(RE::CameraState::kFree)) {
			return nullptr;
		}
		return camera->GetState<RE::FreeCameraState>().get();
	}

	void UpdateClimaxShake()
	{
		auto& shake = g_climaxShake;
		if (!shake.active) {
			return;
		}
		const auto free = FreeCamera();
		if (!free) {
			// The free camera is gone (the scene ended): nothing to put back.
			shake = {};
			return;
		}
		const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - shake.start).count() / shake.duration;
		RE::NiPoint3 offset{};
		float        turnX = 0.0F;
		float        turnY = 0.0F;
		if (t < 1.0F) {
			// Strong at once, fading out quickly.
			const float fade = (1.0F - t) * (1.0F - t);
			std::uniform_real_distribution<float> unit(-1.0F, 1.0F);
			const float move = 0.6F * shake.strength * fade;     // game units
			const float turn = 0.0025F * shake.strength * fade;  // radians
			offset = { unit(g_shakeRandom) * move, unit(g_shakeRandom) * move, unit(g_shakeRandom) * move };
			turnX = unit(g_shakeRandom) * turn;
			turnY = unit(g_shakeRandom) * turn;
		} else {
			shake.active = false;
		}
		free->translation += offset - shake.moved;
		free->rotation.x += turnX - shake.turnedX;
		free->rotation.y += turnY - shake.turnedY;
		shake.moved = offset;
		shake.turnedX = turnX;
		shake.turnedY = turnY;
	}

	// Slow motion at a climax, as OStim's SetSlowMoOrgasms: the game at 0.3x
	// for 2.5 s (OStim runs the console's setGameSpeed; here the game timer's
	// global time multiplier, what Fallout 4's "sgtm" sets). A climax during
	// it starts the 2.5 s over. Main thread.
	std::atomic<std::uint32_t> g_slowMoGeneration = 0;
	bool                       g_slowMo = false;

	void EndSlowMotion()
	{
		if (!g_slowMo) {
			return;
		}
		g_slowMo = false;
		if (const auto timer = RE::BSTimer::GetSingleton()) {
			timer->SetGlobalTimeMultiplier(1.0F, true);
		}
	}

	void StartSlowMotion()
	{
		const auto timer = RE::BSTimer::GetSingleton();
		if (!timer) {
			return;
		}
		timer->SetGlobalTimeMultiplier(0.3F, true);
		g_slowMo = true;
		const auto generation = ++g_slowMoGeneration;
		Scheduler::AfterOnMain(std::chrono::milliseconds(2500), [generation]() {
			if (g_slowMoGeneration == generation) {
				EndSlowMotion();
			}
		});
	}

	// The player's climax effects: shake, blur, glow, rumble, slow motion
	// (Excitement::Config). Slow motion only in scenes with the player, as
	// OStim's (its player thread); the rest also in a scene being watched.
	// a_playerClimax: the player is the one climaxing; OStim blurs only then
	// (its OStimNutEffect), the other effects for anyone's climax.
	void PlayClimaxEffects(bool a_withPlayer, bool a_playerClimax)
	{
		const auto& config = Excitement::Settings();
		const bool  freeCamera = FreeCamera() != nullptr;
		if (config.slowMo && a_withPlayer) {
			StartSlowMotion();
		}
		// OStim's SetUseScreenShake gates every shake (off by default).
		const bool shakeOn = config.useScreenShake && config.shake > 0.0F;
		if (shakeOn && freeCamera) {
			if (g_climaxShake.active) {
				// Already shaking: restart it from where the camera is now.
				g_climaxShake.start = std::chrono::steady_clock::now();
			} else {
				g_climaxShake = { true, std::chrono::steady_clock::now(), 0.8F, config.shake };
			}
			StartAutoplayTicks();
		}
		if (config.blurOn && config.blur > 0.0F && a_playerClimax) {
			// Depth of field with no in-focus range: everything blurs, for
			// a moment.
			RE::ImageSpaceModifierInstanceDOF::Trigger(0.0F, 0.0F, 0.0F, 0.0F,
				RE::ImageSpaceModifierInstanceDOF::DepthOfFieldMode::kFrontBack, std::min(config.blur, 1.0F), 0.5F);
		}
		if (config.flash > 0.0F) {
			HUD::PlayClimax(std::min(config.flash, 1.0F), config.glowSize);
		}
		// The normal cameras' shake and the rumble go through Papyrus.
		const bool papyrusShake = shakeOn && !freeCamera;
		if ((papyrusShake || config.rumble) && g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "ClimaxEffects"sv, callback, papyrusShake ? config.shake : 0.0F, config.rumble);
		}
	}

	// Fires an event (docs/EVENTS.md) in a_active: a_actor did it to
	// a_target, a_performer moving (roles; -1 = none). Its sound,
	// stimulation and reactions come from Events; the camera shake and
	// rumble only in a scene the player is in or watching (OStim shakes for
	// any thread). Then FourStim_OnEvent for registered scripts. Main thread.
	bool FireEvent(ActiveScene& a_active, const std::string& a_event, int a_actor, int a_target, int a_performer)
	{
		const auto effects = Events::Fire(a_event, a_active.actors, a_actor, a_target, a_performer);
		if (!effects.found) {
			return false;
		}
		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		const auto focused = GetPlayerScene();
		const bool watched = focused.Active() && focused.role0 == a_active.actors[0];
		if (withPlayer || watched) {
			const auto& config = Excitement::Settings();
			// SetUseScreenShake / SetUseRumble, as OStim's CameraUtil and
			// ControlUtil check (4Stim: fClimaxShake 0 turns shakes off).
			const float shake = config.useScreenShake && config.shake > 0.0F && effects.shakeDuration > 0.0F ? effects.shakeStrength : 0.0F;
			const float rumble = config.rumble && effects.rumbleDuration > 0.0F ? effects.rumbleStrength : 0.0F;
			const bool  freeCamera = FreeCamera() != nullptr;
			if (shake > 0.0F && freeCamera) {
				if (g_climaxShake.active) {
					// Already shaking: start over from where the camera is now
					// (its offset is undone as it goes).
					g_climaxShake.start = std::chrono::steady_clock::now();
					g_climaxShake.duration = std::max(effects.shakeDuration, 0.05F);
					g_climaxShake.strength = shake;
				} else {
					g_climaxShake = { true, std::chrono::steady_clock::now(), std::max(effects.shakeDuration, 0.05F), shake };
				}
				StartAutoplayTicks();
			}
			const float papyrusShake = freeCamera ? 0.0F : shake;
			if ((papyrusShake > 0.0F || rumble > 0.0F) && g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EventEffects"sv, callback, papyrusShake, effects.shakeDuration, rumble, effects.rumbleDuration);
			}
		}
		auto role = [&](int a_index) {
			return a_index >= 0 && static_cast<std::size_t>(a_index) < a_active.actors.size() ? a_active.actors[static_cast<std::size_t>(a_index)] : 0u;
		};
		SceneEvents::EventFired(a_event, role(a_actor), role(a_target), role(a_performer), a_active.actors, a_active.sceneID, a_active.threadID);
		return true;
	}

	// Hands a running scene's current scene and speed to the excitement
	// system (rates come from its actions), and keeps the clock running.
	void RefreshExcitement(const ActiveScene& a_scene)
	{
		const auto scene = SceneRegistry::Find(a_scene.sceneID);
		if (!scene) {
			return;
		}
		// The excitement rate by sex: a futa's the man's with futaUseMaleExcitement (OStim's).
		std::vector<SceneRegistry::Sex> sexes;
		for (const auto id : a_scene.actors) {
			sexes.push_back(ActorProperties::MaleExcitement(RE::TESForm::GetFormByID<RE::Actor>(id)) ? SceneRegistry::Sex::kMale : SceneRegistry::Sex::kFemale);
		}
		Excitement::Enter(a_scene.actors, sexes, *scene, a_scene.speed);
		for (const auto id : a_scene.actors) {
			Excitement::SetThreadStalled(id, a_scene.stallClimax);  // actors new to the thread too
		}
		// OStim's playbackSpeed for this speed.
		if (a_scene.speed >= 0 && static_cast<std::size_t>(a_scene.speed) < scene->speedInfo.size()) {
			PlaybackSpeed::Set(a_scene.actors, std::clamp(scene->speedInfo[static_cast<std::size_t>(a_scene.speed)].playbackSpeed, 0.05F, 10.0F));
		} else {
			PlaybackSpeed::Set(a_scene.actors, 1.0F);
		}
		// Faces and voices follow the scene too (OStim's changeNode).
		const float relativeSpeed = scene->speeds.empty() ? 0.0F : static_cast<float>(a_scene.speed) / static_cast<float>(scene->speeds.size());
		for (std::size_t role = 0; role < a_scene.actors.size(); ++role) {
			Expressions::Enter(a_scene.actors[role], *scene, role, relativeSpeed);
			Sounds::Enter(a_scene.actors[role], *scene, role, a_scene.actors);
		}
		// Peaks and action sounds (OStim's node handler), and lights.
		const bool withPlayer = std::ranges::any_of(a_scene.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		Peaks::Enter(a_scene.actors, scene, a_scene.speed, withPlayer);
		if (withPlayer) {
			SexToys::Enter(a_scene.actors, scene, a_scene.speed);  // OStim drives toys from the player's thread
		}
		Lights::Enter(a_scene.actors);
		StartAutoplayTicks();
	}

	// Ends a running scene through Papyrus: the focused scene the way the
	// HUD's "End scene" does, any other without touching the camera.
	void EndActiveScene(ActiveScene& a_scene, std::string_view a_why)
	{
		if (!g_vm || a_scene.actors.empty()) {
			return;
		}
		a_scene.ending = true;
		REX::INFO("Scene \"{}\": ending, {}", a_scene.sceneID, a_why);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_scene.actors[0]) {
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndPlayerScene"sv, callback);
		} else {
			std::vector<std::int32_t> ids;
			for (const auto id : a_scene.actors) {
				ids.push_back(static_cast<std::int32_t>(id));
			}
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndSceneOf"sv, callback, ids);
		}
	}

	// a_actorID, in a_active, climaxes now (OStim's climaxInner): counts it,
	// a man drops to the slowest speed, other mods are told, the player sees
	// the effects, and the scene ends if the settings say so.
	// OStim's climaxInner: the climax itself.
	void ClimaxInner(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		const auto& config = Excitement::Settings();
		std::erase_if(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; });
		Sounds::HoldForClimax(a_actorID, false);
		Excitement::Climaxed(a_actorID, ActorProperties::FemalePostClimax(RE::TESForm::GetFormByID<RE::Actor>(a_actorID)));
		SexToys::Climax(a_actorID);
		// As OStim's setAutoModeToMainStage: any climax moves auto mode on
		// to its main stage, from foreplay too.
		if (a_active.autoMode.on && a_active.autoMode.stage != ActiveScene::Auto::Stage::kMain) {
			a_active.autoMode.stage = ActiveScene::Auto::Stage::kMain;
			a_active.autoMode.cooldown = std::min(a_active.autoMode.cooldown, 3.0F);
		}
		const int  times = Excitement::TimesClimaxed(a_actorID);
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actorID);
		const bool penis = ActorProperties::HasPenis(actor);        // OStim's hasSchlong
		const bool maleClimax = ActorProperties::MaleClimax(actor);  // a man's, or a futa's with futaUseMaleClimax
		const auto role = static_cast<std::size_t>(std::ranges::find(a_active.actors, a_actorID) - a_active.actors.begin());
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		REX::INFO("Climax: {:08X} (role {}) in \"{}\", climax {}", a_actorID, role, a_active.sceneID, times);

		// As OStim: the climax of someone with a penis drops the scene to its
		// slowest speed (still in the climax animation: the scene it goes back
		// to starts slowest).
		if (penis && scene) {
			if (scene->IsTransition()) {
				a_active.relativeSpeed = 0.0F;
			} else if (a_active.speed > 0) {
				PlayOnActiveScene(a_active, a_active.sceneID, 0);  // sends the speed event
			}
		}

		SceneEvents::Climaxed(a_actorID, a_active.actors, a_active.sceneID, times, a_active.threadID);
		if (scene) {
			ApiForms::Climaxed(a_actorID, a_active.actors, *scene, times, a_active.threadID == 0);
		}
		// As OStim: climaxing takes it out of them (its 250 Stamina; Fallout
		// 4's is Action Points).
		if (const auto values = RE::ActorValue::GetSingleton(); actor && values && values->actionPoints) {
			actor->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *values->actionPoints, -250.0F);
		}

		// OStim's playClimaxSound: their climax sound, and the others'
		// climaxCommentOther, their own climaxCommentSelf after it.
		Sounds::Climax(a_actorID, a_active.actors, IsPlayer(actor));

		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		const auto focused = GetPlayerScene();
		const bool watched = focused.Active() && focused.role0 == a_active.actors[0];
		if (withPlayer || watched) {
			PlayClimaxEffects(withPlayer, IsPlayer(actor));
		}

		// As OStim's climaxInner: the all / player / male / female rules in
		// every scene, and endNPCSceneOnOrgasm on top for scenes without the
		// player. Each climax that counts sets the end timer again.
		bool end = false;
		if (config.endOnAll) {
			end = std::ranges::all_of(a_active.actors, [](std::uint32_t id) { return Excitement::TimesClimaxed(id) > 0; });
		} else {
			end = (IsPlayer(actor) && config.endOnPlayer) ||
			      (maleClimax && config.endOnMale) ||
			      (!maleClimax && config.endOnFemale);
		}
		if (!withPlayer && config.endNPCScenes) {
			end = true;
		}
		if (end && (a_active.endIn < 0.0F || a_active.endIn < config.endDelay)) {
			a_active.endIn = config.endDelay;
		}
	}

	// OStim's climax: at once, or once the moan they're making has ended
	// (awaitingClimaxInner; ResolveAwaitingClimaxes checks).
	void ClimaxNow(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		if (Sounds::IsMakingSound(a_actorID)) {
			std::erase_if(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; });
			a_active.awaitingClimax.push_back({ a_actorID, a_active.sceneID, false, true });
			Sounds::HoldForClimax(a_actorID, true);  // no new moan or reaction meanwhile
			REX::INFO("Climax: {:08X} waits for their moan to end (as OStim)", a_actorID);
			return;
		}
		ClimaxInner(a_active, a_actorID);
	}

	// a_actorID, in a_active, reached 100 (OStim's orgasm): with a climax
	// animation for their role (SetAutoClimaxAnims), it plays and the climax
	// itself waits for the animation's 4StimClimax annotation (OStim's
	// OStimClimax), or for the scene to move on from it; otherwise they
	// climax at once.
	void HandleClimax(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		const auto& config = Excitement::Settings();
		if (std::ranges::any_of(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; })) {
			return;  // already on its way
		}
		const auto role = static_cast<std::size_t>(std::ranges::find(a_active.actors, a_actorID) - a_active.actors.begin());
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto climaxScene = scene && role < scene->actors.size() ? scene->actors[role].AutoTransition("climax") : std::string{};
		if (config.climaxScenes && scene && !scene->IsTransition() && !climaxScene.empty() && a_active.queuedScene.empty() && !a_active.sequence &&
			PlayOnActiveScene(a_active, climaxScene, -1)) {
			if (ActorProperties::HasPenis(RE::TESForm::GetFormByID<RE::Actor>(a_actorID))) {
				a_active.relativeSpeed = 0.0F;  // back at the slowest speed after it
			}
			ArmAutoplay(a_active);
			a_active.awaitingClimax.push_back({ a_actorID, a_active.sceneID, false });
			REX::INFO("Climax: {:08X} (role {}) plays \"{}\", climaxing at its 4StimClimax annotation or when it ends", a_actorID, role, a_active.sceneID);
			return;
		}
		ClimaxNow(a_active, a_actorID);
	}

	// Climaxes waiting on their climax animation: the annotation came, or
	// the scene moved on from the animation (OStim climaxes on the next
	// scene change). Called from the scene clock.
	void ResolveAwaitingClimaxes(ActiveScene& a_active)
	{
		std::vector<std::uint32_t> due;
		std::vector<std::uint32_t> soundDone;
		for (const auto& entry : a_active.awaitingClimax) {
			if (entry.afterSound) {
				if (!Sounds::IsMakingSound(entry.actor)) {
					soundDone.push_back(entry.actor);
				}
			} else if (entry.annotated || _stricmp(entry.sceneID.c_str(), a_active.sceneID.c_str()) != 0) {
				due.push_back(entry.actor);
			}
		}
		for (const auto id : soundDone) {
			ClimaxInner(a_active, id);
		}
		for (const auto id : due) {
			ClimaxNow(a_active, id);
		}
	}

	void AutoplayTick()
	{
		g_autoplayTickQueued = false;
		const auto now = std::chrono::steady_clock::now();
		float      elapsed = std::chrono::duration<float>(now - g_autoplayLast).count();
		g_autoplayLast = now;
		elapsed = std::min(elapsed, 0.25F);  // a hitch or a load screen doesn't skip a whole step
		if (g_slowMo) {
			elapsed *= 0.3F;  // the animations are in slow motion too
		}

		const auto ui = RE::UI::GetSingleton();
		const bool paused = ui && ui->menuMode > 0;
		static bool wasPaused = false;
		if (paused != wasPaused) {
			wasPaused = paused;
			REX::INFO("Scene clock: {} (menu mode {})", paused ? "paused" : "running", ui ? static_cast<int>(ui->menuMode) : -1);
		}
		if (!paused) {
			// AdvanceAutoplay and HandleClimax can't add or remove running
			// scenes, so the list is safe to walk while they run.
			for (auto& active : g_activeScenes) {
				if (active.remaining < 0.0F) {
					continue;
				}
				active.remaining -= elapsed;
				if (active.remaining <= 0.0F) {
					AdvanceAutoplay(active);
				}
			}
			// The animation files' annotation cues, timed here (AnimCues.h).
			for (const auto& due : AnimCues::Tick(elapsed, [](std::uint32_t a_id) { return PlaybackSpeed::RateOf(RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_id)); })) {
				HandleTimedCue(due);
			}
			for (const auto id : Excitement::Tick(elapsed)) {
				if (const auto active = FindActiveScene(id)) {
					HandleClimax(*active, id);
				} else {
					Excitement::Climaxed(id, ActorProperties::FemalePostClimax(RE::TESForm::GetFormByID<RE::Actor>(id)));
				}
			}
			for (auto& active : g_activeScenes) {
				ResolveAwaitingClimaxes(active);
			}
			Sounds::Tick(elapsed);
			Peaks::Tick(elapsed);
			SexToys::Tick(elapsed);
			Expressions::Tick(elapsed);
			UpdateClimaxShake();
			KeepFadedIn();
			WatchShaking(elapsed);
			GuardScenes(elapsed);
			PruneScenePackage(elapsed);
			Undress::Tick(elapsed);
			EquipObjects::Tick(elapsed);
			for (auto& active : g_activeScenes) {
				SpeedControlTick(active, elapsed);
				AutoModeTick(active, elapsed);
			}
			EndBrokenScenes(elapsed);
			// Scenes ending after a climax, or NPC scenes whose time is up
			// (OStim's NPCSceneDuration). Ending goes through Papyrus, which
			// removes the scene later (TrackSceneStop).
			for (auto& active : g_activeScenes) {
				if (active.stopTimer >= 0.0F) {
					active.stopTimer -= elapsed;
					if (active.stopTimer < 0.0F) {
						active.stopTimer = -1.0F;
						if (active.endIn < 0.0F) {
							EndActiveScene(active, "its time is up (NPCSceneDuration)");
						}
						continue;
					}
				}
				if (active.endIn < 0.0F) {
					continue;
				}
				active.endIn -= elapsed;
				if (active.endIn <= 0.0F) {
					active.endIn = -1.0F;
					EndActiveScene(active, "after a climax");
				}
			}
		}
		Lights::Tick();  // lights follow their bones, paused or not
		PenisBend::Tick();
		Erection::Tick();
		Physics::Tick();
		EyeLook::Tick();
		{
			std::vector<std::uint32_t> everyone;
			for (const auto& active : g_activeScenes) {
				everyone.insert(everyone.end(), active.actors.begin(), active.actors.end());
			}
			Actor3D::Tick(everyone);  // a rebuilt actor gets the scene's changes back
		}
		ApiForms::Tick(elapsed);  // the excitement faction's rank
		const bool waiting = !g_activeScenes.empty();  // faces and voices run as long as any scene does
		if (!waiting && !g_climaxShake.active) {
			g_autoplayTicking = false;
		}
	}

	// As OStim's Thread::loop (Core/Thread.cpp): a scene ends when anyone in
	// it dies or is in combat, and a scene without the player ends when its
	// first actor isn't in the player's cell. Four times a second. Main thread.
	// Fallout4.esm's Health actor value (ActorValueInfo 000002D4), by form
	// ID rather than the game's actor value list (no new address needed).
	RE::ActorValueInfo* g_healthAV = nullptr;

	void EndBrokenScenes(float a_seconds)
	{
		static bool healthLooked = false;
		if (!healthLooked) {
			healthLooked = true;
			g_healthAV = RE::TESForm::GetFormByID<RE::ActorValueInfo>(0x000002D4);
			if (!g_healthAV) {
				REX::WARN("Scene end: Fallout4.esm's Health actor value (000002D4) not found: scenes don't end when someone is hurt");
			}
		}
		static float wait = 0.0F;
		if ((wait -= a_seconds) > 0.0F) {
			return;
		}
		wait = 0.25F;
		const auto player = RE::PlayerCharacter::GetSingleton();
		for (auto& active : g_activeScenes) {
			if (active.ending || active.actors.empty()) {
				continue;
			}
			std::string why;
			bool        withPlayer = false;
			active.health.resize(active.actors.size(), -1.0F);
			for (std::size_t role = 0; role < active.actors.size(); ++role) {
				const auto id = active.actors[role];
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				if (!actor) {
					continue;
				}
				withPlayer = withPlayer || actor == player;
				// 4Stim's (bEndOnDamage): hurt. Actors in a scene don't start
				// fights, so OStim's combat check alone let a scene go on
				// while its actors were shot (the owner's Legendary Raider).
				// Health only goes down from harm; it coming back up is regeneration.
				const float health = g_endOnDamage && g_healthAV ? actor->GetActorValue(*g_healthAV) : -1.0F;
				const float before = active.health[role];
				active.health[role] = health;
				if (actor->IsDead(false)) {
					why = std::format("{:08X} died", id);
				} else if (actor->IsInCombat()) {
					why = std::format("{:08X} is in combat", id);
				} else if (health >= 0.0F && before >= 0.0F && health < before - 0.5F) {
					why = std::format("{:08X} was hurt ({:.0f} health lost)", id, before - health);
				}
				if (!why.empty()) {
					break;
				}
			}
			if (why.empty() && !withPlayer && player) {
				const auto first = RE::TESForm::GetFormByID<RE::Actor>(active.actors.front());
				// OStim (Thread::loop) ends an NPC scene when its first actor
				// and the player aren't in the same cell. Outdoors that's
				// wrong for Fallout 4: exterior cells are 4096 units, so a
				// player standing beside an NPC scene in a settlement is often
				// one cell over, and the owner's NPC scenes ended within
				// seconds (5 November). Indoors 4Stim compares cells as OStim
				// does; outdoors, same worldspace and the scene's cell still
				// attached (in the loaded grid around the player) and its
				// actor's 3D still there.
				const auto sceneCell = first ? first->parentCell : nullptr;
				const auto playerCell = player->parentCell;
				if (first && sceneCell != playerCell) {
					const bool outdoors = sceneCell && playerCell && sceneCell->IsExterior() && playerCell->IsExterior();
					if (!outdoors) {
						why = "the player isn't in its cell";
					} else if (sceneCell->worldSpace != playerCell->worldSpace) {
						why = "the player isn't in its worldspace";
					} else if (!sceneCell->IsAttached() || !first->Get3D()) {
						why = "its cell isn't loaded around the player any more";
					}
				}
			}
			if (!why.empty()) {
				active.ending = true;
				active.interrupted = true;
				EndActiveScene(active, why);
			}
		}
	}

	// Main thread. Starts the ticker thread if it isn't running.
	void StartAutoplayTicks()
	{
		if (g_autoplayTicking.exchange(true)) {
			return;
		}
		g_autoplayLast = std::chrono::steady_clock::now();
		// Every 10 ms while scenes run (the scheduler's thread, no thread of its own).
		Scheduler::Every(std::chrono::milliseconds(10), []() {
			if (g_autoplayTicking && !g_autoplayTickQueued.exchange(true)) {
				F4SE::GetTaskInterface()->AddTask(AutoplayTick);
			}
			return g_autoplayTicking.load();
		});
	}

	// Plays a_sequence from its first entry on the running scene a_actorID
	// is in. Main thread.
	bool StartSequenceOnScene(std::uint32_t a_actorID, const std::shared_ptr<const SceneRegistry::Sequence>& a_sequence)
	{
		const auto active = FindActiveScene(a_actorID);
		if (!active) {
			REX::WARN("Sequence \"{}\": {:08X} isn't in a scene", a_sequence->id, a_actorID);
			return false;
		}
		if (active->actors.size() != a_sequence->actorCount) {
			REX::WARN("Sequence \"{}\" is for {} actor(s), the scene has {}", a_sequence->id, a_sequence->actorCount, active->actors.size());
			return false;
		}
		const auto& first = a_sequence->entries.front();
		if (!PlayOnActiveScene(*active, first.scene, first.speed)) {
			return false;
		}
		active->sequence = a_sequence;
		active->step = 0;
		REX::INFO("Sequence \"{}\": started on the running scene ({} scene(s))", a_sequence->id, a_sequence->entries.size());
		HUD::OnFocusedSceneChanged();  // no navigation during it, as OStim's scene menu
		ArmAutoplay(*active);
		return true;
	}

	// Queues a_sequence for the next scene these actors start (its first
	// scene), from any thread.
	void QueueSequence(std::shared_ptr<const SceneRegistry::Sequence> a_sequence, std::vector<std::uint32_t> a_actors)
	{
		std::scoped_lock lock(g_pendingSequenceLock);
		g_pendingSequence.Put(a_actors, PendingSequence{ std::move(a_sequence), a_actors });
	}

	bool IsPlayer(const RE::Actor* a_actor)
	{
		return a_actor && a_actor == RE::PlayerCharacter::GetSingleton();
	}


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

	// Teleports a_actor to a_spot (on the main thread). Natively, not through
	// Papyrus SetPosition: that left the player where they stood.
	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot)
	{
		if (!a_actor) {
			return false;
		}
		const auto id = a_actor->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, a_spot]() {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				return;
			}
			actor->SetPosition(a_spot.position, true);
			FaceHeading(actor, a_spot.heading);
			g_placedSpots[id] = { a_spot.position, a_spot.heading, std::chrono::steady_clock::now() };
			const auto& now = actor->data.location;
			REX::INFO("Furniture: {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg; now at ({:.1f}, {:.1f}, {:.1f})",
				id, a_spot.position.x, a_spot.position.y, a_spot.position.z, a_spot.heading * 180.0F / PI_F, now.x, now.y, now.z);
		});
		return true;
	}

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
	std::atomic<std::int64_t> g_failedStartAt = 0;  // steady clock, ms

	std::int64_t NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void ReleaseFailedStart(std::vector<std::uint32_t> a_ids, std::string_view a_why)
	{
		std::erase(a_ids, 0u);
		if (a_ids.empty()) {
			return;
		}
		REX::WARN("Start failed ({}): letting {} actor(s) go again", a_why, a_ids.size());
		DropStartingThreadOf(a_ids, a_why);
		g_failedStartAt = NowMs();
		g_focusNextScene = false;
		Scheduler::AfterOnMain(std::chrono::milliseconds(1500), [ids = std::move(a_ids)]() {  // Papyrus's start returns first
			// Started after all (another try): leave it to the scene's own end.
			if (std::ranges::any_of(ids, [](std::uint32_t a_id) { return FindActiveScene(a_id) != nullptr; }) || !g_vm) {
				return;
			}
			std::vector<std::int32_t> args;
			for (const auto id : ids) {
				args.push_back(static_cast<std::int32_t>(id));
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "ReleaseFailedStart"sv, callback, args);
		});
	}

	void StartScene(std::monostate, RE::Actor* a_actor, RE::Actor*, std::string a_sceneID)
	{
		if (!a_actor) {
			REX::WARN("StartScene called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene) {
			REX::WARN("StartScene: no scene \"{}\" (check the Scenes folder and this log's load summary)", a_sceneID);
			ReleaseFailedStart({ a_actor->GetFormID() }, "no such scene");
			return;
		}
		const int  speed = StartSpeed(*scene);  // OStim: a new thread starts at the scene's defaultSpeed
		const auto idle = scene->speeds[speed][0];
		const auto sceneID = scene->id;

		REX::INFO("StartScene: {:08X} <- scene \"{}\" speed {} (idle {:08X})", a_actor->GetFormID(), scene->id, speed + 1, idle->GetFormID());
		PlaceOnPendingFurniture({ a_actor });

		// Papyrus calls arrive on a script thread; run engine animation code on the main thread.
		F4SE::GetTaskInterface()->AddTask([a_actor, idle, sceneID, speed]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StartScene: actor has no AI process");
				ReleaseFailedStart({ a_actor->GetFormID() }, "no AI process");
				return;
			}
			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StartScene: PlayIdle returned {}", ok);
			if (ok) {
				CuePlayed(a_actor->GetFormID(), idle, sceneID);
			}
			if (!ok) {
				ReleaseFailedStart({ a_actor->GetFormID() }, "the game refused the animation");
			}
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor);
			if (ok) {
				TrackSceneStart({ a_actor->GetFormID() }, sceneID, speed);
				if (focus) {
					PlayerScene focused{ a_actor->GetFormID(), 0, sceneID, speed };
					if (const auto active = FindActiveScene(a_actor->GetFormID())) {
						focused.furniture = active->furnitureType;
					}
					SetPlayerScene(std::move(focused));
				}
			}
		});
	}

	void StopScene(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("StopScene called with a null actor");
			return;
		}

		REX::INFO("StopScene: {:08X}", a_actor->GetFormID());

		// FO4.esm's IdleStop (000E9855), played the same way as a pose.
		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopScene: could not find IdleStop in Fallout4.esm");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor, idle]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StopScene: actor has no AI process");
				return;
			}

			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StopScene: PlayIdle(IdleStop) returned {}", ok);
			StopAgainIfJustPlayed({ a_actor->GetFormID() }, idle);
			ClearFocusIfIn(a_actor->GetFormID());
			TrackSceneStop(a_actor->GetFormID());
		});
	}

	// ---- Spike 4: lock / unlock an actor in place ----
	// Uses CallActorMethod to invoke the engine's own Papyrus-native
	// Actor.SetRestrained / Actor.EnableAI, confirmed from Actor.psc:
	//   bool Function SetRestrained(bool abRestrained = true) native
	//   Function EnableAI(bool abEnable = true, bool abPauseVoice = false) native

	// Blocks the Activate/Talk prompt and stops dialogue (including an
	// automatic proximity greet, which needs no button press and can fire
	// the instant an actor is moved near the player -- confirmed: a full
	// dialogue menu opened on its own within ~1s of MoveActorTo, well
	// before LockActor used to apply this same block ~3s later). Call
	// this BEFORE MoveActorTo brings the actor near the
	// player, not as part of locking them in place afterward.
	void SuppressInteraction(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("SuppressInteraction called with a null actor");
			return;
		}

		const auto blockOk = CallActorMethod(a_actor, "ObjectReference"sv, "BlockActivation"sv, true, true);
		const auto dialogueOk = CallActorMethod(a_actor, "Actor"sv, "AllowPCDialogue"sv, false);
		// Before anything moves them: where to put them back afterwards.
		{
			const auto  id = a_actor->GetFormID();
			const auto  pos = a_actor->data.location;
			const float heading = a_actor->data.angle.z;
			F4SE::GetTaskInterface()->AddTask([id, pos, heading]() {
				g_positionBefore[id] = { pos, heading, std::chrono::steady_clock::now() };
			});
		}
		REX::INFO("SuppressInteraction: {:08X} BlockActivation dispatch={}, AllowPCDialogue dispatch={}",
			a_actor->GetFormID(), blockOk, dialogueOk);
		// Out of any furniture or idle marker before they're placed (else
		// they keep its pose and props under the scene), and nothing for
		// their AI to do until the scene ends (ReleaseFromScene's
		// EvaluatePackage hands them back their routine).
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() {
			g_guardKicks.erase(id);  // a new scene: the guard starts over
			g_holsterKicks.erase(id);
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor || IsPlayer(actor)) {
				return;
			}
			if (LeaveFurniture(actor, true)) {
				REX::INFO("SuppressInteraction: {:08X} was in furniture or an idle marker, taken out", id);
			}
			// OStim's scene package (ScenePackage.h): their AI picks nothing
			// else until the scene ends, so a settler's job, a sandbox or a
			// work spot can't pull them out of it. EvaluatePackage, as
			// OStim's Thread::AddActor, for them to switch to it now.
			ScenePackage::Add(id);
			CallActorMethod(actor, "Actor"sv, "EvaluatePackage"sv, false);
		});
	}

	void RestoreInteraction(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("RestoreInteraction called with a null actor");
			return;
		}

		const auto blockOk = CallActorMethod(a_actor, "ObjectReference"sv, "BlockActivation"sv, false, false);
		const auto dialogueOk = CallActorMethod(a_actor, "Actor"sv, "AllowPCDialogue"sv, true);
		REX::INFO("RestoreInteraction: {:08X} BlockActivation dispatch={}, AllowPCDialogue dispatch={}",
			a_actor->GetFormID(), blockOk, dialogueOk);
	}

	void LockActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("LockActor called with a null actor");
			return;
		}

		REX::INFO("LockActor: {:08X}", a_actor->GetFormID());

		const auto restrainedOk = CallActorMethod(a_actor, "Actor"sv, "SetRestrained"sv, true);
		const auto aiOk = CallActorMethod(a_actor, "Actor"sv, "EnableAI"sv, false, false);
		REX::INFO("LockActor: SetRestrained dispatch={}, EnableAI dispatch={}", restrainedOk, aiOk);
	}

	void UnlockActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("UnlockActor called with a null actor");
			return;
		}

		REX::INFO("UnlockActor: {:08X}", a_actor->GetFormID());

		const auto restrainedOk = CallActorMethod(a_actor, "Actor"sv, "SetRestrained"sv, false);
		const auto aiOk = CallActorMethod(a_actor, "Actor"sv, "EnableAI"sv, true, false);
		REX::INFO("UnlockActor: SetRestrained dispatch={}, EnableAI dispatch={}", restrainedOk, aiOk);
	}

	// ---- Spike 4 continued: position one actor relative to another ----
	// Uses `data.angle.z` (radians) for the anchor's current heading -- the
	// one field confirmed directly from Actor.h's own inline GetHeading():
	// `return data.angle.z;`. Position itself is no longer read in native
	// code at all; MoveTo computes it from the anchor's own location.

	// Places a_target at a_distance units from a_anchor, offset by
	// a_angleOffsetDeg degrees from the anchor's own facing, then turns
	// a_target to face back toward the anchor.
	//
	// Goes through ObjectReference's own native MoveTo/SetAngle (dispatched
	// via CallActorMethod, same as LockActor/UnlockActor) rather than the
	// raw C++ SetPosition/SetHeading virtuals. Those only update partial
	// actor state and caused real desync bugs (mesh stretching, then a
	// facegen glitch after a QueueUpdate workaround); MoveTo/SetAngle are
	// the same native functions the engine itself uses for every teleport,
	// so they should keep position, 3D, and physics in sync properly.
	//
	// MoveTo's offsets are relative to the ANCHOR's own local facing
	// (X = right, Y = forward), so we don't need the anchor's world
	// heading to compute the offset -- only to compute the final facing
	// angle afterward.
	void MoveActorTo(std::monostate, RE::Actor* a_target, RE::Actor* a_anchor, float a_distance, float a_angleOffsetDeg)
	{
		if (!a_target || !a_anchor) {
			REX::WARN("MoveActorTo called with a null actor");
			return;
		}

		// NOTE: switched from MoveTo(ObjectReference, ...) to SetPosition
		// (plain floats) -- MoveTo's object argument silently no-op'd even
		// with timing ruled out as the cause, while SetAngle (plain floats,
		// no object arg) worked correctly. Computing world-space XYZ
		// ourselves sidesteps whatever that argument-packing issue was.
		// data.location is UNCONFIRMED (inferred alongside the confirmed
		// data.angle.z); if this fails to compile, that's the first thing
		// to check -- search TESObjectREFR.h for GetPositionX/Y/Z or the
		// real member name.
		const auto anchorPos = a_anchor->data.location;
		const float anchorHeadingRad = a_anchor->data.angle.z;
		const float anchorHeadingDeg = anchorHeadingRad * 180.0f / PI_F;

		const float angleRad = anchorHeadingRad + a_angleOffsetDeg * PI_F / 180.0f;
		const float targetX = anchorPos.x + a_distance * std::sin(angleRad);
		const float targetY = anchorPos.y + a_distance * std::cos(angleRad);
		const float targetZ = anchorPos.z;
		const float targetHeadingDeg = anchorHeadingDeg + a_angleOffsetDeg + 180.0f;  // face back toward anchor

		REX::INFO("MoveActorTo: {:08X} relative to {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
			a_target->GetFormID(), a_anchor->GetFormID(), targetX, targetY, targetZ, targetHeadingDeg);

		const auto moveOk = CallActorMethod(a_target, "ObjectReference"sv, "SetPosition"sv, targetX, targetY, targetZ);
		const auto angleOk = CallActorMethod(a_target, "ObjectReference"sv, "SetAngle"sv, 0.0f, 0.0f, targetHeadingDeg);
		REX::INFO("MoveActorTo: SetPosition dispatch={}, SetAngle dispatch={}", moveOk, angleOk);
	}

	// ---- Scene collision ----
	// Paired animations put both actors' references on one spot, so their
	// collision capsules overlap for the whole scene and shove each other
	// apart (confirmed: equal-and-opposite 40-70 unit displacement before
	// the idles even started). Bodies sharing a nonzero Havok system group
	// skip collision with EACH OTHER but still collide with the world, so
	// they keep standing on the ground. (The kNoCollision flag would also
	// drop ground collision, so it's deliberately not used.)

	// g_savedFilters (each actor's filter before the scene) is with the
	// collision watch above.

	RE::bhkCharacterController* GetCharController(RE::Actor* a_actor)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		const auto middleHigh = process ? process->middleHigh : nullptr;
		return middleHigh ? middleHigh->charController.get() : nullptr;
	}

	bool ApplyFilter(RE::Actor* a_actor, std::uint32_t a_filter)
	{
		const PhysicsLock physics(a_actor);
		const auto        controller = GetCharController(a_actor);
		if (!controller) {
			return false;
		}
		RE::CFilter filter{};
		filter.filter = a_filter;
		return controller->SetCollisionFilterInfo(filter);
	}

	void IgnorePairCollision(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("IgnorePairCollision called with a null actor");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor0, a_actor1]() {
			const PhysicsLock physics(a_actor0);  // both are in one cell for a scene
			const auto        c0 = GetCharController(a_actor0);
			const auto c1 = GetCharController(a_actor1);
			const auto b0 = c0 ? c0->GetBodyImpl() : nullptr;
			const auto b1 = c1 ? c1->GetBodyImpl() : nullptr;
			if (!b0 || !b1) {
				REX::WARN("IgnorePairCollision: missing character controller or body");
				return;
			}

			const auto f0 = b0->m_collisionFilterInfo;
			const auto f1 = b1->m_collisionFilterInfo;
			g_savedFilters.try_emplace(a_actor0->GetFormID(), f0);
			g_savedFilters.try_emplace(a_actor1->GetFormID(), f1);

			RE::CFilter cf0{}, cf1{};
			cf0.filter = f0;
			cf1.filter = f1;
			// Share actor 0's group if it has one; otherwise a fixed group
			// unlikely to clash with anything the game assigns.
			std::uint32_t group = cf0.GetSystemGroup();
			if (group == 0) {
				group = 0xF5A1;
			}
			cf0.SetSystemGroup(group);
			cf1.SetSystemGroup(group);

			const auto ok0 = c0->SetCollisionFilterInfo(cf0);
			const auto ok1 = c1->SetCollisionFilterInfo(cf1);
			REX::INFO("IgnorePairCollision: {:08X} filter {:08X} -> {:08X} ({}), {:08X} filter {:08X} -> {:08X} ({})",
				a_actor0->GetFormID(), f0, cf0.filter, ok0, a_actor1->GetFormID(), f1, cf1.filter, ok1);
		});
	}

	void RestoreCollisionNow(RE::Actor* a_actor);

	void RestoreCollision(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("RestoreCollision called with a null actor");
			return;
		}

		// After the actors have stepped apart (MoveApart): collision back
		// while they're still inside each other leaves them stuck together.
		Scheduler::AfterOnMain(std::chrono::milliseconds(700), [a_actor]() { RestoreCollisionNow(a_actor); });
	}

	void RestoreCollisionNow(RE::Actor* a_actor)
	{
		RestoreTouched(a_actor->GetFormID());
		RestoreGravity(a_actor);
		{
			const auto it = g_savedFilters.find(a_actor->GetFormID());
			if (it == g_savedFilters.end()) {
				REX::INFO("RestoreCollision: {:08X} has no saved filter", a_actor->GetFormID());
				return;
			}
			const auto ok = ApplyFilter(a_actor, it->second);
			REX::INFO("RestoreCollision: {:08X} filter -> {:08X} ({})", a_actor->GetFormID(), it->second, ok);
			g_savedFilters.erase(it);
		}
	}

	// ---- Two-actor scenes ----

	// Puts both actors on the same spot, a_distance units in front of
	// a_anchor, with the same heading: a_anchor's heading plus
	// a_headingOffsetDeg (180 = facing the anchor, 0 = facing where the
	// anchor faces; use distance 0 / offset 0 to build the scene on the
	// anchor's own spot, e.g. when the anchor is a participant). Paired animations
	// are authored around one shared origin, with the offset between the
	// actors baked into the animation itself, so both must start from the
	// same point and facing.
	void PlacePair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, RE::Actor* a_anchor, float a_distance, float a_headingOffsetDeg)
	{
		if (!a_actor0 || !a_actor1 || !a_anchor) {
			REX::WARN("PlacePair called with a null actor");
			return;
		}
		if (PlaceOnPendingFurniture({ a_actor0, a_actor1 })) {
			return;
		}

		const auto  anchorPos = a_anchor->data.location;
		const float anchorHeadingRad = a_anchor->data.angle.z;
		const float x = anchorPos.x + a_distance * std::sin(anchorHeadingRad);
		const float y = anchorPos.y + a_distance * std::cos(anchorHeadingRad);
		const float z = anchorPos.z;
		float headingDeg = std::fmod(anchorHeadingRad * 180.0f / PI_F + a_headingOffsetDeg, 360.0f);
		if (headingDeg < 0.0f) {
			headingDeg += 360.0f;
		}

		REX::INFO("PlacePair: {:08X} + {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
			a_actor0->GetFormID(), a_actor1->GetFormID(), x, y, z, headingDeg);

		// Natively, like furniture placement: Papyrus SetPosition on the
		// player is a teleport the game may fade the screen for, and with
		// the script engine busy that fade can take very long to clear.
		const Furniture::Spot spot{ RE::NiPoint3{ x, y, z }, headingDeg * PI_F / 180.0F };
		for (auto* actor : { a_actor0, a_actor1 }) {
			PlaceActorAt(actor, spot);
		}
	}

	// Starts roles 0 and 1 of a registered two-actor scene in the same task,
	// i.e. on the same game frame, so the two actors begin in step.
	void PlaySceneIdles(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, std::string a_sceneID)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("PlaySceneIdles called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() < 2) {
			REX::WARN("PlaySceneIdles: no two-actor scene \"{}\"", a_sceneID);
			ReleaseFailedStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, "no such scene");
			return;
		}
		const int  speed = StartSpeed(*scene);  // OStim: a new thread starts at the scene's defaultSpeed
		const auto idle0 = scene->speeds[speed][0];
		const auto idle1 = scene->speeds[speed][1];
		const auto sceneID = scene->id;

		REX::INFO("PlaySceneIdles: scene \"{}\": {:08X} <- {:08X}, {:08X} <- {:08X}",
			scene->id, a_actor0->GetFormID(), idle0->GetFormID(), a_actor1->GetFormID(), idle1->GetFormID());

		F4SE::GetTaskInterface()->AddTask([a_actor0, idle0, a_actor1, idle1, sceneID, speed]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			if (!p0 || !p1) {
				REX::WARN("PlaySceneIdles: an actor has no AI process");
				ReleaseFailedStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, "no AI process");
				return;
			}
			for (auto* actor : { a_actor0, a_actor1 }) {
				const auto& pos = actor->data.location;
				REX::INFO("PlaySceneIdles: {:08X} actually at ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
					actor->GetFormID(), pos.x, pos.y, pos.z, actor->data.angle.z * 180.0f / PI_F);
			}
			const auto ok0 = p0->PlayIdle(*a_actor0, idle0, nullptr);
			const auto ok1 = p1->PlayIdle(*a_actor1, idle1, nullptr);
			REX::INFO("PlaySceneIdles: PlayIdle returned {} / {}", ok0, ok1);
			if (ok0) {
				CuePlayed(a_actor0->GetFormID(), idle0, sceneID);
			}
			if (ok1) {
				CuePlayed(a_actor1->GetFormID(), idle1, sceneID);
			}
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor0) || IsPlayer(a_actor1);
			if (!ok0 && !ok1) {
				ReleaseFailedStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, "the game refused both animations");
				return;
			}
			TrackSceneStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, sceneID, speed);
			if (focus) {
				PlayerScene focused{ a_actor0->GetFormID(), a_actor1->GetFormID(), sceneID, speed };
				if (const auto active = FindActiveScene(a_actor0->GetFormID())) {
					focused.furniture = active->furnitureType;
				}
				SetPlayerScene(std::move(focused));
			}
		});
	}

	// Queues sequence a_sequenceID for the next scene started with these
	// actors (akActor1 None for a solo sequence) and returns the scene to
	// start, its first one; "" if there's no such sequence or the actor
	// count doesn't match. FourStimScene.BeginPairSequence wraps this.
	std::string QueueSequenceNative(std::monostate, std::string a_sequenceID, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequenceID);
		if (!sequence || !a_actor0) {
			REX::WARN("QueueSequence: no sequence \"{}\" (or no actor)", a_sequenceID);
			return {};
		}
		std::vector<std::uint32_t> actors{ a_actor0->GetFormID() };
		if (a_actor1) {
			actors.push_back(a_actor1->GetFormID());
		}
		if (actors.size() != sequence->actorCount) {
			REX::WARN("QueueSequence: \"{}\" is for {} actor(s), {} given", sequence->id, sequence->actorCount, actors.size());
			return {};
		}
		QueueSequence(sequence, std::move(actors));
		return sequence->entries.front().scene;
	}

	// Plays sequence a_sequenceID on the scene a_actor is already in.
	// Returns false if there's no such sequence; the rest is checked on the
	// next frame (see the log).
	bool StartSequenceOnSceneNative(std::monostate, RE::Actor* a_actor, std::string a_sequenceID)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequenceID);
		if (!sequence || !a_actor) {
			REX::WARN("StartSequenceOnScene: no sequence \"{}\" (or no actor)", a_sequenceID);
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID(), sequence]() { StartSequenceOnScene(id, sequence); });
		return true;
	}

	// Stops the sequence the scene a_actor is in is playing (the scene stays
	// where it is).
	void StopSequenceNative(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() {
			const auto active = FindActiveScene(id);
			if (active && active->sequence) {
				REX::INFO("Sequence \"{}\": stopped", active->sequence->id);
				active->sequence.reset();
				ArmAutoplay(*active);  // a transition still finishes
			}
		});
	}

	// Number of actors (roles) in a registered scene, or 0 if it doesn't exist.
	std::int32_t GetSceneActorCount(std::monostate, std::string a_sceneID)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene ? static_cast<std::int32_t>(scene->actors.size()) : 0;
	}

	// ---- Actions (docs/ACTIONS.md) ----

	bool SceneHasAction(std::monostate, std::string a_sceneID, std::string a_type)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && scene->HasAction(a_type);
	}

	bool SceneHasActionTag(std::monostate, std::string a_sceneID, std::string a_tag)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && scene->HasActionTag(a_tag);
	}

	std::int32_t GetSceneActionCount(std::monostate, std::string a_sceneID)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene ? static_cast<std::int32_t>(scene->actions.size()) : 0;
	}

	// The first action of a_type (id, alias, or "" for any) with these roles
	// (-1: any); its index, or -1.
	std::int32_t FindSceneAction(std::monostate, std::string a_sceneID, std::string a_type, std::int32_t a_actor, std::int32_t a_target)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene) {
			return -1;
		}
		// By the alias-resolved id, as OStim's findAction: unknown types too.
		const auto id = a_type.empty() ? std::string{} : Actions::ResolveAlias(a_type);
		for (std::size_t i = 0; i < scene->actions.size(); ++i) {
			const auto& action = scene->actions[i];
			if ((id.empty() || action.type->id == id) &&
				(a_actor < 0 || action.actor == static_cast<std::size_t>(a_actor)) &&
				(a_target < 0 || action.target == static_cast<std::size_t>(a_target))) {
				return static_cast<std::int32_t>(i);
			}
		}
		return -1;
	}

	namespace
	{
		const SceneRegistry::SceneAction* SceneActionAt(const SceneRegistry::Scene* a_scene, std::int32_t a_index)
		{
			return a_scene && a_index >= 0 && static_cast<std::size_t>(a_index) < a_scene->actions.size() ? &a_scene->actions[static_cast<std::size_t>(a_index)] : nullptr;
		}
	}

	std::string GetSceneActionType(std::monostate, std::string a_sceneID, std::int32_t a_index)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		const auto action = SceneActionAt(scene.get(), a_index);
		return action ? action->type->id : std::string{};
	}

	// a_which: 0 actor, 1 target, 2 performer. The role index, or -1.
	std::int32_t GetSceneActionRole(std::monostate, std::string a_sceneID, std::int32_t a_index, std::int32_t a_which)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		const auto action = SceneActionAt(scene.get(), a_index);
		if (!action) {
			return -1;
		}
		switch (a_which) {
		case 0:
			return static_cast<std::int32_t>(action->actor);
		case 1:
			return static_cast<std::int32_t>(action->target);
		case 2:
			return static_cast<std::int32_t>(action->performer);
		default:
			return -1;
		}
	}

	std::vector<std::string> GetActionTypes(std::monostate)
	{
		return Actions::List();
	}

	// Events (docs/EVENTS.md), OStim's OThread.CallEvent: fires a_event in
	// the scene a_inScene is in; roles -1: a_inScene's own (actor and
	// performer) or none (target). False if there's no such event. The event
	// fires on the main thread right after.
	bool CallEvent(std::monostate, RE::Actor* a_inScene, std::string a_event, std::int32_t a_actor, std::int32_t a_target, std::int32_t a_performer)
	{
		if (!a_inScene) {
			return false;
		}
		auto lower = a_event;
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const auto events = Events::List();
		if (std::ranges::find(events, lower) == events.end()) {
			REX::WARN("CallEvent: no event \"{}\"", a_event);
			return false;
		}
		const auto id = a_inScene->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, lower, a_actor, a_target, a_performer]() {
			const auto active = FindActiveScene(id);
			if (!active) {
				REX::WARN("CallEvent \"{}\": {:08X} isn't in a scene", lower, id);
				return;
			}
			const int self = static_cast<int>(std::ranges::find(active->actors, id) - active->actors.begin());
			FireEvent(*active, lower, a_actor < 0 ? self : a_actor, a_target, a_performer < 0 ? (a_actor < 0 ? self : a_actor) : a_performer);
		});
		return true;
	}

	std::vector<std::string> GetEvents(std::monostate)
	{
		return Events::List();
	}

	std::string GetActionName(std::monostate, std::string a_type)
	{
		const auto type = Actions::Find(a_type);
		return type ? type->name : std::string{};
	}

	std::vector<std::string> GetActionTags(std::monostate, std::string a_type)
	{
		const auto type = Actions::Find(a_type);
		return type ? type->tags : std::vector<std::string>{};
	}

	bool ActionHasTag(std::monostate, std::string a_type, std::string a_tag)
	{
		const auto type = Actions::Find(a_type);
		return type && type->HasTag(a_tag);
	}

	// ---- Excitement (Excitement.h) ----

	float GetExcitement(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::Get(a_actor->GetFormID()) : -1.0F;
	}

	void SetExcitement(std::monostate, RE::Actor* a_actor, float a_value)
	{
		if (a_actor) {
			Excitement::Set(a_actor->GetFormID(), a_value);
		}
	}

	void AddExcitement(std::monostate, RE::Actor* a_actor, float a_value, bool a_useMultiplier)
	{
		if (a_actor) {
			Excitement::Add(a_actor->GetFormID(), a_value, a_useMultiplier);
		}
	}

	std::int32_t GetTimesClimaxed(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::TimesClimaxed(a_actor->GetFormID()) : 0;
	}

	// OStim's OActor.Climax(act, ignoreStall) (ThreadActor::orgasm): while
	// stalled the climax waits for the stall to end, and ignoring the stall
	// doesn't lift it. Was: the stall dropped for good, and the climax lost
	// if a stall came back before the next tick.
	void Climax(std::monostate, RE::Actor* a_actor, bool a_ignoreStall)
	{
		if (a_actor) {
			Excitement::Orgasm(a_actor->GetFormID(), a_ignoreStall);
		}
	}

	void StallClimax(std::monostate, RE::Actor* a_actor, bool a_stall)
	{
		if (a_actor) {
			Excitement::SetStalled(a_actor->GetFormID(), a_stall);
		}
	}

	// OActor.IsClimaxStalled(act, checkThread): the actor's own stall, or
	// with a_checkThread their thread's too.
	bool IsClimaxStalled(std::monostate, RE::Actor* a_actor, bool a_checkThread)
	{
		return a_actor && Excitement::IsStalled(a_actor->GetFormID(), a_checkThread);
	}

	float GetExcitementMultiplier(std::monostate, RE::Actor* a_actor)
	{
		// As OStim's: -1 for an actor not in a scene.
		return a_actor && Excitement::Get(a_actor->GetFormID()) >= 0.0F ? Excitement::Multiplier(a_actor->GetFormID()) : -1.0F;
	}

	void SetExcitementMultiplier(std::monostate, RE::Actor* a_actor, float a_multiplier)
	{
		if (a_actor) {
			Excitement::SetMultiplier(a_actor->GetFormID(), a_multiplier);
		}
	}

	float GetTimeUntilClimax(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::TimeUntilClimax(a_actor->GetFormID()) : -1.0F;
	}

	// ---- Undressing (Undress.h) ----

	// FourStimUndress.Strip reports what it took off.
	void NoteStripped(std::monostate, RE::Actor* a_actor, std::vector<RE::TESForm*> a_items)
	{
		if (!a_actor) {
			return;
		}
		std::vector<std::uint32_t> ids;
		for (const auto item : a_items) {
			if (item) {
				ids.push_back(item->GetFormID());
			}
		}
		Undress::NoteStripped(a_actor->GetFormID(), ids);
	}

	void UndressActor(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			Undress::StripAll(a_actor->GetFormID(), true);
		}
	}

	void RedressActor(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			Undress::Redress(a_actor->GetFormID(), true);
		}
	}

	// ---- Auto mode and scene details ----

	// Whether a_actor is in a running scene (one actor, one scene).
	bool IsInScene(std::monostate, RE::Actor* a_actor)
	{
		return a_actor && FindActiveScene(a_actor->GetFormID()) != nullptr;
	}

	// FourStimScene's EndPairScene / EndGroupScene, once they've let the
	// actors go: the end event of the scene they were in goes out now
	// (SendEndedFor).
	void SceneReleased(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			if (actor) {
				ids.push_back(actor->GetFormID());
			}
		}
		F4SE::GetTaskInterface()->AddTask([ids = std::move(ids)]() { SendEndedFor(ids, false); });
	}

	void SetAutoMode(std::monostate, RE::Actor* a_actor, bool a_on)
	{
		if (!a_actor) {
			return;
		}
		const auto id = a_actor->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, a_on]() {
			if (const auto active = FindActiveScene(id)) {
				a_on ? StartAutoMode(*active) : StopAutoMode(*active);
			}
		});
	}

	bool IsAutoMode(std::monostate, RE::Actor* a_actor)
	{
		// Read from a Papyrus thread: a plain copy of the flag is fine.
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		return active && active->autoMode.on;
	}

	// Plays the a_event auto transition ("climax", "pullout"...) of the
	// scene a_actor is in: a_role's (or the first role that has one, -1).
	bool AutoTransition(std::monostate, RE::Actor* a_actor, std::string a_event, std::int32_t a_role)
	{
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		const auto scene = active ? SceneRegistry::Find(active->sceneID) : nullptr;
		if (!scene) {
			return false;
		}
		std::string dest;
		for (std::size_t role = 0; role < scene->actors.size() && dest.empty(); ++role) {
			if (a_role < 0 || static_cast<std::size_t>(a_role) == role) {
				dest = scene->actors[role].AutoTransition(a_event);
			}
		}
		if (dest.empty()) {
			dest = scene->AutoTransition(a_event);  // OStim's scene-wide ones
		}
		if (dest.empty()) {
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID(), dest]() {
			if (const auto active = FindActiveScene(id); active && PlayOnActiveScene(*active, dest, -1)) {
				ArmAutoplay(*active);
			}
		});
		return true;
	}

	bool SceneActorHasTag(std::monostate, std::string a_sceneID, std::int32_t a_role, std::string a_tag)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && a_role >= 0 && static_cast<std::size_t>(a_role) < scene->actors.size() && scene->actors[a_role].HasTag(a_tag);
	}

	std::vector<std::string> GetSceneActorTags(std::monostate, std::string a_sceneID, std::int32_t a_role)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene && a_role >= 0 && static_cast<std::size_t>(a_role) < scene->actors.size() ? scene->actors[a_role].tags : std::vector<std::string>{};
	}

	// Re-reads all scene files (handy while authoring). Returns the count loaded.
	std::int32_t ReloadScenes(std::monostate)
	{
		Translations::Load();  // first: scenes, furniture... are translated as they load
		const auto count = SceneRegistry::Reload();
		Alignment::Reload();
		Expressions::Reload();
		Sounds::Reload();
		Events::Reload();
		Lights::Reload();
		Scaling::Reload();
		ActorProperties::Reload();
		EquipObjects::Reload();
		Options::Reload();
		HUD::LoadConfig();  // theme and Utility entries too, for HUD authors
		return count;
	}

	// Equip objects (OStim's OActor functions), for actors in a scene.
	bool IsTongue(std::string a_type)
	{
		std::ranges::transform(a_type, a_type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_type == "tongue";
	}

	bool EquipObject(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		if (!a_actor) {
			return false;
		}
		const bool tongue = IsTongue(a_type);
		const bool ok = EquipObjects::Equip(a_actor->GetFormID(), std::move(a_type));
		if (ok && tongue) {
			Expressions::Hold(a_actor->GetFormID(), "tongue");  // its mouth open, or it can't be seen (Expressions::Hold)
		}
		return ok;
	}

	void UnequipObject(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		if (a_actor) {
			const bool tongue = IsTongue(a_type);
			EquipObjects::Unequip(a_actor->GetFormID(), std::move(a_type));
			if (tongue) {
				Expressions::Hold(a_actor->GetFormID(), "");
			}
		}
	}

	bool IsObjectEquipped(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		return a_actor && EquipObjects::IsEquipped(a_actor->GetFormID(), std::move(a_type));
	}

	bool SetObjectVariant(std::monostate, RE::Actor* a_actor, std::string a_type, std::string a_variant, float a_duration)
	{
		return a_actor && EquipObjects::SetVariant(a_actor->GetFormID(), std::move(a_type), std::move(a_variant), a_duration);
	}

	void UnsetObjectVariant(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		if (a_actor) {
			EquipObjects::UnsetVariant(a_actor->GetFormID(), std::move(a_type));
		}
	}

	std::vector<std::string> GetEquipObjectTypes(std::monostate)
	{
		return EquipObjects::Types();
	}

	// 4Stim's undressing took clothes off or put them on (Papyrus, after the
	// items): heels are measured again and HHS turned off or on to match.
	void HeelsChanged(std::monostate, std::int32_t a_actorID)
	{
		const auto id = static_cast<std::uint32_t>(a_actorID);
		F4SE::GetTaskInterface()->AddTask([id]() {
			if (const auto active = FindActiveScene(id)) {
				LockScene(*active);  // measures heels again, rescales, HHS on / off
				return;
			}
			bool waiting;
			{
				std::scoped_lock lock(g_heelsWaitingLock);
				waiting = g_heelsWaiting.contains(id);
			}
			if (waiting && Scaling::HeelHeight(RE::TESForm::GetFormByID<RE::Actor>(id)) > 0.0F) {
				{
					std::scoped_lock lock(g_heelsWaitingLock);
					g_heelsWaiting.erase(id);
				}
				RestartHeels(id, "dressed again");
			}
		});
	}

	// Actor properties (docs/ACTORS.md): OStim's OActor.HasSchlong (4Stim:
	// HasPenis) and the actor type.
	bool HasPenis(std::monostate, RE::Actor* a_actor)
	{
		return ActorProperties::HasPenis(a_actor);
	}

	std::string GetActorType(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? ActorProperties::Type(a_actor) : std::string{};
	}

	// ---- Furniture (OStim's OFurniture and OThread furniture functions) ----

	// FourStimFurniture.Block blocked it (it wasn't already): 4Stim unblocks it.
	void NoteFurnitureBlocked(std::monostate, std::int32_t a_refID)
	{
		std::scoped_lock lock(g_blockedFurnitureLock);
		g_blockedFurniture.insert(static_cast<std::uint32_t>(a_refID));
	}

	// After the fade (FourStimMenu.ChangeFurnitureFaded).
	void ApplyFurnitureChangeNative(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() { ApplyFurnitureChange(); });
	}

	// OStim's OThread.ChangeFurniture: a_actor's scene onto a_furniture, with
	// a_sceneID or ("") a random intro / idle scene that fits there.
	void ChangeFurnitureNative(std::monostate, RE::Actor* a_actor, RE::TESObjectREFR* a_furniture, std::string a_sceneID)
	{
		if (!a_actor || !a_furniture) {
			return;
		}
		const auto actorID = a_actor->GetFormID();
		const auto refID = a_furniture->GetFormID();
		F4SE::GetTaskInterface()->AddTask([actorID, refID, sceneID = std::move(a_sceneID)]() {
			const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(refID);
			const auto type = ref ? Furniture::Classify(ref) : std::string{};
			if (type.empty()) {
				REX::WARN("ChangeFurniture: {:08X} isn't furniture of any type", refID);
				return;
			}
			RequestFurnitureChange(actorID, Furniture::Found{ type, Furniture::Name(type), refID, 0.0F, std::nullopt }, sceneID);
		});
	}

	// ---- Muting and choosing equip objects (OStim's OActor.Mute; the HUD's options) ----

	// Mute / Unmute apply on the main thread; until they have, IsMuted
	// answers what was asked, so a script reads back its own call (OStim's
	// are synchronous; parity audit F21).
	std::mutex                               g_muteLock;
	std::unordered_map<std::uint32_t, bool> g_muteAsked;

	void AskMute(std::uint32_t a_id, bool a_muted)
	{
		{
			std::scoped_lock lock(g_muteLock);
			g_muteAsked[a_id] = a_muted;
		}
		F4SE::GetTaskInterface()->AddTask([a_id, a_muted]() {
			Sounds::SetMuted(a_id, a_muted);
			std::scoped_lock lock(g_muteLock);
			if (const auto it = g_muteAsked.find(a_id); it != g_muteAsked.end() && it->second == a_muted) {
				g_muteAsked.erase(it);
			}
		});
	}

	void Mute(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			AskMute(a_actor->GetFormID(), true);
		}
	}

	void Unmute(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			AskMute(a_actor->GetFormID(), false);
		}
	}

	// ---- The weapon an actor held before their scene ----
	// FourStimScene.HolsterForScene notes it as it takes it off, and
	// ReleaseFromScene puts it back on, as OStim's SetRemoveWeaponsAtStart
	// (on by default: ThreadActor::removeWeapons, given back at the end).
	// Kept in the save with the scene.
	std::mutex                                   g_sceneWeaponsLock;
	std::unordered_map<std::uint32_t, std::uint32_t> g_sceneWeapons;  // actor -> weapon form

	void NoteSceneWeapon(std::uint32_t a_actor, std::uint32_t a_weapon)
	{
		std::scoped_lock lock(g_sceneWeaponsLock);
		if (a_weapon) {
			g_sceneWeapons[a_actor] = a_weapon;
		} else {
			g_sceneWeapons.erase(a_actor);
		}
	}

	std::uint32_t SceneWeaponOf(std::uint32_t a_actor)
	{
		std::scoped_lock lock(g_sceneWeaponsLock);
		const auto it = g_sceneWeapons.find(a_actor);
		return it != g_sceneWeapons.end() ? it->second : 0;
	}

	// FourStim.SheatheForScene: HolsterForScene puts a drawn weapon away
	// before taking it off (see SheatheNow).
	bool RemoveWeaponsAtStart(std::monostate)
	{
		return Undress::Settings().removeWeaponsAtStart;
	}

	bool PlayerAIDrivenInScenes(std::monostate)
	{
		return g_settings.playerAIDriven;
	}

	void SheatheForScene(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() { SheatheNow(RE::TESForm::GetFormByID<RE::Actor>(id), "scene start"); });
		}
	}

	void RememberSceneWeapon(std::monostate, RE::Actor* a_actor, RE::TESForm* a_weapon)
	{
		if (a_actor && a_weapon) {
			NoteSceneWeapon(a_actor->GetFormID(), a_weapon->GetFormID());
		}
	}

	// The weapon to put back on, by form ID (0: nothing was taken off), and
	// forgotten. A form ID, not the Form: a Form returned from here arrived
	// in Papyrus as "Mismatched types assigning to variable" (a 1.10.163
	// tester's Papyrus log, every scene end), as Actors passed from native
	// code do (FourStimPhysics), so weapons were never put back.
	std::int32_t TakeSceneWeaponID(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return 0;
		}
		const auto id = SceneWeaponOf(a_actor->GetFormID());
		NoteSceneWeapon(a_actor->GetFormID(), 0);
		return static_cast<std::int32_t>(id);
	}

	// OStim's OData.GetVoiceSetPairs / GetVoiceSetName / SetVoiceSet: voice
	// set choices per actor base form ID (docs/SOUND.md).
	std::vector<std::string> GetVoiceSetPairs(std::monostate)
	{
		return Sounds::VoiceSetPairs();
	}

	std::string GetVoiceSetName(std::monostate, std::int32_t a_formID)
	{
		return Sounds::VoiceSetName(static_cast<std::uint32_t>(a_formID));
	}

	void SetVoiceSet(std::monostate, std::int32_t a_formID, std::string a_voice)
	{
		const auto base = static_cast<std::uint32_t>(a_formID);
		Sounds::SetVoiceSet(base, a_voice);
		F4SE::GetTaskInterface()->AddTask([base]() { Sounds::ApplyVoiceSet(base); });
	}

	// OStim's OData action and event stimulation: per actor base, for a
	// side of an action or event (aiRole 1 actor, 2 target, 4 performer),
	// in place of its own (docs/EXCITEMENT.md). Saved with the game.
	std::vector<std::string> GetActions(std::monostate)
	{
		auto list = Actions::List();
		std::ranges::sort(list);
		return list;
	}

	// The side's own values: the action's, or the event's (its supertypes').
	std::pair<float, float> DefaultStim(StimOverrides::Kind a_kind, std::int32_t a_role, const std::string& a_id)
	{
		const int index = a_role == StimOverrides::kActor ? 0 : a_role == StimOverrides::kTarget ? 1 : a_role == StimOverrides::kPerformer ? 2 : -1;
		if (index < 0) {
			return { 0.0F, 0.0F };
		}
		if (a_kind == StimOverrides::Kind::kEvent) {
			return Events::DefaultStimulation(a_id, index);
		}
		const auto type = Actions::Find(a_id);
		if (!type) {
			return { 0.0F, 0.0F };
		}
		const auto& side = index == 0 ? type->actor : index == 1 ? type->target : type->performer;
		return { side.stimulation, side.maxStimulation };
	}

	template <StimOverrides::Kind K>
	float GetStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		return StimOverrides::Stimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id).value_or(DefaultStim(K, a_role, a_id).first);
	}

	template <StimOverrides::Kind K>
	float GetMaxStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		return StimOverrides::MaxStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id).value_or(DefaultStim(K, a_role, a_id).second);
	}

	template <StimOverrides::Kind K>
	void SetStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id, float a_value)
	{
		StimOverrides::SetStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id, a_value);
	}

	template <StimOverrides::Kind K>
	void SetMaxStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id, float a_value)
	{
		StimOverrides::SetMaxStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id, a_value);
	}

	template <StimOverrides::Kind K>
	float GetDefaultStim(std::monostate, std::int32_t a_role, std::string a_id)
	{
		return DefaultStim(K, a_role, a_id).first;
	}

	template <StimOverrides::Kind K>
	float GetDefaultMaxStim(std::monostate, std::int32_t a_role, std::string a_id)
	{
		return DefaultStim(K, a_role, a_id).second;
	}

	template <StimOverrides::Kind K>
	void ResetStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		StimOverrides::ResetStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id);
	}

	template <StimOverrides::Kind K>
	void ResetMaxStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		StimOverrides::ResetMaxStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id);
	}

	// 4Stim's: the same for an actor (their base), for the HUD's options.
	void SetActorVoiceSet(std::monostate, RE::Actor* a_actor, std::string a_voice)
	{
		const auto npc = a_actor ? a_actor->GetNPC() : nullptr;
		if (npc) {
			SetVoiceSet({}, static_cast<std::int32_t>(npc->GetFormID()), std::move(a_voice));
		}
	}

	std::string GetActorVoiceSetName(std::monostate, RE::Actor* a_actor)
	{
		const auto npc = a_actor ? a_actor->GetNPC() : nullptr;
		return Sounds::VoiceSetName(npc ? npc->GetFormID() : 0);
	}

	bool IsMuted(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		{
			std::scoped_lock lock(g_muteLock);
			if (const auto it = g_muteAsked.find(a_actor->GetFormID()); it != g_muteAsked.end()) {
				return it->second;  // asked, not applied yet
			}
		}
		return Sounds::IsMuted(a_actor->GetFormID());  // read from a Papyrus thread, as IsInScene
	}

	// Which object of a type a_actor wears (its file name), and choosing it.
	bool SetEquipObjectID(std::monostate, RE::Actor* a_actor, std::string a_type, std::string a_id)
	{
		return a_actor && EquipObjects::SetChosen(a_actor->GetFormID(), std::move(a_type), std::move(a_id));
	}

	std::string GetEquipObjectID(std::monostate, RE::Actor* a_actor, std::string a_type)
	{
		return a_actor ? EquipObjects::Chosen(a_actor->GetFormID(), std::move(a_type)) : std::string{};
	}

	// OStim's OData.GetEquipObjectPairs / GetEquipObjectName / SetEquipObjectID,
	// by form ID (an actor base, a race, 0x0 men, 0x1 women; the player's
	// base is 0x7). SetEquipObjectID is 4Stim's by actor, so OStim's is
	// SetFormEquipObjectID here.
	std::vector<std::string> GetEquipObjectPairs(std::monostate, std::int32_t a_formID, std::string a_type)
	{
		return EquipObjects::PairsFor(static_cast<std::uint32_t>(a_formID), std::move(a_type));
	}

	std::string GetEquipObjectName(std::monostate, std::int32_t a_formID, std::string a_type)
	{
		return EquipObjects::NameFor(static_cast<std::uint32_t>(a_formID), std::move(a_type));
	}

	void SetFormEquipObjectID(std::monostate, std::int32_t a_formID, std::string a_type, std::string a_id)
	{
		EquipObjects::SetFor(static_cast<std::uint32_t>(a_formID), std::move(a_type), std::move(a_id));
	}

	// ---- The MCM page (docs/MCM.md) ----

	void RunKeyAction(std::string_view a_action);
	namespace API
	{
		void NotifyNGControl(FourStimNG_API::Thread::Controls a_control);  // the thread API's control callbacks
	}

	// A setting changed in the MCM: 4Stim.ini and the MCM's file read again.
	void ReloadSettings(std::monostate)
	{
		// A moment later, so the MCM has written its file first.
		Scheduler::AfterOnMain(std::chrono::milliseconds(250), []() {
			LoadSettings();
			HUD::LoadConfig();  // the theme and control scheme
			REX::INFO("Settings: read again (the MCM changed one)");
		});
	}

	// The MCM's buttons: export to / import from Documents\My Games\Fallout4\4Stim\mcmsettings.json
	// (OStim's), and back to 4Stim.ini's values.
	bool ExportSettings(std::monostate)
	{
		const bool ok = MCM::Export();
		Notify(ok ? TR("$4stim_settings_exported", "4Stim: settings exported") : TR("$4stim_settings_not_exported", "4Stim: couldn't export the settings"));
		return ok;
	}

	bool ImportSettings(std::monostate)
	{
		const bool ok = MCM::Import();
		if (ok) {
			ReloadSettings({});
		}
		Notify(ok ? TR("$4stim_settings_imported", "4Stim: settings imported") : TR("$4stim_settings_not_imported", "4Stim: no exported settings to import"));
		return ok;
	}

	void UseIniSettings(std::monostate)
	{
		MCM::UseIni();
		ReloadSettings({});
		Notify(TR("$4stim_settings_from_ini", "4Stim: back to 4Stim.ini's settings"));
	}

	// A scene key's action by name (the MCM's hotkeys): hotkey, npcscene,
	// speedup, speeddown, automode, pullout, end, search, align, freecam, hideui.
	void PressKey(std::monostate, std::string a_action)
	{
		std::ranges::transform(a_action, a_action.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		F4SE::GetTaskInterface()->AddTask([action = std::move(a_action)]() { RunKeyAction(action); });
	}

	// The "Change furniture" list for the focused scene (the HUD's Utility
	// entry does the same).
	void OpenFurnitureMenu(std::monostate)
	{
		FourStim::OpenFurnitureForFocused();
	}

	// OStim's OFurniture.GetFurnitureType ("" = none).
	std::string GetFurnitureType(std::monostate, RE::TESObjectREFR* a_furniture)
	{
		return a_furniture ? Furniture::Classify(a_furniture) : std::string{};
	}

	// OStim's OFurniture.IsChildOf: furniture of a_subType takes scenes for a_superType.
	bool IsFurnitureChildOf(std::monostate, std::string a_superType, std::string a_subType)
	{
		return !a_superType.empty() && !a_subType.empty() && Furniture::IsA(a_subType, a_superType);
	}

	// OStim's OEvent.IsChildOf: a_subType is a_superType or has it among
	// its supertypes.
	bool IsEventChildOf(std::monostate, std::string a_superType, std::string a_subType)
	{
		return !a_superType.empty() && !a_subType.empty() && Events::IsChildOf(a_subType, a_superType);
	}

	// OStim's OData.GetUndressingSlotMask / SetUndressingSlotMask: the slots
	// undressed, bit 0 = slot 30 (Fallout 4's slots). Set until the settings
	// are read again (4Stim.ini and the MCM's are what last: OStim's is in
	// the save).
	std::int32_t GetUndressingSlotMask(std::monostate)
	{
		std::uint32_t mask = 0;
		for (const auto slot : Undress::Settings().slots) {
			if (slot >= 30 && slot <= 61) {
				mask |= 1u << (slot - 30);
			}
		}
		return static_cast<std::int32_t>(mask);
	}

	void SetUndressingSlotMask(std::monostate, std::int32_t a_mask)
	{
		const auto       mask = static_cast<std::uint32_t>(a_mask);
		std::vector<int> slots;
		for (int slot = 30; slot <= 61; ++slot) {
			if (mask & (1u << (slot - 30))) {
				slots.push_back(slot);
			}
		}
		F4SE::GetTaskInterface()->AddTask([slots = std::move(slots)]() mutable { Undress::Settings().slots = std::move(slots); });
	}

	// OStim's OData.Localize / ToLocalizedString: a "$key" text translated
	// (4Stim's translation files, OStim's own keys among them).
	std::string LocalizeText(std::monostate, std::string a_text)
	{
		return Translations::Localize(a_text);
	}

	// OStim's OThread.GetFurniture: the furniture a_actor's scene is on (None
	// off furniture, or at a wall, which is no object).
	RE::TESObjectREFR* GetSceneFurniture(std::monostate, RE::Actor* a_actor)
	{
		// Read from a Papyrus thread, as IsInScene.
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		const auto ref = active ? active->furnitureRef : 0;
		return ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(ref) : nullptr;
	}

	// The type of the furniture a_actor's scene is on ("" = none).
	std::string GetSceneFurnitureType(std::monostate, RE::Actor* a_actor)
	{
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		return active ? active->furnitureType : std::string{};
	}

	bool RegisterForSceneEvents(std::monostate, RE::TESForm* a_receiver)
	{
		return SceneEvents::Register(a_receiver);
	}

	void UnregisterForSceneEvents(std::monostate, RE::TESForm* a_receiver)
	{
		SceneEvents::Unregister(a_receiver);
	}

	namespace Natives::FourModEvents
	{
		// ---- Mod events (FourModEvents, ModEvents.h): SKSE's for Fallout 4 ----
		bool Register(std::monostate, RE::TESForm* a_receiver, std::string a_eventName, std::string a_callback)
		{
			return ModEvents::Register(a_receiver, a_eventName, a_callback);
		}

		void Unregister(std::monostate, RE::TESForm* a_receiver, std::string a_eventName)
		{
			ModEvents::Unregister(a_receiver, a_eventName);
		}

		void UnregisterAll(std::monostate, RE::TESForm* a_receiver)
		{
			ModEvents::UnregisterAll(a_receiver);
		}

		void Send(std::monostate, std::string a_eventName, std::string a_strArg, float a_numArg, RE::TESForm* a_sender)
		{
			ModEvents::Send(a_eventName, a_strArg, a_numArg, a_sender);
		}
	}  // namespace Natives::FourModEvents

	namespace Natives::FourJSON
	{
		// FourJSON (OStim's OJSON): the end event's JSON.
		std::vector<RE::Actor*> GetActors(std::monostate, std::string a_json) { return ModEvents::JSONActors(a_json); }
		std::string             GetScene(std::monostate, std::string a_json) { return ModEvents::JSONScene(a_json); }
		std::vector<std::string> GetMetadata(std::monostate, std::string a_json) { return ModEvents::JSONMetadata(a_json); }
	}  // namespace Natives::FourJSON

	// The player's scene is about to end (end key, picker, HUD): nothing plays
	// on it any more (ActiveScene::ending), from the next frame. Main thread.
	void MarkPlayerSceneEnding()
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto focused = GetPlayerScene();
			if (const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr) {
				active->ending = true;
			}
		});
	}

	// An end's IdleStop in the same moment as a scene's PlayIdle can lose: the
	// game plays the scene's idle and the actors keep it after the end (the
	// owner's ZaZ pillory scene, 5 November: IdleStop returned true, and
	// both went on in the last scene's animation, the player held in place).
	// So when the end comes within a moment of a scene's idle, IdleStop goes
	// again shortly after, to whoever hasn't started another scene since.
	// From the IdleStop task (main thread), before TrackSceneStop.
	void StopAgainIfJustPlayed(const std::vector<std::uint32_t>& a_ids, RE::TESIdleForm* a_idle)
	{
		const auto active = a_ids.empty() ? nullptr : FindActiveScene(a_ids.front());
		if (!active || std::chrono::steady_clock::now() - active->lastPlayed > std::chrono::milliseconds(750)) {
			return;
		}
		REX::INFO("Scene end: \"{}\" played {} ms before its end: IdleStop again in a moment", active->sceneID,
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - active->lastPlayed).count());
		Scheduler::AfterOnMain(std::chrono::milliseconds(400), [a_ids, a_idle]() {
			for (const auto id : a_ids) {
				if (FindActiveScene(id)) {
					continue;  // in a new scene by now
				}
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, a_idle, nullptr);
				REX::INFO("Scene end: {:08X} IdleStop again returned {}", id, ok);
			}
		});
	}

	// Clears both actors' poses in the same task (same frame), mirroring
	// PlaySceneIdles, so the pair releases together.
	void StopPair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("StopPair called with a null actor");
			return;
		}

		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopPair: could not find IdleStop in Fallout4.esm");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor0, a_actor1, idle]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			const auto ok0 = p0 ? p0->PlayIdle(*a_actor0, idle, nullptr) : false;
			const auto ok1 = p1 ? p1->PlayIdle(*a_actor1, idle, nullptr) : false;
			REX::INFO("StopPair: {:08X} / {:08X} IdleStop returned {} / {}", a_actor0->GetFormID(), a_actor1->GetFormID(), ok0, ok1);
			StopAgainIfJustPlayed({ a_actor0->GetFormID(), a_actor1->GetFormID() }, idle);
			ClearFocusIfIn(a_actor0->GetFormID());
			ClearFocusIfIn(a_actor1->GetFormID());
			TrackSceneStop(a_actor0->GetFormID());
			TrackSceneStop(a_actor1->GetFormID());
		});
	}

	// ---- Group scenes (any number of actors) ----
	// The pair natives above, for a whole cast: all actors in role order.

	PlayerScene FocusFor(const std::vector<std::uint32_t>& a_ids, const std::string& a_sceneID)
	{
		PlayerScene scene;
		scene.role0 = a_ids.size() > 0 ? a_ids[0] : 0;
		scene.role1 = a_ids.size() > 1 ? a_ids[1] : 0;
		if (a_ids.size() > 2) {
			scene.more.assign(a_ids.begin() + 2, a_ids.end());
		}
		scene.sceneID = a_sceneID;
		return scene;
	}

	void IgnoreGroupCollision(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::erase(a_actors, nullptr);
		if (a_actors.size() < 2) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([a_actors]() {
			const PhysicsLock physics(a_actors.front());
			std::uint32_t     group = 0;
			for (const auto actor : a_actors) {
				const auto c = GetCharController(actor);
				const auto b = c ? c->GetBodyImpl() : nullptr;
				if (!b) {
					REX::WARN("IgnoreGroupCollision: {:08X} has no character controller", actor->GetFormID());
					continue;
				}
				const auto f = b->m_collisionFilterInfo;
				g_savedFilters.try_emplace(actor->GetFormID(), f);
				RE::CFilter cf{};
				cf.filter = f;
				if (group == 0) {
					group = cf.GetSystemGroup() != 0 ? cf.GetSystemGroup() : 0xF5A1;
				}
				cf.SetSystemGroup(group);
				const auto ok = c->SetCollisionFilterInfo(cf);
				REX::INFO("IgnoreGroupCollision: {:08X} filter {:08X} -> {:08X} ({})", actor->GetFormID(), f, cf.filter, ok);
			}
		});
	}

	// Puts the whole cast on the pending furniture's spot, or else on the
	// player's own spot (or the first actor's, without the player).
	void PlaceGroup(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::erase(a_actors, nullptr);
		if (a_actors.empty() || PlaceOnPendingFurniture(a_actors)) {
			return;
		}
		RE::Actor* anchor = a_actors.front();
		for (const auto actor : a_actors) {
			if (IsPlayer(actor)) {
				anchor = actor;
			}
		}
		Furniture::Spot spot{ anchor->data.location, anchor->data.angle.z };
		for (const auto actor : a_actors) {
			if (actor != anchor) {
				PlaceActorAt(actor, spot);
			}
		}
	}

	void PlayGroupIdles(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_sceneID)
	{
		std::erase(a_actors, nullptr);
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() != a_actors.size()) {
			REX::WARN("PlayGroupIdles: no {}-actor scene \"{}\"", a_actors.size(), a_sceneID);
			std::vector<std::uint32_t> failed;
			for (const auto actor : a_actors) {
				failed.push_back(actor->GetFormID());
			}
			ReleaseFailedStart(std::move(failed), "no such scene");
			return;
		}
		const int  speed = StartSpeed(*scene);  // OStim: a new thread starts at the scene's defaultSpeed
		const auto idles = scene->speeds[speed];
		const auto sceneID = scene->id;
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		F4SE::GetTaskInterface()->AddTask([ids, idles, sceneID, speed]() {
			bool        any = false;
			bool        withPlayer = false;
			std::string results;
			for (std::size_t role = 0; role < ids.size(); ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
				if (ok) {
					CuePlayed(ids[role], idles[role], sceneID);
				}
				any = any || ok;
				withPlayer = withPlayer || IsPlayer(actor);
				results += std::format("{}{:08X} {}", role ? ", " : "", ids[role], ok ? "ok" : "refused");
			}
			REX::INFO("PlayGroupIdles: \"{}\": {}", sceneID, results);
			const bool focus = g_focusNextScene.exchange(false) || withPlayer;
			if (!any) {
				ReleaseFailedStart(ids, "the game refused every animation");
				return;
			}
			TrackSceneStart(ids, sceneID, speed);
			if (focus) {
				auto focused = FocusFor(ids, sceneID);
				focused.speed = speed;
				if (const auto active = FindActiveScene(ids.front())) {
					focused.furniture = active->furnitureType;
				}
				SetPlayerScene(std::move(focused));
			}
		});
	}

	void StopGroup(std::monostate, std::vector<RE::Actor*> a_actors)
	{
		std::erase(a_actors, nullptr);
		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopGroup: could not find IdleStop in Fallout4.esm");
			return;
		}
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		F4SE::GetTaskInterface()->AddTask([ids, idle]() {
			StopAgainIfJustPlayed(ids, idle);
			for (const auto id : ids) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, idle, nullptr);
				REX::INFO("StopGroup: {:08X} IdleStop returned {}", id, ok);
				ClearFocusIfIn(id);
				TrackSceneStop(id);
			}
		});
	}

	// ---- Player controls during scenes ----
	// A dedicated input-enable layer, held for the whole scene, blocks
	// everything a scene shouldn't allow (menus, favorites, combat, activate,
	// jumping, VATS...) while leaving looking enabled. Movement is toggled
	// by the scene view: on in the free camera (which flies with the
	// movement keys), off in first person. Replaces SetPlayerAIDriven, which
	// also disabled the look/move input the free camera needs.

	RE::BSTSmartPointer<RE::BSInputEnableLayer> g_sceneLayer;

	void SetLayerUserEvents(std::int32_t a_flags, bool a_enable)
	{
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (manager && g_sceneLayer) {
			manager->EnableUserEvent(g_sceneLayer->layerID, static_cast<RE::UserEvents::USER_EVENT_FLAG>(a_flags), a_enable, RE::UserEvents::SENDER_ID::kScript);
		}
	}

	void SetLayerOtherEvents(std::int32_t a_flags, bool a_enable)
	{
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (manager && g_sceneLayer) {
			manager->EnableOtherEvent(g_sceneLayer->layerID, static_cast<RE::OtherInputEvents::OTHER_EVENT_FLAG>(a_flags), a_enable, RE::UserEvents::SENDER_ID::kScript);
		}
	}

	constexpr std::int32_t flag(RE::UserEvents::USER_EVENT_FLAG a_flag) { return static_cast<std::int32_t>(a_flag); }
	constexpr std::int32_t flag(RE::OtherInputEvents::OTHER_EVENT_FLAG a_flag) { return static_cast<std::int32_t>(a_flag); }

	void LockPlayerControls(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto manager = RE::BSInputEnableManager::GetSingleton();
			if (!manager || g_sceneLayer) {
				return;
			}
			if (!manager->AllocateNewLayer(g_sceneLayer, "4Stim") || !g_sceneLayer) {
				REX::WARN("LockPlayerControls: couldn't allocate an input layer");
				return;
			}
			using U = RE::UserEvents::USER_EVENT_FLAG;
			using O = RE::OtherInputEvents::OTHER_EVENT_FLAG;
			SetLayerUserEvents(flag(U::kActivate) | flag(U::kMenu) | flag(U::kPOVSwitch) | flag(U::kFighting) | flag(U::kSneaking) |
								   flag(U::kMainFour) | flag(U::kJumping) | flag(U::kVATS),
				false);
			SetLayerOtherEvents(flag(O::kJournalTabs) | flag(O::kActivation) | flag(O::kFastTravel) | flag(O::kPOVChange) | flag(O::kVATS) |
									flag(O::kFavorites) | flag(O::kPipboyLight) | flag(O::kZKey) | flag(O::kRunning) | flag(O::kSprinting),
				false);
			REX::INFO("LockPlayerControls: layer {} active", g_sceneLayer->layerID);
		});
	}

	void ReleaseSceneLayer()  // main thread only
	{
		if (g_sceneLayer) {
			SetLayerUserEvents(flag(RE::UserEvents::USER_EVENT_FLAG::kAll), true);
			SetLayerOtherEvents(flag(RE::OtherInputEvents::OTHER_EVENT_FLAG::kAll), true);
			g_sceneLayer.reset();
			REX::INFO("Player controls restored");
		}
	}

	void UnlockPlayerControls(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() { ReleaseSceneLayer(); });
	}

	// ---- Scene camera and HUD ----
	// While a scene with the player (or one the player chose to watch) runs,
	// the vanilla HUD is hidden and the camera is the third-person free
	// camera, slowed by fFreeCameraSpeed from 4Stim.ini. At the end the HUD
	// comes back, the speed is restored, and the camera returns to the view
	// the player started in. The camera's real state is queried each time
	// (QCameraEquals) so it can't get out of step with the game.
	// (A first-person option is shelved: the game's first-person mode hides
	// the player's body, so it needs a head-pinned camera instead.)

	std::atomic<bool> g_sceneCameraActive = false;
	bool              g_startedFirstPerson = false;
	bool              g_startViewSaved = false;
	float             g_savedFreeCamSpeed = -1.0f;  // < 0: nothing saved

	constexpr auto FREE_CAM_SPEED_SETTING = "fFreeCameraTranslationSpeed:Camera"sv;

	void SetHUDVisible(bool a_visible)
	{
		if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage("HUDMenu", a_visible ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
		}
	}

	void ScaleFreeCameraSpeed()
	{
		const auto setting = RE::GetINISetting(FREE_CAM_SPEED_SETTING);
		if (!setting) {
			REX::WARN("Scene camera: game setting {} not found, free camera speed unchanged", FREE_CAM_SPEED_SETTING);
			return;
		}
		if (g_savedFreeCamSpeed < 0.0f) {
			g_savedFreeCamSpeed = setting->GetFloat();
		}
		// As OStim: the speed is set outright (SetCameraSpeed). The older
		// fFreeCameraSpeed multiplied the game's own instead.
		const float speed = g_settings.freeCamSpeedMult > 0.0F ? g_savedFreeCamSpeed * g_settings.freeCamSpeedMult : g_settings.freeCamSpeed;
		setting->SetFloat(speed);
		REX::INFO("Scene camera: free camera speed {} -> {}", g_savedFreeCamSpeed, setting->GetFloat());
	}

	// The world FOV during the scene (OStim's SetFreeCamFOV), put back after.
	float g_savedWorldFOV = -1.0F;  // < 0: nothing saved

	void SetSceneFOV()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || g_settings.freeCamFOV <= 0.0F) {
			return;
		}
		if (g_savedWorldFOV < 0.0F) {
			g_savedWorldFOV = camera->worldFOV;
		}
		camera->worldFOV = g_settings.freeCamFOV;
		REX::INFO("Scene camera: FOV {} -> {}", g_savedWorldFOV, camera->worldFOV);
	}

	void RestoreFOV()
	{
		if (g_savedWorldFOV < 0.0F) {
			return;
		}
		if (const auto camera = RE::PlayerCamera::GetSingleton()) {
			camera->worldFOV = g_savedWorldFOV;
		}
		g_savedWorldFOV = -1.0F;
	}

	// OStim's SetCustomTimescale: the game's TimeScale during a scene with
	// the player (0 = unchanged), put back after. Fallout4.esm's TimeScale
	// global is 0x3A, as Skyrim.esm's.
	float g_savedTimescale = -1.0F;  // < 0: nothing saved

	RE::TESGlobal* TimescaleGlobal()
	{
		return RE::TESForm::GetFormByID<RE::TESGlobal>(0x3A);
	}

	void SetSceneTimescale()
	{
		const auto global = g_settings.customTimescale > 0.0F ? TimescaleGlobal() : nullptr;
		if (!global) {
			return;
		}
		if (g_savedTimescale < 0.0F) {
			g_savedTimescale = global->value;
		}
		global->value = g_settings.customTimescale;
		REX::INFO("Scene camera: time scale {} -> {} (SetCustomTimescale)", g_savedTimescale, global->value);
	}

	void RestoreTimescale()
	{
		if (g_savedTimescale < 0.0F) {
			return;
		}
		if (const auto global = TimescaleGlobal()) {
			global->value = g_savedTimescale;
		}
		g_savedTimescale = -1.0F;
	}

	void RestoreFreeCameraSpeed()
	{
		if (g_savedFreeCamSpeed < 0.0f) {
			return;
		}
		if (const auto setting = RE::GetINISetting(FREE_CAM_SPEED_SETTING)) {
			setting->SetFloat(g_savedFreeCamSpeed);
		}
		g_savedFreeCamSpeed = -1.0f;
	}

	// Enters the free camera from third person. First person hides the
	// player's body (only the first-person arms are drawn), and entering the
	// free camera straight from first person keeps it hidden, so switch to
	// third person first and wait -- the game blends between views over a
	// few frames -- until it has actually taken effect. Main thread only.
	void EnterFreeCamera(int a_attempt)
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive || camera->QCameraEquals(RE::CameraState::kFree)) {
			return;
		}
		if (!camera->QCameraEquals(RE::CameraState::k3rdPerson)) {
			if (a_attempt == 0) {
				camera->SetState(camera->cameraStates[RE::CameraState::k3rdPerson].get());
			}
			if (a_attempt < 60) {  // about a second at 60 fps
				F4SE::GetTaskInterface()->AddTask([a_attempt]() { EnterFreeCamera(a_attempt + 1); });
				return;
			}
			REX::WARN("Scene camera: third person didn't take effect, entering free camera anyway");
		}
		camera->ToggleFreeCameraMode(false);  // false = don't freeze time
		REX::INFO("Scene camera: free camera");
	}

	// Records whether the player is in first person, so the scene can force
	// third person (Game.ForceThirdPerson, the game's full view change that
	// also swaps the first-person arms for the body) and still return the
	// player to first person afterwards. Call before forcing third person.
	void SaveStartView(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (camera && !g_sceneCameraActive) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
				g_startViewSaved = true;
			}
			g_focusNextScene = true;  // the scene being prepared is one the player watches
		});
	}

	// A loading screen or a screen fade is up (a teleport, a cell load).
	// Switching to the free camera then can leave the game stuck on the
	// loading screen, so the scene camera waits for it to clear.
	bool LoadingOrFading()
	{
		const auto ui = RE::UI::GetSingleton();
		return ui && (ui->GetMenuOpen("LoadingMenu") || (!g_sceneFade && ui->GetMenuOpen("FaderMenu")));
	}

	// Papyrus: FourStim.UseFades() / SetSceneFade (FourStimMenu's fades,
	// OStim's SetUseFades).
	bool UseFades(std::monostate)
	{
		return g_settings.useFades;
	}

	void SetSceneFade(std::monostate, bool a_fading)
	{
		g_sceneFade = a_fading;
	}

	// Runs a_task on the main thread once no loading screen or fade is up
	// (or after 20 s regardless).
	void AfterLoading(std::function<void()> a_task)
	{
		if (!LoadingOrFading()) {
			F4SE::GetTaskInterface()->AddTask(std::move(a_task));
			return;
		}
		REX::INFO("Scene camera: waiting for a loading screen / fade to clear");
		// Checked every 50 ms (20 s at most), then a little longer: the game
		// finishes settling after the menu closes.
		Scheduler::WhileThenOnMain(std::chrono::milliseconds(50), 400, []() { return LoadingOrFading(); }, std::chrono::milliseconds(250), std::move(a_task));
	}

	void BeginSceneCamera(std::monostate)
	{
		AfterLoading([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (!camera || g_sceneCameraActive) {
				return;
			}
			// The start this camera was for just failed (ReleaseFailedStart):
			// no camera, or nothing would ever end it.
			if (!GetPlayerScene().Active() && NowMs() - g_failedStartAt.load() < 3000) {
				REX::INFO("Scene camera: not started, the scene didn't start");
				return;
			}
			if (!g_startViewSaved) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
			}
			g_sceneCameraActive = true;
			SetHUDVisible(false);
			SetSceneFOV();
			SetSceneTimescale();
			if (g_settings.useFreeCam) {
				ScaleFreeCameraSpeed();
				EnterFreeCamera(0);
			}
		});
	}

	void EndSceneCamera(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			if (!g_sceneCameraActive) {
				return;
			}
			g_sceneCameraActive = false;
			g_startViewSaved = false;
			if (const auto camera = RE::PlayerCamera::GetSingleton()) {
				if (camera->QCameraEquals(RE::CameraState::kFree)) {
					camera->ToggleFreeCameraMode(false);
				}
				// As OStim's endSceneMode: third person, or first person with
				// SetForceFirstPerson (until 0.5.0 4Stim went back to the view
				// the scene started in). g_startedFirstPerson is still noted,
				// for the log.
				const bool firstPerson = g_settings.forceFirstPerson;
				const auto restore = firstPerson ? RE::CameraState::kFirstPerson : RE::CameraState::k3rdPerson;
				if (!camera->QCameraEquals(restore)) {
					camera->SetState(camera->cameraStates[restore].get());
				}
			}
			RestoreFreeCameraSpeed();
			RestoreFOV();
			RestoreTimescale();
			SetHUDVisible(true);
			REX::INFO("Scene camera: restored ({}; the scene started in {})", g_settings.forceFirstPerson ? "first person" : "third person",
				g_startedFirstPerson ? "first person" : "third person");
		});
	}

	// What a player scene changes outside the scene itself, put back: the
	// input layer, the scene camera's flags, free camera speed, FOV, slow
	// motion and the hidden HUD. For a game loaded (or started) while the
	// player was in a scene: none of these are saved, and none of them end
	// with the old game, so they stayed (the player's controls locked, the
	// camera zoomed). OStim resets its state on every load. The camera's own
	// state comes from the save, so it isn't touched. Main thread.
	void ResetPlayerSceneState(std::string_view a_why)
	{
		const bool wasActive = g_sceneCameraActive.exchange(false);
		const bool locked = static_cast<bool>(g_sceneLayer);
		g_startViewSaved = false;
		g_focusNextScene = false;
		g_sceneFade = false;
		RestoreFreeCameraSpeed();
		RestoreFOV();
		RestoreTimescale();
		EndSlowMotion();
		ReleaseSceneLayer();
		if (locked && g_vm) {
			// The player's AI driving (LockForScene) goes with the scene.
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "Game"sv, "SetPlayerAIDriven"sv, callback, false);
		}
		if (wasActive) {
			SetHUDVisible(true);
		}
		if (wasActive || locked) {
			REX::INFO("Player scene state reset ({}): controls, camera speed, FOV and HUD back", a_why);
		}
	}

	// OStim's SetFreeCamToggleKey: in and out of the free camera during a
	// scene. Main thread.
	void ToggleSceneFreeCamera()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive) {
			return;
		}
		if (camera->QCameraEquals(RE::CameraState::kFree)) {
			camera->ToggleFreeCameraMode(false);
			RestoreFreeCameraSpeed();
			REX::INFO("Scene camera: free camera off");
		} else {
			ScaleFreeCameraSpeed();
			EnterFreeCamera(0);
		}
	}

	// The scene moved (a furniture change): out of the free camera and back
	// in, so it starts at the player again. Main thread.
	void RecenterFreeCamera()
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive || !camera->QCameraEquals(RE::CameraState::kFree)) {
			return;
		}
		camera->ToggleFreeCameraMode(false);
		F4SE::GetTaskInterface()->AddTask([]() { EnterFreeCamera(0); });
	}

	// ---- Speed and navigation (the player's current scene) ----
	// Both replay idles on the actors already in place: same spot, same
	// collision setup, both roles started on the same frame. Navigation keeps
	// the roles (destination scenes always have the same number of actors)
	// and keeps the current speed where the destination has that many.

	// Plays speed a_speed of a_scene on the player's scene actors, on the main
	// thread, and records the new state.
	void PlayPlayerScene(const SceneRegistry::Scene& a_scene, int a_speed, const PlayerScene& a_current)
	{
		const auto  idles = a_scene.speeds[a_speed];
		const int   count = static_cast<int>(a_scene.speeds.size());
		PlayerScene next = a_current;
		next.sceneID = a_scene.id;
		next.speed = a_speed;
		F4SE::GetTaskInterface()->AddTask([idles, next, count, previous = a_current]() {
			// The scene may have ended or moved on since this was queued (an
			// End right after a speed key, say): then don't bring it back.
			const auto now = GetPlayerScene();
			if (now.ActorIDs() != previous.ActorIDs() || now.sceneID != previous.sceneID || now.speed != previous.speed) {
				REX::INFO("Player scene: change to \"{}\" speed {} dropped, the scene changed first", next.sceneID, next.speed + 1);
				return;
			}
			const auto  ids = next.ActorIDs();
			std::string results;
			for (std::size_t role = 0; role < idles.size() && role < ids.size(); ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
				if (ok) {
					CuePlayed(ids[role], idles[role], next.sceneID);
				}
				results += std::format("{}{:08X} {}", results.empty() ? "" : ", ", ids[role], ok ? "ok" : process ? "refused" : "no AI process");
				if (!ok && process) {
					// Refused (a tester on 1.10.163: the player stood still while
					// their partner moved on): once more on the next frame.
					F4SE::GetTaskInterface()->AddTask([id = ids[role], idle = idles[role], sceneID = next.sceneID]() {
						const auto again = RE::TESForm::GetFormByID<RE::Actor>(id);
						const auto process = again ? again->currentProcess : nullptr;
						const bool ok = process && GetPlayerScene().sceneID == sceneID && process->PlayIdle(*again, idle, nullptr);
						if (ok) {
							CuePlayed(id, idle, sceneID);
						}
						REX::INFO("Player scene: {:08X} PlayIdle again: {}", id, ok ? "ok" : "refused again");
					});
				}
			}
			SetPlayerScene(next);
			REX::INFO("Player scene: \"{}\" speed {}: PlayIdle {}", next.sceneID, next.speed + 1, results);
			TrackSceneChange(next.role0, next.sceneID, next.speed);
			if (next.sceneID != previous.sceneID) {
				SceneEvents::SceneChanged(next.ActorIDs(), previous.sceneID, next.sceneID, ThreadIDOf(next.role0), next.speed);
			} else if (next.speed != previous.speed) {
				SceneEvents::SpeedChanged(next.ActorIDs(), next.sceneID, next.speed, count, ThreadIDOf(next.role0));
			}
		});
	}

	// Changes the player's scene speed by a_delta. Returns the new speed
	// (1-based), or 0 if the player isn't in a scene.
	// OStim's NoPlayerControl (a thread builder's): the player's own speed,
	// navigation and auto mode keys leave this scene alone.
	bool NoPlayerControl(std::uint32_t a_actorID)
	{
		const auto active = a_actorID ? FindActiveScene(a_actorID) : nullptr;
		return active && active->noPlayerControl;
	}

	std::int32_t ChangeSceneSpeedImpl(std::int32_t a_delta)
	{
		const auto current = GetPlayerScene();
		const auto scene = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		if (!scene || NoPlayerControl(current.role0)) {
			return 0;
		}
		const int count = static_cast<int>(scene->speeds.size());
		const int speed = std::clamp(current.speed + a_delta, 0, count - 1);
		if (speed != current.speed) {
			PlayPlayerScene(*scene, speed, current);
		}
		return speed + 1;
	}

	std::int32_t ChangeSceneSpeed(std::monostate, std::int32_t a_delta)
	{
		return ChangeSceneSpeedImpl(a_delta);
	}

	// Moves the player's scene to a_destination (normally one of the current
	// scene's navigation links). Returns false if that isn't possible.
	// a_search: picked in the search picker, which goes anywhere, as OStim's
	// search menu (warpTo), sequence or not.
	bool NavigateSceneImpl(const std::string& a_destination, bool a_search = false)
	{
		const auto current = GetPlayerScene();
		const auto from = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		const auto to = SceneRegistry::Find(a_destination);
		if (NoPlayerControl(current.role0)) {
			REX::INFO("NavigateScene: this scene was started with NoPlayerControl");
			return false;
		}
		if (!a_search && FourStim::FocusedInSequence()) {
			REX::INFO("NavigateScene: not during a sequence (as OStim's scene menu)");
			return false;
		}
		if (!from || !to || to->actors.size() != from->actors.size()) {
			REX::WARN("NavigateScene: can't go to \"{}\"", a_destination);
			return false;
		}
		if (!SexesFit(*to, current.ActorIDs())) {
			REX::WARN("NavigateScene: \"{}\" is for other sexes than this scene's actors (bMatchSex)", a_destination);
			return false;
		}
		if (!SceneFitsFurniture(*to, current.furniture)) {
			REX::WARN("NavigateScene: \"{}\" is for {} furniture, this scene is on {}", a_destination,
				to->furniture.empty() ? "no" : to->furniture, current.furniture.empty() ? "none" : current.furniture);
			return false;
		}
		if (from->IsTransition()) {
			// Let the transition finish, then go there instead of its
			// destination (no jump out of the middle of it).
			F4SE::GetTaskInterface()->AddTask([actorID = current.role0, sceneID = to->id]() {
				if (const auto active = FindActiveScene(actorID)) {
					active->queuedScene = sceneID;
					REX::INFO("NavigateScene: \"{}\" queued until the transition ends", sceneID);
				}
			});
			return true;
		}
		const auto active = FindActiveScene(current.role0);
		const int  speed = active ? MappedSpeed(*active, *to) : std::min(current.speed, static_cast<int>(to->speeds.size()) - 1);
		// As OStim's Navigate (warpTo with the node's fadeOnEntry): into a
		// scene marked fadeOnEntry, the screen fades to black, the scene
		// changes 0.7 s in, and it fades back 0.55 s later.
		const bool withPlayer = std::ranges::any_of(current.ActorIDs(), [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		if (to->fadeOnEntry && withPlayer && g_vm && !g_sceneFade) {
			REX::INFO("NavigateScene: fading to \"{}\" (fadeOnEntry)", to->id);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeToBlack"sv, callback);
			Scheduler::AfterOnMain(std::chrono::milliseconds(700), [to, speed, role0 = current.role0]() {
				const auto now = GetPlayerScene();
				if (now.role0 == role0 && FindActiveScene(role0)) {
					PlayPlayerScene(*to, speed, now);
				}
			});
			Scheduler::After(std::chrono::milliseconds(700 + 550), []() {
				if (g_vm) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> after;
					Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeFromBlack"sv, after, 0.0F);
				}
			});
			return true;
		}
		PlayPlayerScene(*to, speed, current);
		return true;
	}

	bool NavigateScene(std::monostate, std::string a_destination)
	{
		return NavigateSceneImpl(a_destination);
	}

	// Fills {n} in a navigation label with the name of the actor in role n,
	// and capitalizes the first letter.
	std::string FormatNavLabel(std::string a_label, const PlayerScene& a_scene)
	{
		const auto ids = a_scene.ActorIDs();
		for (std::size_t role = 0; role < ids.size(); ++role) {
			const std::string token = "{" + std::to_string(role) + "}";
			std::string       name = "partner";
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role])) {
				if (const auto n = actor->GetDisplayFullName(); n && *n) {
					name = n;
				}
			}
			for (auto pos = a_label.find(token); pos != std::string::npos; pos = a_label.find(token, pos + name.size())) {
				a_label.replace(pos, token.size(), name);
			}
		}
		if (!a_label.empty()) {
			a_label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(a_label[0])));
		}
		return a_label;
	}

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
	std::atomic<int>                                   g_navHeld = 0;
	std::atomic<std::chrono::steady_clock::time_point> g_navHeldSince{};
	std::atomic<std::uint32_t>                         g_navPressCount = 0;

	constexpr auto PICKER_MENU = "FourStimPickerMenu";
	constexpr auto END_SCENE_ID = "__end__";
	constexpr auto CHANGE_FURNITURE_ID = "__furniture__";
	constexpr auto STOP_WATCHING_ID = "__stopwatching__";

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

	enum class PickerMode
	{
		kStart,     // start a new scene: who, where, then an idle (see StartStep)
		kNavigate,  // the focused scene's navigation links, plus "End scene" (used when the HUD isn't available)
		kSearch,    // every scene with the focused scene's actor count; picking one moves the scene there
		kFurniture,  // furniture near the focused scene; picking one moves the scene onto it (OStim's changeFurniture)
		kAsk         // a question with its answers (OStim's message boxes: the start flow's)
	};
	std::atomic<PickerMode>    g_pickerMode = PickerMode::kStart;

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
	std::mutex              g_questionLock;
	std::optional<Question> g_question;
	void                    Ask(Question a_question);
	// OStim's search menu: what's been typed in the picker's search mode
	// (UTF-8, main thread) and whether the picker is taking typing now.
	std::string                g_searchQuery;
	std::atomic<bool>          g_searchTyping = false;

	// kFurniture: the focused scene's role 0 and the furniture near it that
	// any scene fits its actors on (found when the list opened).
	std::mutex                    g_changeFurnitureLock;
	std::uint32_t                 g_changeFurnitureScene = 0;
	std::vector<Furniture::Found> g_changeFurniture;

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
	struct Candidate
	{
		std::uint32_t      id = 0;
		std::string        name;
		float              distance = 0.0F;
		SceneRegistry::Sex sex = SceneRegistry::Sex::kAny;
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

	// ---- Hotkey ----
	// Hooks MenuControls::PerformInputProcessing (vtable slot 0, from its
	// BSInputEventReceiver base), which receives the whole input queue every
	// frame during normal play. Registering a BSInputEventUser with
	// MenuControls::RegisterHandler succeeded but never received any input.

	bool g_shiftDown = false;  // main thread (the input hook)

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

	// The hotkey went down and waits to be let go (OStim's hold-for-solo).
	bool g_hotkeyHeld = false;

	// A scene with the player alone, as OStim's held hotkey (a thread
	// builder with the player: OStim's start flow follows).
	void StartSoloScene();

	bool IsHotkeyEvent(const RE::ButtonEvent* a_event)
	{
		const auto code = static_cast<std::uint32_t>(a_event->idCode);
		if (a_event->device.get() == RE::INPUT_DEVICE::kKeyboard) {
			return code != 0 && code == g_settings.hotkey;
		}
		return a_event->device.get() == RE::INPUT_DEVICE::kGamepad && code != 0 && code == g_settings.padHotkey;
	}

	// Out of a scene the hotkey acts when it's let go (a short press: the
	// picker; held: alone). In a scene with the HUD it switches the HUD's
	// focus at once, as before.
	bool HotkeyWaitsForRelease()
	{
		return !(GetPlayerScene().Active() && HUD::IsOpen());
	}

	void HandleButton(const RE::ButtonEvent* a_event)
	{
		// Hold-to-scroll tracking for the picker: arrow keys and d-pad up/down.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
			int        dir = 0;
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				dir = code == 0x26 ? -1 : code == 0x28 ? 1 : 0;  // arrow up / down
			} else if (device == RE::INPUT_DEVICE::kGamepad) {
				dir = code == 0x1 ? -1 : code == 0x2 ? 1 : 0;  // d-pad up / down
			}
			if (dir != 0) {
				if (a_event->QJustPressed()) {
					g_navHeldSince = std::chrono::steady_clock::now();
					++g_navPressCount;
					g_navHeld = dir;
				} else if (!a_event->QPressed() && g_navHeld == dir) {
					g_navHeld = 0;
				}
			}
		} else {
			g_navHeld = 0;
		}

		// The thread API's control callbacks (OStim's UIState::HandleControl):
		// the navigation keys in a scene the player is in or watches, with
		// no menu up. With an external UI, that's all they do.
		if (const auto ui = RE::UI::GetSingleton(); a_event->QJustPressed() && GetPlayerScene().Active() && ui && !ui->GetMenuOpen(PICKER_MENU) &&
													ui->menuMode == 0 && !ui->GetMenuOpen("Console")) {
			if (const auto name = HUD::NavigationInputName(a_event)) {
				using C = FourStimNG_API::Thread::Controls;
				const std::string_view n(name);
				const std::optional<C> control = n == "Up"sv ? C::Up : n == "Down"sv ? C::Down : (n == "Left"sv || n == "PadLeft"sv) ? C::Left :
				                                 (n == "Right"sv || n == "PadRight"sv) ? C::Right : n == "Accept"sv ? C::Yes :
				                                 n == "Toggle"sv ? C::Toggle : (n == "Cancel"sv || n == "Back"sv) ? C::No : std::optional<C>{};
				if (control) {
					API::NotifyNGControl(*control);
				}
				if (HUD::ExternalUI()) {
					return;
				}
			}
		}

		// The HUD, while focused, gets the navigation keys (both press and
		// release), unless the console, a pausing menu or the picker is up.
		if (const auto ui = RE::UI::GetSingleton(); !HUD::ExternalUI() && HUD::IsFocused() && ui && !ui->GetMenuOpen(PICKER_MENU) &&
													ui->menuMode == 0 && !ui->GetMenuOpen("Console")) {
			if (HUD::HandleInput(a_event)) {
				return;
			}
		}

		// Shift (either one), held or not, for Shift + the hotkey. Keyboard
		// codes are Windows virtual-key codes: 0x10 Shift, 0xA0 / 0xA1 left / right.
		if (a_event->device.get() == RE::INPUT_DEVICE::kKeyboard) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			if (code == 0x10 || code == 0xA0 || code == 0xA1) {
				g_shiftDown = a_event->QPressed();
			}
		}

		// OStim's hold-for-solo: the hotkey is acted on when it's let go.
		// Held 2 s or more, out of a scene: a scene with you alone (OStim
		// starts one when no one is under the crosshair; 4Stim's picker
		// finds the partner, so a short press opens it as before).
		if (a_event->QReleased() && g_hotkeyHeld && IsHotkeyEvent(a_event)) {
			g_hotkeyHeld = false;
			const auto ui = RE::UI::GetSingleton();
			if (ui && (ui->menuMode > 0 || ui->GetMenuOpen("Console"))) {
				return;
			}
			if (a_event->QHeldDownSecs() >= 2.0F && !GetPlayerScene().Active()) {
				REX::INFO("Hotkey held {:.1f} s: a scene with you alone", a_event->QHeldDownSecs());
				StartSoloScene();
			} else {
				RunKeyAction("hotkey");
			}
			return;
		}

		if (!a_event->QJustPressed()) {
			return;
		}

		// Escape hatch: Esc (or the controller's B) always closes the picker,
		// whatever its AS3 side is doing.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				if (const auto picker = TypingPicker()) {
					HandleTypingKey(picker, code);
				}
			}
			if ((device == RE::INPUT_DEVICE::kKeyboard && code == 0x1B) || (device == RE::INPUT_DEVICE::kGamepad && code == 0x2000)) {
				if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
					queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kHide);
				}
			}
			return;
		}

		// Ignore the hotkey while the console or a game-pausing menu is open.
		if (const auto ui = RE::UI::GetSingleton(); ui && (ui->menuMode > 0 || ui->GetMenuOpen("Console"))) {
			return;
		}

		// The controller's A in a scene you're in or watching, while the HUD is
		// hidden or doesn't have the keys: shows it and gives it the keys (B
		// hid it: 4Stim's controller scheme, docs/HUD.md).
		if (a_event->device.get() == RE::INPUT_DEVICE::kGamepad) {
			const auto button = static_cast<std::uint32_t>(a_event->idCode);
			// A (and with the Classic scheme the d-pad too, as OStim's
			// navigation keys bring its menu up) shows the HUD and gives it the keys.
			const bool wakes = button == 0x1000 || (HUD::PadClassic() && (button == 0x1 || button == 0x2 || button == 0x4 || button == 0x8));
			if (wakes && HUD::IsOpen() && (HUD::IsHidden() || !HUD::IsFocused()) && GetPlayerScene().Active()) {
				HUD::Show();
				return;
			}
			// The controller buttons set in [Controller] (none by default).
			if (button == 0) {
				return;
			}
			const auto& st = g_settings;
			const std::string_view padAction = button == st.padSpeedUp ? "speedup"sv :
			                                   button == st.padSpeedDown ? "speeddown"sv :
			                                   button == st.padAutoMode ? "automode"sv :
			                                   button == st.padPullOut ? "pullout"sv :
			                                   button == st.padEnd ? "end"sv :
			                                   button == st.padSearch ? "search"sv :
			                                   button == st.padAlign ? "align"sv :
			                                   button == st.padFreeCam ? "freecam"sv :
			                                   button == st.padHideUI ? "hideui"sv :
			                                   button == st.padNpcScene ? "npcscene"sv :
			                                   button == st.padHotkey ? "hotkey"sv :
			                                                            ""sv;
			if (padAction == "hotkey"sv && HotkeyWaitsForRelease()) {
				g_hotkeyHeld = true;
				return;
			}
			if (!padAction.empty()) {
				RunKeyAction(padAction);
			}
			return;
		}

		// Keyboard codes are Windows virtual-key codes (N = 0x4E).
		if (a_event->device.get() != RE::INPUT_DEVICE::kKeyboard) {
			return;
		}

		const auto code = static_cast<std::uint32_t>(a_event->idCode);
		if (code == 0) {
			return;
		}
		// OStim's navigation keys, while the HUD doesn't have the keys: as
		// OStim shows its bars on any of them, show the HUD and give it the
		// keys (it has them from then on, so the next press moves).
		if (HUD::IsNavigationKey(code) && HUD::IsOpen() && GetPlayerScene().Active()) {
			HUD::Show();
			return;
		}
		// The NPC scene key (or Shift + the hotkey) before the hotkey itself.
		const bool npcKey = g_settings.npcSceneKey != 0 ? code == g_settings.npcSceneKey : (code == g_settings.hotkey && g_shiftDown);
		const std::string_view action = code == g_settings.speedUpKey ? "speedup"sv :
		                                code == g_settings.speedDownKey ? "speeddown"sv :
		                                (g_autoConfig.key != 0 && code == g_autoConfig.key) ? "automode"sv :
		                                code == g_settings.pullOutKey ? "pullout"sv :
		                                code == g_settings.endKey ? "end"sv :
		                                code == g_settings.searchKey ? "search"sv :
		                                code == g_settings.alignmentKey ? "align"sv :
		                                code == g_settings.freeCamKey ? "freecam"sv :
		                                code == g_settings.hideUIKey ? "hideui"sv :
		                                npcKey ? "npcscene"sv :
		                                code == g_settings.hotkey ? "hotkey"sv :
		                                                            ""sv;
		if (action == "hotkey"sv && HotkeyWaitsForRelease()) {
			g_hotkeyHeld = true;
			return;
		}
		if (!action.empty()) {
			RunKeyAction(action);
		}
	}

	// What a scene key does (4Stim.ini's keys, and the MCM's hotkeys through
	// FourStim.PressKey). Main thread.
	void RunKeyAction(std::string_view a_action)
	{
		const auto focused = GetPlayerScene();
		if (a_action == "speedup" || a_action == "speeddown") {
			// Speed keys, while the player is in a scene.
			if (focused.role0 != 0) {
				ChangeSceneSpeedImpl(a_action == "speedup" ? 1 : -1);
			}
		} else if (a_action == "automode") {
			// On / off for the scene you're in or watching.
			if (const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr; active && !active->noPlayerControl) {
				if (active->autoMode.on) {
					StopAutoMode(*active);
				} else {
					StartAutoMode(*active);
				}
				Notify(active->autoMode.on ? TR("$4stim_auto_mode_on", "Auto mode on") : TR("$4stim_auto_mode_off", "Auto mode off"));
			}
		} else if (a_action == "pullout" || a_action == "end" || a_action == "search" || a_action == "align" || a_action == "freecam" || a_action == "hideui") {
			// OStim's other scene keys, for the scene you're in or watching:
			// as OStim's EventListener.
			if (!focused.Active()) {
				return;
			}
			if (a_action == "pullout") {
				// OStim's No key in its align menu (4Stim: the pull-out key,
				// with the Align tab open): the next step size.
				if (HUD::CycleAlignStep()) {
					return;
				}
				if (const auto active = FindActiveScene(focused.role0); active && !AutoPullOut(*active)) {
					Notify(TR("$4stim_no_pullout", "4Stim: nowhere to pull out to from here"));
				}
			} else if (a_action == "end") {
				if (g_vm) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					MarkPlayerSceneEnding();
					Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "EndPlayerScene"sv, callback);
				}
			} else if (a_action == "search") {
				API::NotifyNGControl(FourStimNG_API::Thread::Controls::SearchMenu);
				if (!HUD::ExternalUI()) {
					ShowPicker(PickerMode::kSearch);
				}
			} else if (a_action == "align") {
				API::NotifyNGControl(FourStimNG_API::Thread::Controls::AlignMenu);
				HUD::ToggleTab("align");
			} else if (a_action == "freecam") {
				ToggleSceneFreeCamera();
			} else {
				API::NotifyNGControl(FourStimNG_API::Thread::Controls::KEY_HIDE);
				HUD::ToggleHidden();
			}
		} else if (a_action == "npcscene") {
			// The picker for a scene without you, whether or not you're in one.
			if (g_vm) {
				REX::INFO("NPC scene key pressed");
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "OnNPCSceneHotkey"sv, callback,
					g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
			}
		} else if (a_action == "hotkey") {
			// In a scene with the HUD up, the hotkey switches the HUD's focus.
			if (focused.Active() && HUD::IsOpen()) {
				HUD::SetFocus(!HUD::IsFocused());
				return;
			}
			if (g_vm) {
				REX::INFO("Hotkey pressed");
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "OnHotkey"sv, callback,
					g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
			}
		} else {
			REX::WARN("Keys: unknown action \"{}\"", a_action);
		}
	}

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

	// ---- Animation events of scene actors ----
	// A transition idle played with the one-shot dyn_Activation event sends
	// "IdleStop" when its clip ends (and the actor starts blending back to
	// its base pose). If the timer hasn't moved the scene on by then, this
	// does, at once. With bLogAnimEvents=1, every event a scene actor gets
	// in the 5 s after a scene change is logged too (for animation authors).
	// Hooks ProcessEvent of the BSTEventSink<BSAnimationGraphEvent> base
	// (vtable 3, at +0x38) of Actor and PlayerCharacter.

	struct AnimWatch
	{
		std::string                           sceneID;
		std::chrono::steady_clock::time_point start;
	};
	std::mutex                                    g_animWatchLock;
	std::unordered_map<std::uint32_t, AnimWatch>  g_animWatch;

	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		std::scoped_lock lock(g_animWatchLock);
		const auto now = std::chrono::steady_clock::now();
		for (const auto id : a_actors) {
			g_animWatch[id] = { a_sceneID, now };
		}
	}

	struct AnimEventHook
	{
		using func_t = RE::BSEventNotifyControl (*)(RE::BSTEventSink<RE::BSAnimationGraphEvent>*, const RE::BSAnimationGraphEvent&, RE::BSTEventSource<RE::BSAnimationGraphEvent>*);
		static inline func_t originalActor = nullptr;
		static inline func_t originalPlayer = nullptr;

		static void Log(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event)
		{
			const auto ref = reinterpret_cast<RE::TESObjectREFR*>(reinterpret_cast<std::uintptr_t>(a_this) - 0x38);
			const char* tag = a_event.tag.c_str();
			if (tag && _strnicmp(tag, "4Stim", 5) == 0) {
				AnimCues::EngineForwarded(tag);  // the game sent one itself: 4Stim's own cue timing stands down
			}
			Handle(ref->GetFormID(), tag ? tag : "", a_event.payload.c_str() ? a_event.payload.c_str() : "", true);
		}

		// An animation event (a_fromEngine) or an annotation cue 4Stim read
		// from the animation file and timed itself (AnimCues).
		static void Handle(std::uint32_t id, const std::string& a_tag, const std::string& a_payload, bool a_fromEngine)
		{
			// 4Stim's annotations count for as long as the actor is in a scene
			// (checked on the main thread); the rest only shortly after a
			// scene change.
			const char* tag = a_tag.c_str();
			const bool  ours = tag && _strnicmp(tag, "4Stim", 5) == 0;
			std::string sceneID;
			float       seconds = 0.0F;
			{
				std::scoped_lock lock(g_animWatchLock);
				const auto it = g_animWatch.find(id);
				if (it == g_animWatch.end()) {
					seconds = 1000.0F;
				} else {
					seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - it->second.start).count();
					if (seconds > 60.0F) {  // long enough for any transition
						g_animWatch.erase(it);
					} else {
						sceneID = it->second.sceneID;
					}
				}
			}
			if (sceneID.empty() && !ours) {
				return;
			}
			if (g_settings.logAnimEvents && (seconds <= 5.0F || !a_fromEngine)) {
				REX::INFO("AnimEvent: {:08X} +{:.2f}s in \"{}\": \"{}\" ({}){}", id, seconds, sceneID,
					a_tag.c_str(), a_payload.c_str(), a_fromEngine ? "" : ", timed by 4Stim from the animation file");
			}
			if (_stricmp(a_tag.c_str(), "IdleStop") == 0) {
				// Animation threads: hand it to the main thread.
				F4SE::GetTaskInterface()->AddTask([id, sceneID]() {
					const auto active = FindActiveScene(id);
					if (!active || active->remaining < 0.0F || _stricmp(active->sceneID.c_str(), sceneID.c_str()) != 0) {
						return;
					}
					const auto scene = SceneRegistry::Find(active->sceneID);
					if (!scene || !scene->IsTransition()) {
						return;
					}
					REX::INFO("Transition \"{}\": clip ended ({:.2f}s before the timer), moving on now", scene->id, active->remaining);
					active->remaining = -1.0F;
					AdvanceAutoplay(*active);
				});
			} else if (_stricmp(a_tag.c_str(), "4StimClimax") == 0) {
				// The climax annotation (OStim's OStimClimax): this actor
				// climaxes now, as OStim's climax() does when its annotation
				// comes, whether or not they were waiting for it.
				F4SE::GetTaskInterface()->AddTask([id]() {
					if (const auto active = FindActiveScene(id)) {
						REX::INFO("Climax: {:08X} 4StimClimax annotation", id);
						ClimaxNow(*active, id);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimEvent") == 0) {
				// OStim's OStimEvent: "4StimEvent.<event>,<target>" (this actor
				// did it to role <target>) or "4StimEvent.<event>,<actor>,
				// <target>,<performer>" (roles).
				std::string payload = a_payload.c_str();
				F4SE::GetTaskInterface()->AddTask([id, payload]() {
					const auto active = FindActiveScene(id);
					if (!active) {
						return;
					}
					std::vector<std::string> parts;
					for (std::size_t start = 0;;) {
						const auto comma = payload.find(',', start);
						parts.push_back(payload.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
						if (comma == std::string::npos) {
							break;
						}
						start = comma + 1;
					}
					const int self = static_cast<int>(std::ranges::find(active->actors, id) - active->actors.begin());
					try {
						if (parts.size() == 2) {
							FireEvent(*active, parts[0], self, std::stoi(parts[1]), self);
						} else if (parts.size() == 4) {
							FireEvent(*active, parts[0], std::stoi(parts[1]), std::stoi(parts[2]), std::stoi(parts[3]));
						} else {
							REX::WARN("Events: 4StimEvent.{} on {:08X}: expected <event>,<target> or <event>,<actor>,<target>,<performer>", payload, id);
						}
					} catch (const std::exception&) {
						REX::WARN("Events: 4StimEvent.{} on {:08X}: roles must be numbers", payload, id);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimPeak") == 0) {
				// OStim's OStimPeak: "4StimPeak.<action index>", for actions
				// with "peaksAnnotated".
				std::string payload = a_payload.c_str();
				F4SE::GetTaskInterface()->AddTask([id, payload]() {
					try {
						Peaks::Annotated(id, std::stoi(payload));
					} catch (const std::exception&) {
						REX::WARN("Peaks: 4StimPeak.{} on {:08X}: the action must be a number", payload, id);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimPlayExpression") == 0) {
				// OStim's OStimPlayExpression: the event expression, no sound.
				std::string payload = a_payload.c_str();
				F4SE::GetTaskInterface()->AddTask([id, payload]() {
					if (FindActiveScene(id)) {
						std::string event = payload;
						std::ranges::transform(event, event.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						Expressions::SetEvent(id, event, true);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimBendSoS") == 0) {
				// OStim's OStimBendSoS: "<steps>" on top of the scene's bend.
				try {
					PenisBend::Offset(id, std::stoi(a_payload.c_str()));
				} catch (const std::exception&) {
					REX::WARN("Penis bend: 4StimBendSoS.{} on {:08X}: the bend must be a number", a_payload.c_str(), id);
				}
			} else if (_stricmp(a_tag.c_str(), "4StimResetSoS") == 0) {
				PenisBend::Offset(id, 0);  // OStim's OStimResetSoS
			} else if (_stricmp(a_tag.c_str(), "4StimPenisAim") == 0) {
				PenisBend::Aim(id, true);  // AnimBridge aimed this animation's penis: no scene bend on top
			} else if (_stricmp(a_tag.c_str(), "4StimUndress") == 0 || _stricmp(a_tag.c_str(), "4StimRedress") == 0) {
				// OStim's OStimUndress / OStimRedress: everything in the undress
				// slots off, or everything 4Stim took off back on.
				const bool undress = _stricmp(a_tag.c_str(), "4StimUndress") == 0;
				F4SE::GetTaskInterface()->AddTask([id, undress]() {
					if (!FindActiveScene(id)) {
						return;
					}
					if (undress) {
						Undress::StripAll(id);
					} else {
						Undress::Redress(id, true);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimUndressPartial") == 0 || _stricmp(a_tag.c_str(), "4StimRedressPartial") == 0) {
				// OStim's OStimUndressPartial / OStimRedressPartial: "<hex slot
				// mask>". 4Stim: Fallout 4's slots (bit 0 = slot 30), as
				// OActor.UndressPartial; OStim's masks are Skyrim's, so an
				// animation converted from OStim needs its mask mapped (EVENTS.md).
				const bool  undress = _stricmp(a_tag.c_str(), "4StimUndressPartial") == 0;
				std::string payload = a_payload.c_str();
				try {
					const auto mask = static_cast<std::uint32_t>(std::stoul(payload, nullptr, 16));
					F4SE::GetTaskInterface()->AddTask([id, undress, mask]() {
						if (!FindActiveScene(id)) {
							return;
						}
						if (undress) {
							Undress::StripMask(id, mask);
						} else {
							Undress::RedressMask(id, mask);
						}
					});
				} catch (const std::exception&) {
					REX::WARN("Undress: {}.{} on {:08X}: the slot mask must be hexadecimal", a_tag.c_str(), payload, id);
				}
			} else if (_stricmp(a_tag.c_str(), "4StimRemoveWeapons") == 0 || _stricmp(a_tag.c_str(), "4StimAddWeapons") == 0) {
				// OStim's OStimRemoveWeapons / OStimAddWeapons: the weapon off
				// (remembered, as at a scene's start) or back on.
				const std::string_view function = _stricmp(a_tag.c_str(), "4StimRemoveWeapons") == 0 ? "HolsterByID"sv : "RearmByID"sv;
				F4SE::GetTaskInterface()->AddTask([id, function]() {
					if (g_vm && FindActiveScene(id)) {
						RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
						Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, function, callback, static_cast<std::int32_t>(id));
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimSetLooking") == 0) {
				// OStim's OStimSetLooking: "<left>" or "<left>,<up>" (-100 to
				// 100; - is right / down), over the scene's look until the next
				// scene or 4StimResetLooking.
				std::string payload = a_payload.c_str();
				try {
					const auto  comma = payload.find(',');
					const float left = static_cast<float>(std::stoi(payload.substr(0, comma)));
					const float up = comma != std::string::npos ? static_cast<float>(std::stoi(payload.substr(comma + 1))) : 0.0F;
					EyeLook::Set(id, std::clamp(up, -100.0F, 100.0F), std::clamp(left, -100.0F, 100.0F));
				} catch (const std::exception&) {
					REX::WARN("Eyes: 4StimSetLooking.{} on {:08X}: expected <left> or <left>,<up>", payload, id);
				}
			} else if (_stricmp(a_tag.c_str(), "4StimUnsetLooking") == 0) {
				EyeLook::Set(id, 0.0F, 0.0F);  // OStim's OStimUnsetLooking: their eyes their own
			} else if (_stricmp(a_tag.c_str(), "4StimResetLooking") == 0) {
				// OStim's OStimResetLooking: the scene's look again (none: as Unset).
				F4SE::GetTaskInterface()->AddTask([id]() {
					const auto active = FindActiveScene(id);
					const auto scene = active ? SceneRegistry::RoleScene(SceneRegistry::Find(active->sceneID)) : nullptr;
					const auto role = active ? static_cast<std::size_t>(std::ranges::find(active->actors, id) - active->actors.begin()) : 0;
					if (scene && role < scene->actors.size()) {
						EyeLook::Set(id, scene->actors[role].lookUp, scene->actors[role].lookLeft);
					} else {
						EyeLook::Set(id, 0.0F, 0.0F);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimEquipObject") == 0 || _stricmp(a_tag.c_str(), "4StimUnequipObject") == 0) {
				// OStim's OStimEquipObject / OStimUnequipObject: "<type>".
				const bool  equip = _stricmp(a_tag.c_str(), "4StimEquipObject") == 0;
				std::string payload = a_payload.c_str();
				if (equip) {
					EquipObjects::Equip(id, payload);
				} else {
					EquipObjects::Unequip(id, payload);
				}
			}
		}

		static RE::BSEventNotifyControl ThunkActor(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			Log(a_this, a_event);
			return originalActor(a_this, a_event, a_source);
		}

		static RE::BSEventNotifyControl ThunkPlayer(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			Log(a_this, a_event);
			return originalPlayer(a_this, a_event, a_source);
		}

		// After game data loads (the player exists then): checks the player's
		// own vtable pointer at +0x38 against the one to patch first, so a
		// wrong table index can't patch the wrong function.
		static void Install()
		{
			static bool installed = false;
			if (installed) {
				return;
			}
			installed = true;
			REL::Relocation<std::uintptr_t> check{ RE::VTABLE::PlayerCharacter[3] };
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto actual = player ? *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(player) + 0x38) : 0;
			if (actual != check.address()) {
				REX::WARN("Animation event log: vtable check failed ({:X} vs {:X}), not installed", actual, check.address());
				return;
			}
			REL::Relocation<std::uintptr_t> actorVtbl{ RE::VTABLE::Actor[3] };
			originalActor = reinterpret_cast<func_t>(actorVtbl.write_vfunc(1, ThunkActor));
			REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE::PlayerCharacter[3] };
			originalPlayer = reinterpret_cast<func_t>(playerVtbl.write_vfunc(1, ThunkPlayer));
			REX::INFO("Animation event log hook installed");
		}
	};

	void HandleTimedCue(const AnimCues::Due& a_due)
	{
		AnimEventHook::Handle(a_due.actor, a_due.tag, a_due.payload, false);
	}

	// ---- OStim's thread API (docs/API.md): FourThread, FourThreadBuilder ----
	// A thread is a running scene; its ID is ActiveScene::threadID (0 = the
	// player's, as OStim's player thread). Reads happen where Papyrus calls
	// them, as the other natives here do; changes go to the main thread.

	ActiveScene* FindThread(std::int32_t a_threadID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) { return a_scene.threadID == a_threadID; });
		return it != g_activeScenes.end() ? &*it : nullptr;
	}

	std::int32_t ThreadIDOf(std::uint32_t a_actorID)
	{
		const auto active = FindActiveScene(a_actorID);
		return active ? active->threadID : -1;
	}

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

	std::string LowerCopy(std::string a_text)
	{
		std::ranges::transform(a_text, a_text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return a_text;
	}

	std::vector<std::string> SplitCSV(const std::string& a_csv)
	{
		std::vector<std::string> out;
		std::string              item;
		for (const char ch : a_csv + ",") {
			if (ch == ',') {
				const auto b = item.find_first_not_of(" \t");
				const auto e = item.find_last_not_of(" \t");
				if (b != std::string::npos) {
					out.push_back(item.substr(b, e - b + 1));
				}
				item.clear();
			} else {
				item += ch;
			}
		}
		return out;
	}

	// The scenes a new scene for a_cast can start with on a_furniture ("" =
	// off furniture), any role order that fits (a thread builder's actors
	// get OStim's order when it fits, else another).
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForActors(const std::vector<std::uint32_t>& a_cast, const std::string& a_furniture,
		std::optional<std::size_t> a_pinned = std::nullopt)
	{
		SceneRegistry::ListFilter filter;
		filter.actors = ConditionsOf(a_cast);
		filter.pinned = a_pinned;
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> out;
		for (const auto& summary : SceneRegistry::List(a_cast.size(), filter)) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && SceneFitsFurniture(*scene, a_furniture)) {
				out.push_back(scene);
			}
		}
		return out;
	}

	// Why a_actor isn't eligible for scenes at all, or "" if they are:
	// OStim's isEligible (Core.cpp: not disabled, deleted, a child or dead,
	// with an actor type, in no thread), which OThreadBuilder.Create checks.
	std::string NotEligible(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return "no actor";
		}
		if (a_actor->IsDisabled()) {
			return "disabled";
		}
		if (a_actor->IsDeleted()) {
			return "deleted";
		}
		if (a_actor->IsChild()) {
			return "children are never eligible";
		}
		if (a_actor->IsDead(false)) {
			return "dead";
		}
		if (ActorProperties::Type(a_actor).empty()) {
			return "no actor type (docs/ACTORS.md)";
		}
		if (FindActiveScene(a_actor->GetFormID())) {
			return "already in a scene";
		}
		if (InStartingThread(a_actor->GetFormID())) {
			return "already in a scene that's starting";
		}
		return {};
	}

	// Why a_actor can't be in a new scene now, or "" if they can (as
	// FourStimScene.CanUseActor): eligible, loaded (OStim's ThreadStarter),
	// and (4Stim) not in combat.
	std::string CantUse(RE::Actor* a_actor)
	{
		if (auto why = NotEligible(a_actor); !why.empty()) {
			return why;
		}
		if (a_actor->IsInCombat()) {
			return "in combat";
		}
		if (!a_actor->Get3D()) {
			return "not loaded";  // OStim's ThreadStarter: unloaded actors can't start
		}
		return {};
	}

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
	std::mutex                                        g_buildersLock;
	std::unordered_map<std::int32_t, ThreadBuilder>   g_builders;
	std::int32_t                                      g_nextBuilderID = 1;

	std::int32_t BuilderCreate(std::vector<RE::Actor*> a_actors)
	{
		ThreadBuilder builder;
		for (const auto actor : a_actors) {
			// As OStim's Create: -1 for an actor who isn't eligible.
			if (const auto why = NotEligible(actor); !why.empty()) {
				REX::WARN("ThreadBuilder.Create: {:08X} isn't eligible ({})", actor ? actor->GetFormID() : 0, why);
				return -1;
			}
			builder.actors.push_back(actor->GetFormID());
		}
		if (builder.actors.empty()) {
			REX::WARN("ThreadBuilder.Create: no actors");
			return -1;
		}
		std::scoped_lock lock(g_buildersLock);
		const auto id = g_nextBuilderID++;
		g_builders.emplace(id, std::move(builder));
		return id;
	}

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

	// OStim's Node::getRoute: the scenes from a_from to a_to through
	// navigations (transitions on the way included), at most a_steps
	// navigations, each fitting a_cast in role order; transitions play for
	// their length, other scenes on the way half a second (as OStim's 500
	// ms). Empty if there's no way.
	std::vector<SceneRegistry::SequenceEntry> RouteBetween(const std::shared_ptr<const SceneRegistry::Scene>& a_from, const std::shared_ptr<const SceneRegistry::Scene>& a_to,
		std::span<const SceneRegistry::ActorCondition> a_cast, int a_steps)
	{
		using Route = std::vector<std::shared_ptr<const SceneRegistry::Scene>>;
		std::vector<std::string>                                                   visited{ a_from->id };
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> frontier{ { a_from, {} } };
		for (int step = 0; step < a_steps && !frontier.empty(); ++step) {
			std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> next;
			for (const auto& [scene, route] : frontier) {
				for (const auto& nav : scene->navigations) {
					auto  dest = SceneRegistry::Find(nav.to);
					Route path = route;
					for (int hop = 0; dest && hop < 8; ++hop) {
						path.push_back(dest);
						if (!dest->IsTransition()) {
							break;
						}
						dest = SceneRegistry::Find(dest->destination);
					}
					if (!dest || dest->IsTransition() || !SceneRegistry::Fits(dest->actors, a_cast) || std::ranges::find(visited, dest->id) != visited.end()) {
						continue;
					}
					visited.push_back(dest->id);
					if (dest->id == a_to->id) {
						std::vector<SceneRegistry::SequenceEntry> out;
						for (const auto& s : path) {
							out.push_back({ s->id, s->IsTransition() ? std::max(s->length, 0.5F) : 0.5F, 0 });
						}
						return out;
					}
					next.emplace_back(dest, std::move(path));
				}
			}
			frontier = std::move(next);
		}
		return {};
	}

	// A starting entry with no time plays for its scene's length (OStim's
	// animationLengthMs; FourThreadBuilder.AddStartingAnimation's duration
	// is optional). A scene without a length gets this.
	constexpr float STARTING_NO_LENGTH = 10.0F;

	// OStim's ActorUtil::sort, before the player's place: the dominant
	// actors first, then those with a penis, otherwise as given.
	void SortLikeOStim(std::vector<std::uint32_t>& a_cast, const std::vector<std::uint32_t>& a_dominant)
	{
		std::ranges::stable_sort(a_cast, [&](std::uint32_t a, std::uint32_t b) {
			const bool domA = std::ranges::find(a_dominant, a) != a_dominant.end();
			const bool domB = std::ranges::find(a_dominant, b) != a_dominant.end();
			if (domA != domB) {
				return domA;
			}
			return ActorProperties::Cached(a).penis && !ActorProperties::Cached(b).penis;
		});
	}

	// What OStim's start flow decided for a player thread (StartFlow below).
	struct StartChoice
	{
		bool                            placeDecided = false;  // piece is the answer (none = no furniture)
		std::optional<Furniture::Found> piece;
		bool                            sorted = false;        // the cast is in role order already
		std::optional<std::size_t>      pin;                   // this actor keeps their place (the player's)
	};

	// The rest of a builder's start, on the main thread: the order, the
	// place, the scene, then Papyrus places the actors.
	void LaunchThread(const ThreadBuilder& builder, std::int32_t threadID, bool withPlayer, const StartChoice& a_choice)
	{
		{
			// Out of the start flow: the usual time to be placed from here.
			std::scoped_lock lock(g_threadsLock);
			if (const auto record = ThreadRecordLocked(threadID)) {
				record->flow = 0;
				record->since = std::chrono::steady_clock::now();
			}
		}
		auto cast = builder.actors;
		auto starting = builder.starting;
		// As OStim's: with starting scenes the caller's order is the role
		// order; if the first doesn't fit the actors in that order, a start
		// scene is picked instead (ThreadStarter.cpp).
		if (!starting.empty()) {
			const auto first = SceneRegistry::Find(starting.front().scene);
			if (!first || !SceneRegistry::Fits(first->actors, ConditionsOf(cast))) {
				REX::WARN("ThreadBuilder: thread {}: \"{}\" doesn't fit its actors in the order given; picking a start scene instead", threadID, starting.front().scene);
				starting.clear();
			}
		}
		if (starting.empty() && !a_choice.sorted) {
			SortLikeOStim(cast, builder.dominant);
		}

		// Where: the furniture given, else (unless NoFurniture) the nearest
		// that has a scene for them (with a starting scene: one that takes
		// it), as OStim's thread starters.
		std::optional<Furniture::Found> piece;
		if (builder.furniture) {
			if (const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(builder.furniture)) {
				const auto type = Furniture::Classify(ref);
				if (type.empty()) {
					REX::WARN("ThreadBuilder: {:08X} isn't furniture of any type, starting without it", builder.furniture);
				} else if (FurnitureBlocked(builder.furniture) || std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a) { return a.furnitureRef == builder.furniture; })) {
					REX::WARN("ThreadBuilder: {:08X} is in use, starting without it", builder.furniture);
				} else {
					piece = Furniture::Found{ type, {}, builder.furniture, 0.0F, std::nullopt };
				}
			}
		} else if (a_choice.placeDecided) {
			piece = a_choice.piece;  // the start flow's choice
		} else if (!builder.noFurniture && g_settings.useFurniture) {
			const auto firstActor = RE::TESForm::GetFormByID<RE::Actor>(cast.front());
			const auto firstScene = starting.empty() ? nullptr : SceneRegistry::Find(starting.front().scene);
			if (firstActor) {
				for (const auto& found : FurnitureNear(firstActor->data.location, firstActor)) {
					if (firstScene ? (!firstScene->furniture.empty() && SceneFitsFurniture(*firstScene, found.type)) : !ScenesForActors(cast, found.type).empty()) {
						piece = found;
						break;
					}
				}
			}
		}
		const std::string here = piece ? piece->type : std::string{};

		std::shared_ptr<const SceneRegistry::Scene> scene;
		std::vector<std::int32_t>                   ordered;
		if (!starting.empty()) {
			scene = SceneRegistry::Find(starting.front().scene);
			for (const auto id : cast) {
				ordered.push_back(static_cast<std::int32_t>(id));
			}
		} else {
			std::string why;
			scene = PickStartScene(ScenesForActors(cast, here, a_choice.pin), here, withPlayer, why);
			if (!scene && piece) {
				piece.reset();  // nothing fits there: right here instead
				scene = PickStartScene(ScenesForActors(cast, {}, a_choice.pin), {}, withPlayer, why);
			}
			if (!scene) {
				DropStartingThread(threadID, std::format("no scene for {} actor(s){}", cast.size(), here.empty() ? std::string{} : " on " + here));
				return;
			}
			// Who takes which role.
			const auto order = SceneRegistry::AssignRoles(scene->actors, ConditionsOf(cast), a_choice.pin);
			if (order.size() != cast.size()) {
				DropStartingThread(threadID, std::format("its actors don't fit \"{}\"'s roles", scene->id));
				return;
			}
			std::vector<std::uint32_t> roleOrder;
			for (const auto index : order) {
				ordered.push_back(static_cast<std::int32_t>(cast[index]));
				roleOrder.push_back(cast[index]);
			}
			cast = std::move(roleOrder);
		}
		{
			std::scoped_lock lock(g_threadsLock);
			if (const auto record = ThreadRecordLocked(threadID)) {
				record->roleActors = cast;
			}
		}

		if (piece) {
			PendingFurniture pending{ piece->ref, piece->type, ActorSlots<int>::Key(cast) };
			pending.offset = scene->furnitureOffset;
			pending.floorScene = scene->furniture.empty();
			pending.fixedSpot = piece->spot;
			SetPendingFurniture(std::move(pending));
		}
		// Starting scenes after the first: a sequence, as OStim's starting
		// nodes. An entry without a time plays for its scene's length;
		// navigateTo puts in the way there from the entry before. One
		// starting scene is a plain start, as OStim's (no sequence, no
		// sequence end).
		if (starting.size() > 1) {
			const auto conditions = ConditionsOf(cast);
			std::vector<SceneRegistry::SequenceEntry> entries;
			for (std::size_t i = 0; i < starting.size(); ++i) {
				auto       entry = starting[i];
				const auto at = SceneRegistry::Find(entry.scene);
				if (!at || at->actors.size() != cast.size()) {
					REX::WARN("ThreadBuilder: thread {}: starting scene \"{}\" left out (no such scene for {} actor(s))", threadID, entry.scene, cast.size());
					continue;
				}
				if (i > 0 && i < builder.navigateTo.size() && builder.navigateTo[i] && !entries.empty()) {
					const auto from = SceneRegistry::Find(entries.back().scene);
					if (from && from->id != at->id) {
						auto route = RouteBetween(from, at, conditions, g_autoConfig.maxSteps);
						if (!route.empty()) {
							route.pop_back();  // the entry itself, with its own time, below
							entries.insert(entries.end(), route.begin(), route.end());
						}
					}
				}
				if (entry.duration <= 0.0F) {
					entry.duration = at->length > 0.0F ? at->length : STARTING_NO_LENGTH;
				}
				entries.push_back(std::move(entry));
			}
			if (entries.size() > 1) {
				auto sequence = std::make_shared<SceneRegistry::Sequence>();
				sequence->id = std::format("thread {}", threadID);
				sequence->actorCount = cast.size();
				sequence->entries = std::move(entries);
				QueueSequence(std::move(sequence), cast);
			}
		}
		REX::INFO("ThreadBuilder: thread {} starting \"{}\"{}", threadID, scene->id, here.empty() ? std::string{} : " on " + here);
		if (g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			const std::int32_t anchor = withPlayer ? 0 : static_cast<std::int32_t>(cast.front());
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "StartPickedCast"sv, callback, scene->id, ordered, anchor);
		} else {
			DropStartingThread(threadID, "no script engine");
		}
	}

	// OStim's PlayerThreadStarter: a player thread from a script waits until
	// the player is out of dialogue, then asks (as OStim's message boxes, in
	// the picker) about furniture, adding actors near and the player's role,
	// as the settings say, and sorts the actors with PlayerAlways...; then
	// LaunchThread. Each step on the main thread.
	struct StartFlow
	{
		ThreadBuilder builder;
		std::int32_t  threadID = 0;
		std::uint64_t id = 0;
		StartChoice   choice;
	};
	using StartFlowPtr = std::shared_ptr<StartFlow>;
	std::atomic<std::uint64_t> g_nextStartFlow = 1;

	// Whether a_flow's thread is still waiting for it (not stopped, the game
	// not loaded since).
	bool FlowAlive(const StartFlow& a_flow)
	{
		std::scoped_lock lock(g_threadsLock);
		const auto record = ThreadRecordLocked(a_flow.threadID);
		return record && record->status == ThreadRecord::Status::kStarting && record->flow == a_flow.id;
	}

	// OStim's position names (PlayerThreadStarter.cpp).
	std::string PositionName(std::size_t a_index)
	{
		constexpr std::array names{ "Dom", "Sub", "Third", "Fourth", "Fifth", "Sixth", "Seventh", "Eighth", "Ninth" };
		return a_index < names.size() ? std::string(names[a_index]) : std::format("{}", a_index + 1);
	}

	void FlowLaunch(const StartFlowPtr& a_flow)
	{
		if (FlowAlive(*a_flow)) {
			LaunchThread(a_flow->builder, a_flow->threadID, true, a_flow->choice);
		}
	}

	// OStim's sortActors: the order, then the player moved to the position
	// picked (a_playerIndex), or for two, where PlayerAlways... says.
	void FlowSort(const StartFlowPtr& a_flow, int a_playerIndex)
	{
		auto&      cast = a_flow->builder.actors;
		const auto player = RE::PlayerCharacter::GetSingleton()->GetFormID();
		SortLikeOStim(cast, a_flow->builder.dominant);
		const int want = a_playerIndex >= 0 ? a_playerIndex : PlayerAlwaysIndex(cast);
		if (const auto at = std::ranges::find(cast, player); at != cast.end() && want >= 0 && static_cast<std::size_t>(want) < cast.size()) {
			cast.erase(at);
			cast.insert(cast.begin() + want, player);
			a_flow->choice.pin = static_cast<std::size_t>(want);
		}
		a_flow->choice.sorted = true;
		FlowLaunch(a_flow);
	}

	// OStim's handleActorSorting: asks the player's position, by
	// playerSelectRole... (for two: the same or not by penis, as OStim's).
	void FlowRole(const StartFlowPtr& a_flow)
	{
		if (!FlowAlive(*a_flow)) {
			return;
		}
		const auto& cast = a_flow->builder.actors;
		if (!a_flow->builder.starting.empty()) {
			FlowLaunch(a_flow);
			return;
		}
		bool select = false;
		if (cast.size() >= 3) {
			select = g_settings.playerSelectRoleThreesome;
		} else if (cast.size() == 2) {
			select = ActorProperties::Cached(cast[0]).penis == ActorProperties::Cached(cast[1]).penis ? g_settings.playerSelectRoleGay : g_settings.playerSelectRoleStraight;
		}
		if (!select) {
			FlowSort(a_flow, -1);
			return;
		}
		Question question;
		question.title = TR("$4stim_message_select_position", "Which position do you want to be in?");
		for (std::size_t i = 0; i < cast.size(); ++i) {
			question.answers.push_back({ PositionName(i), {} });
		}
		question.then = [a_flow](int a_index) { FlowSort(a_flow, a_index); };
		Ask(std::move(question));
	}

	// Whether any scene has a_count actors and plays on a_furniture ("" = off
	// furniture): OStim's GraphTable::hasNodes.
	bool HasScenesFor(std::size_t a_count, const std::string& a_furniture)
	{
		for (const auto& summary : SceneRegistry::List(a_count)) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && !scene->IsTransition() && SceneFitsFurniture(*scene, a_furniture)) {
				return true;
			}
		}
		return false;
	}

	// OStim's handleActorAdding (addActorsAtStart): "Add actor?" with the
	// eligible actors near the player, again after each one added, while
	// some scene takes one more on the place chosen.
	void FlowAddActors(const StartFlowPtr& a_flow)
	{
		if (!FlowAlive(*a_flow)) {
			return;
		}
		const auto& cast = a_flow->builder.actors;
		if (!a_flow->builder.starting.empty()) {
			FlowLaunch(a_flow);
			return;
		}
		const std::string here = a_flow->choice.piece ? a_flow->choice.piece->type : std::string{};
		if (!g_settings.addActorsAtStart || !HasScenesFor(cast.size() + 1, here)) {
			FlowRole(a_flow);
			return;
		}
		std::vector<Candidate> near;
		for (auto& c : FindCandidates(2000.0F)) {  // OStim's 2000
			if (std::ranges::find(cast, c.id) == cast.end() && CantUse(RE::TESForm::GetFormByID<RE::Actor>(c.id)).empty()) {
				near.push_back(std::move(c));
			}
		}
		if (near.empty()) {
			FlowRole(a_flow);
			return;
		}
		Question question;
		question.title = TR("$4stim_message_add_actor", "Add actor?");
		question.answers.push_back({ TR("$4stim_message_none", "None"), {} });
		for (const auto& c : near) {
			const char* sex = c.sex == SceneRegistry::Sex::kMale ? " (M)" : c.sex == SceneRegistry::Sex::kFemale ? " (F)" : "";
			question.answers.push_back({ c.name + sex, TRF("$4stim_meters_away", "{:.0f} m away", c.distance / 70.0F) });
		}
		question.then = [a_flow, near](int a_index) {
			if (a_index < 1 || static_cast<std::size_t>(a_index) > near.size() || !FlowAlive(*a_flow)) {
				FlowRole(a_flow);
				return;
			}
			const auto id = near[a_index - 1].id;
			if (!CantUse(RE::TESForm::GetFormByID<RE::Actor>(id)).empty()) {
				FlowAddActors(a_flow);  // busy since: ask again
				return;
			}
			a_flow->builder.actors.push_back(id);
			REX::INFO("ThreadBuilder: thread {}: {:08X} added", a_flow->threadID, id);
			{
				std::scoped_lock lock(g_threadsLock);
				if (const auto record = ThreadRecordLocked(a_flow->threadID)) {
					record->actors = ActorSlots<int>::Key(a_flow->builder.actors);
					record->roleActors = a_flow->builder.actors;
				}
			}
			FlowAddActors(a_flow);
		};
		Ask(std::move(question));
	}

	// OStim's handleFurniture. With a starting scene, OStim asks about the
	// nearest bed whatever the scene; 4Stim asks about the nearest furniture
	// the starting scene plays on (none for a floor scene), as its other
	// starts pick it.
	void FlowFurniture(const StartFlowPtr& a_flow)
	{
		if (!FlowAlive(*a_flow)) {
			return;
		}
		// As OStim's startThread, before the flow: starting scenes their
		// actors don't fit in the order given are dropped.
		if (auto& starting = a_flow->builder.starting; !starting.empty()) {
			const auto scene = SceneRegistry::Find(starting.front().scene);
			if (!scene || !SceneRegistry::Fits(scene->actors, ConditionsOf(a_flow->builder.actors))) {
				REX::WARN("ThreadBuilder: thread {}: \"{}\" doesn't fit its actors in the order given; picking a start scene instead", a_flow->threadID, starting.front().scene);
				starting.clear();
				a_flow->builder.navigateTo.clear();
			}
		}
		const auto& b = a_flow->builder;
		const auto  first = RE::TESForm::GetFormByID<RE::Actor>(b.actors.front());
		if (b.noFurniture || b.furniture || !g_settings.useFurniture || !first) {
			FlowAddActors(a_flow);
			return;
		}
		a_flow->choice.placeDecided = true;
		if (!b.starting.empty()) {
			const auto                      scene = SceneRegistry::Find(b.starting.front().scene);
			std::optional<Furniture::Found> piece;
			if (scene && !scene->furniture.empty()) {
				for (const auto& found : FurnitureNear(first->data.location, first)) {
					if (SceneFitsFurniture(*scene, found.type)) {
						piece = found;
						break;
					}
				}
			}
			if (!piece) {
				FlowAddActors(a_flow);
				return;
			}
			if (!g_settings.selectFurniture) {
				a_flow->choice.piece = piece;
				FlowAddActors(a_flow);
				return;
			}
			Question question;
			question.title = TRF("$4stim_message_use_furniture", "Do you want to use the nearby {}?", piece->name);
			question.answers.push_back({ TR("$4stim_message_yes", "Yes"), {} });
			question.answers.push_back({ TR("$4stim_message_no", "No"), {} });
			question.then = [a_flow, piece](int a_index) {
				if (a_index == 0) {
					a_flow->choice.piece = piece;
					FlowAddActors(a_flow);
				} else {
					FlowRole(a_flow);
				}
			};
			Ask(std::move(question));
			return;
		}
		// OStim's selectFurniture: the furniture near that some scene for
		// these actors plays on; asked (SetSelectFurniture), else the nearest.
		std::vector<Furniture::Found> found;
		for (auto& f : FurnitureNear(first->data.location, first)) {
			if (!ScenesForActors(b.actors, f.type).empty()) {
				found.push_back(std::move(f));
			}
		}
		if (found.empty()) {
			FlowAddActors(a_flow);
			return;
		}
		if (!g_settings.selectFurniture) {
			a_flow->choice.piece = found.front();
			FlowAddActors(a_flow);
			return;
		}
		Question question;
		question.title = TR("$4stim_message_select_furniture", "Which furniture do you want to use?");
		question.answers.push_back({ TR("$4stim_message_none", "None"), {} });
		for (const auto& f : found) {
			question.answers.push_back({ f.name, TRF("$4stim_meters_away", "{:.0f} m away", f.distance / 70.0F) });
		}
		question.then = [a_flow, found](int a_index) {
			if (a_index >= 1 && static_cast<std::size_t>(a_index) <= found.size()) {
				a_flow->choice.piece = found[a_index - 1];
			}
			FlowAddActors(a_flow);
		};
		Ask(std::move(question));
	}

	// OStim's startPlayerThread: waits (every 250 ms) while the player is in
	// dialogue, as scripts start scenes from a topic's end, while it's
	// still closing.
	void StartPlayerFlow(StartFlowPtr a_flow)
	{
		// Every 250 ms while the player is in dialogue (no limit, as before).
		Scheduler::WhileThenOnMain(
			std::chrono::milliseconds(250), std::numeric_limits<int>::max(),
			[a_flow, logged = false]() mutable {
				const bool waiting = Compat::CanCheckDialogue() && RE::IsPlayerInDialogue() && FlowAlive(*a_flow);
				if (waiting && !logged) {
					REX::INFO("ThreadBuilder: thread {} waits for the dialogue to end", a_flow->threadID);
					logged = true;
				}
				return waiting;
			},
			std::chrono::milliseconds(0), [a_flow]() { FlowFurniture(a_flow); });
	}

	// Starts a scene from a builder (OStim's Start). Returns the thread ID
	// (0 for the player's), or -1. As OStim's, the thread exists from here
	// on (FourThread.IsRunning, metadata, GetActors...), while Papyrus places
	// the actors a moment later; changes that need the scene playing (speed,
	// navigation, stop) are done once it is. A start that fails after this
	// (no scene fits, Papyrus gives up) drops the thread and logs why.
	std::int32_t StartThread(ThreadBuilder a_builder)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		bool       withPlayer = false;
		for (std::size_t i = 0; i < a_builder.actors.size(); ++i) {
			const auto id = a_builder.actors[i];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (const auto why = CantUse(actor); !why.empty()) {
				REX::WARN("ThreadBuilder.Start: {:08X} can't be in a scene ({})", id, why);
				return -1;
			}
			if (std::ranges::find(a_builder.actors.begin(), a_builder.actors.begin() + static_cast<std::ptrdiff_t>(i), id) != a_builder.actors.begin() + static_cast<std::ptrdiff_t>(i)) {
				REX::WARN("ThreadBuilder.Start: {:08X} is given twice", id);  // as OStim's ThreadStarter
				return -1;
			}
			withPlayer = withPlayer || (player && id == player->GetFormID());
		}
		std::int32_t        threadID = 0;
		const std::uint64_t flowID = g_nextStartFlow++;
		{
			std::scoped_lock lock(g_threadsLock);
			PruneStartingLocked();
			if (withPlayer && g_threads.contains(0) && g_threads.at(0).status != ThreadRecord::Status::kEnded) {
				REX::WARN("ThreadBuilder.Start: the player's scene is already starting");
				return -1;
			}
			threadID = withPlayer ? 0 : g_nextThreadID++;
			ThreadRecord record;
			record.id = threadID;
			record.actors = ActorSlots<int>::Key(a_builder.actors);
			record.roleActors = a_builder.actors;
			record.duration = a_builder.duration;
			record.undressAtStart = a_builder.undress;
			record.noAutoMode = a_builder.noAutoMode;
			record.noPlayerControl = a_builder.noPlayerControl;
			record.noUndress = a_builder.noUndress;
			record.noPostDialogue = a_builder.noPostDialogue;
			record.endAfterSequence = a_builder.endAfterSequence;
			record.metadata = a_builder.metadata;
			if (withPlayer) {
				record.flow = flowID;
			}
			g_threads.insert_or_assign(threadID, std::move(record));
		}
		if (withPlayer) {
			auto flow = std::make_shared<StartFlow>();
			flow->builder = std::move(a_builder);
			flow->threadID = threadID;
			flow->id = flowID;
			StartPlayerFlow(std::move(flow));
			return threadID;
		}
		F4SE::GetTaskInterface()->AddTask([builder = std::move(a_builder), threadID]() { LaunchThread(builder, threadID, false, {}); });
		return threadID;
	}

	namespace Natives::FourThreadBuilder
	{
		// FourThreadBuilder (OThreadBuilder)
		std::int32_t Create(std::monostate, std::vector<RE::Actor*> a_actors) { return BuilderCreate(std::move(a_actors)); }
		void SetDominantActors(std::monostate, std::int32_t a_id, std::vector<RE::Actor*> a_actors)
		{
			WithBuilder(a_id, [&](ThreadBuilder& b) {
				b.dominant.clear();
				for (const auto actor : a_actors) {
					if (actor) {
						b.dominant.push_back(actor->GetFormID());
					}
				}
			});
		}
		void SetFurniture(std::monostate, std::int32_t a_id, RE::TESObjectREFR* a_ref)
		{
			WithBuilder(a_id, [&](ThreadBuilder& b) { b.furniture = a_ref ? a_ref->GetFormID() : 0; });
		}
		void SetDuration(std::monostate, std::int32_t a_id, float a_seconds)
		{
			WithBuilder(a_id, [&](ThreadBuilder& b) { b.duration = std::max(a_seconds, 0.0F); });
		}
		// As OStim's ThreadStartParams: an unknown scene is left out, with a warning.
		bool KnownStartingScene(const std::string& a_scene, std::string_view a_what)
		{
			if (SceneRegistry::Find(a_scene)) {
				return true;
			}
			REX::WARN("ThreadBuilder.{}: no scene \"{}\", left out", a_what, a_scene);
			return false;
		}
		void SetStartingAnimation(std::monostate, std::int32_t a_id, std::string a_scene)
		{
			if (!KnownStartingScene(a_scene, "SetStartingAnimation")) {
				return;
			}
			WithBuilder(a_id, [&](ThreadBuilder& b) {
				b.starting = { SceneRegistry::SequenceEntry{ a_scene, 0.0F, 0 } };
				b.navigateTo = { false };
			});
		}
		void AddStartingAnimation(std::monostate, std::int32_t a_id, std::string a_scene, float a_duration, bool a_navigateTo)
		{
			if (!KnownStartingScene(a_scene, "AddStartingAnimation")) {
				return;
			}
			WithBuilder(a_id, [&](ThreadBuilder& b) {
				b.navigateTo.resize(b.starting.size(), false);
				b.starting.push_back(SceneRegistry::SequenceEntry{ a_scene, a_duration, 0 });
				b.navigateTo.push_back(a_navigateTo);
			});
		}
		void SetStartingSequence(std::monostate, std::int32_t a_id, std::string a_sequence)
		{
			const auto sequence = SceneRegistry::FindSequence(a_sequence);
			if (!sequence) {
				REX::WARN("ThreadBuilder.SetStartingSequence: no sequence \"{}\"", a_sequence);
				return;
			}
			WithBuilder(a_id, [&](ThreadBuilder& b) {
				b.starting = sequence->entries;
				b.navigateTo.assign(b.starting.size(), false);
			});
		}
		void ConcatStartingSequence(std::monostate, std::int32_t a_id, std::string a_sequence, bool a_navigateTo)
		{
			const auto sequence = SceneRegistry::FindSequence(a_sequence);
			if (!sequence) {
				REX::WARN("ThreadBuilder.ConcatStartingSequence: no sequence \"{}\"", a_sequence);
				return;
			}
			WithBuilder(a_id, [&](ThreadBuilder& b) {
				b.navigateTo.resize(b.starting.size(), false);
				b.starting.insert(b.starting.end(), sequence->entries.begin(), sequence->entries.end());
				b.navigateTo.push_back(a_navigateTo);  // the way to its first scene
				b.navigateTo.resize(b.starting.size(), false);
			});
		}
		void EndAfterSequence(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.endAfterSequence = true; }); }
		void UndressActors(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.undress = true; }); }
		void NoAutoMode(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noAutoMode = true; }); }
		void NoPlayerControl(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noPlayerControl = true; }); }
		void NoUndressing(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noUndress = true; }); }
		void NoPostDialogue(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noPostDialogue = true; }); }
		void NoFurniture(std::monostate, std::int32_t a_id) { WithBuilder(a_id, [](ThreadBuilder& b) { b.noFurniture = true; }); }
		void SetMetadata(std::monostate, std::int32_t a_id, std::vector<std::string> a_metadata)
		{
			WithBuilder(a_id, [&](ThreadBuilder& b) {
				b.metadata.clear();
				for (auto& m : a_metadata) {
					b.metadata.push_back(LowerCopy(m));
				}
			});
		}
		void SetMetadataCSV(std::monostate, std::int32_t a_id, std::string a_csv)
		{
			SetMetadata({}, a_id, SplitCSV(a_csv));
		}
		std::int32_t Start(std::monostate, std::int32_t a_id)
		{
			ThreadBuilder builder;
			{
				std::scoped_lock lock(g_buildersLock);
				const auto it = g_builders.find(a_id);
				if (it == g_builders.end()) {
					REX::WARN("ThreadBuilder.Start: no builder {}", a_id);
					return -1;
				}
				builder = std::move(it->second);
				g_builders.erase(it);
			}
			return StartThread(std::move(builder));
		}
		void Cancel(std::monostate, std::int32_t a_id)
		{
			std::scoped_lock lock(g_buildersLock);
			g_builders.erase(a_id);
		}
	}  // namespace Natives::FourThreadBuilder

	void StartSoloScene()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto id = player ? BuilderCreate({ player }) : -1;
		if (id < 0 || Natives::FourThreadBuilder::Start({}, id) < 0) {
			Notify(TR("$4stim_solo_failed", "4Stim: couldn't start a scene with you alone (see 4Stim.log)"));
		}
	}

	namespace Natives::FourThread
	{
		// FourThread (OThread)
		std::int32_t QuickStart(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_scene, RE::TESObjectREFR* a_furniture)
		{
			const auto id = BuilderCreate(std::move(a_actors));
			if (id < 0) {
				return -1;
			}
			if (!a_scene.empty()) {
				Natives::FourThreadBuilder::SetStartingAnimation({}, id, a_scene);
			}
			if (a_furniture) {
				Natives::FourThreadBuilder::SetFurniture({}, id, a_furniture);
			}
			return Natives::FourThreadBuilder::Start({}, id);
		}
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

		bool IsLive(const ThreadRecord& a_record) { return a_record.status != ThreadRecord::Status::kEnded; }

		// As OStim's: a thread exists as soon as Start returned it (its actors
		// are still being placed), until its scene ends.
		bool IsRunning(std::monostate, std::int32_t a_id)
		{
			bool live = false;
			WithRecord(a_id, [&](ThreadRecord& r) { live = IsLive(r); });
			return live;
		}
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
		void Stop(std::monostate, std::int32_t a_id)
		{
			ThreadDo(
				a_id, [](ActiveScene& a) { EndActiveScene(a, "asked by another mod (FourThread.Stop)"); },
				[](ThreadRecord& r) { r.stopWhenStarted = true; });
		}
		std::int32_t GetThreadCount(std::monostate)
		{
			std::scoped_lock lock(g_threadsLock);
			return static_cast<std::int32_t>(std::ranges::count_if(g_threads, [](const auto& a_entry) { return IsLive(a_entry.second); }));
		}
		std::vector<std::int32_t> GetAllThreadIDs(std::monostate)
		{
			std::vector<std::int32_t> ids;
			std::scoped_lock lock(g_threadsLock);
			for (const auto& [id, record] : g_threads) {
				if (IsLive(record)) {
					ids.push_back(id);
				}
			}
			std::ranges::sort(ids);
			return ids;
		}
		std::string GetScene(std::monostate, std::int32_t a_id)
		{
			const auto a = FindThread(a_id);
			return a ? a->sceneID : std::string{};
		}
		// NavigateTo (OStim: through transitions where it can): a transition
		// running finishes first; WarpTo goes there at once.
		void ThreadGoTo(std::int32_t a_id, std::string a_scene, bool a_warp)
		{
			ThreadDo(
				a_id,
				[a_scene, a_warp](ActiveScene& a) {
					const auto to = SceneRegistry::Find(a_scene);
					if (!to || to->actors.size() != a.actors.size()) {
						REX::WARN("FourThread: thread {} can't go to \"{}\"", a.threadID, a_scene);
						return;
					}
					const auto from = SceneRegistry::Find(a.sceneID);
					if (!a_warp && from && from->IsTransition()) {
						a.queuedScene = to->id;
						return;
					}
					if (InterruptSequence(a, "another mod moved the scene")) {
						return;
					}
					// As OStim's navigateTo: along the navigation graph when the
					// scene is within navigationDistanceMax (auto mode's way), else
					// straight there.
					if (!a_warp && from) {
						const auto reachable = Reachable(a, from, g_autoConfig.maxSteps);
						const auto it = std::ranges::find_if(reachable, [&](const auto& a_entry) { return a_entry.first->id == to->id; });
						if (it != reachable.end() && it->second.size() > 1) {
							AutoGoTo(a, to, it->second);
							return;
						}
					}
					if (PlayOnActiveScene(a, to->id, -1)) {
						ArmAutoplay(a);
					}
				},
				[a_scene](ThreadRecord& r) { r.sceneWhenStarted = a_scene; });
		}
		void NavigateTo(std::monostate, std::int32_t a_id, std::string a_scene) { ThreadGoTo(a_id, std::move(a_scene), false); }
		void WarpTo(std::monostate, std::int32_t a_id, std::string a_scene, bool) { ThreadGoTo(a_id, std::move(a_scene), true); }
		std::int32_t GetSpeed(std::monostate, std::int32_t a_id)
		{
			const auto a = FindThread(a_id);
			return a ? a->speed : -1;
		}
		void SetSpeed(std::monostate, std::int32_t a_id, std::int32_t a_speed)
		{
			ThreadDo(
				a_id,
				[a_speed](ActiveScene& a) {
					const auto scene = SceneRegistry::Find(a.sceneID);
					if (!scene || scene->speeds.empty()) {
						return;
					}
					const int speed = std::clamp(a_speed, 0, static_cast<int>(scene->speeds.size()) - 1);
					if (speed != a.speed) {
						PlayOnActiveScene(a, a.sceneID, speed);
					}
				},
				[a_speed](ThreadRecord& r) { r.speedWhenStarted = a_speed; });
		}
		// The actors in role order; while starting, in the order they were given.
		std::vector<std::uint32_t> ThreadActors(std::int32_t a_id)
		{
			if (const auto a = FindThread(a_id)) {
				return a->actors;
			}
			std::vector<std::uint32_t> out;
			WithRecord(a_id, [&](ThreadRecord& r) {
				if (IsLive(r)) {
					out = r.roleActors;
				}
			});
			return out;
		}
		std::vector<RE::Actor*> GetActors(std::monostate, std::int32_t a_id)
		{
			std::vector<RE::Actor*> out;
			for (const auto id : ThreadActors(a_id)) {
				out.push_back(RE::TESForm::GetFormByID<RE::Actor>(id));
			}
			return out;
		}
		RE::Actor* GetActor(std::monostate, std::int32_t a_id, std::int32_t a_index)
		{
			const auto actors = ThreadActors(a_id);
			return a_index >= 0 && static_cast<std::size_t>(a_index) < actors.size() ? RE::TESForm::GetFormByID<RE::Actor>(actors[a_index]) : nullptr;
		}
		std::int32_t GetActorPosition(std::monostate, std::int32_t a_id, RE::Actor* a_actor)
		{
			if (!a_actor) {
				return -1;
			}
			const auto actors = ThreadActors(a_id);
			const auto it = std::ranges::find(actors, a_actor->GetFormID());
			return it != actors.end() ? static_cast<std::int32_t>(it - actors.begin()) : -1;
		}
		// OThread's climax stall, as OStim's Thread::stallClimax: the thread's
		// own flag, apart from each actor's (OActor.StallClimax), so permitting
		// one doesn't lift the other. PermitClimax(id, true) lifts the actors'
		// too, as OStim's permitActors.
		void StallClimax(std::monostate, std::int32_t a_id)
		{
			ThreadDo(
				a_id,
				[](ActiveScene& a) {
					a.stallClimax = true;
					for (const auto id : a.actors) {
						Excitement::SetThreadStalled(id, true);
					}
				},
				[](ThreadRecord& r) { r.stallWhenStarted = true; });
		}
		void PermitClimax(std::monostate, std::int32_t a_id, bool a_permitActors)
		{
			ThreadDo(
				a_id,
				[a_permitActors](ActiveScene& a) {
					a.stallClimax = false;
					for (const auto id : a.actors) {
						Excitement::SetThreadStalled(id, false);
						if (a_permitActors) {
							Excitement::SetStalled(id, false);
						}
					}
				},
				[a_permitActors](ThreadRecord& r) { r.stallWhenStarted = false; (void)a_permitActors; });
		}
		bool IsClimaxStalled(std::monostate, std::int32_t a_id)
		{
			bool stalled = false;
			WithRecord(a_id, [&](ThreadRecord& r) { stalled = r.status == ThreadRecord::Status::kStarting && r.stallWhenStarted.value_or(false); });
			if (stalled) {
				return true;
			}
			const auto a = FindThread(a_id);
			return a && a->stallClimax;  // OStim's: the thread's own flag only
		}
		RE::TESObjectREFR* GetFurniture(std::monostate, std::int32_t a_id)
		{
			const auto a = FindThread(a_id);
			return a && a->furnitureRef ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a->furnitureRef) : nullptr;
		}
		std::string GetFurnitureType(std::monostate, std::int32_t a_id)
		{
			const auto a = FindThread(a_id);
			return a ? a->furnitureType : std::string{};
		}
		bool IsInAutoMode(std::monostate, std::int32_t a_id)
		{
			bool queued = false;
			WithRecord(a_id, [&](ThreadRecord& r) { queued = r.status == ThreadRecord::Status::kStarting && r.autoWhenStarted.value_or(false); });
			const auto a = FindThread(a_id);
			return queued || (a && a->autoMode.on);
		}
		void StartAutoMode(std::monostate, std::int32_t a_id)
		{
			ThreadDo(
				a_id,
				[](ActiveScene& a) {
					if (!a.autoMode.on) {
						::StartAutoMode(a);
					}
				},
				[](ThreadRecord& r) { r.autoWhenStarted = true; });
		}
		void StopAutoMode(std::monostate, std::int32_t a_id)
		{
			ThreadDo(
				a_id,
				[](ActiveScene& a) {
					if (a.autoMode.on) {
						::StopAutoMode(a);
					}
				},
				[](ThreadRecord& r) { r.autoWhenStarted = false; });
		}
		// Metadata: read and written at once (OStim's are), from the record, so
		// a starting thread has it too, and an ended one until its end event.
		bool HasMetadata(std::monostate, std::int32_t a_id, std::string a_meta)
		{
			bool has = false;
			WithRecord(a_id, [&, meta = LowerCopy(a_meta)](ThreadRecord& r) { has = std::ranges::find(r.metadata, meta) != r.metadata.end(); });
			return has;
		}
		void AddMetadata(std::monostate, std::int32_t a_id, std::string a_meta)
		{
			WithRecord(a_id, [meta = LowerCopy(a_meta)](ThreadRecord& r) {
				if (std::ranges::find(r.metadata, meta) == r.metadata.end()) {
					r.metadata.push_back(meta);
				}
			});
		}
		std::vector<std::string> GetMetadata(std::monostate, std::int32_t a_id)
		{
			std::vector<std::string> out;
			WithRecord(a_id, [&](ThreadRecord& r) { out = r.metadata; });
			return out;
		}
		bool HasMetaFloat(std::monostate, std::int32_t a_id, std::string a_key)
		{
			bool has = false;
			WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) { has = r.metaFloat.contains(key); });
			return has;
		}
		float GetMetaFloat(std::monostate, std::int32_t a_id, std::string a_key)
		{
			float value = 0.0F;
			WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) {
				if (const auto it = r.metaFloat.find(key); it != r.metaFloat.end()) {
					value = it->second;
				}
			});
			return value;
		}
		void SetMetaFloat(std::monostate, std::int32_t a_id, std::string a_key, float a_value)
		{
			WithRecord(a_id, [key = LowerCopy(a_key), a_value](ThreadRecord& r) { r.metaFloat[key] = a_value; });
		}
		bool HasMetaString(std::monostate, std::int32_t a_id, std::string a_key)
		{
			bool has = false;
			WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) { has = r.metaString.contains(key); });
			return has;
		}
		std::string GetMetaString(std::monostate, std::int32_t a_id, std::string a_key)
		{
			std::string value;
			WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) {
				if (const auto it = r.metaString.find(key); it != r.metaString.end()) {
					value = it->second;
				}
			});
			return value;
		}
		void SetMetaString(std::monostate, std::int32_t a_id, std::string a_key, std::string a_value)
		{
			WithRecord(a_id, [key = LowerCopy(a_key), value = std::move(a_value)](ThreadRecord& r) { r.metaString[key] = value; });
		}
		bool PlaySequence(std::monostate, std::int32_t a_id, std::string a_sequence, bool, bool)
		{
			const auto sequence = SceneRegistry::FindSequence(a_sequence);
			const auto a = FindThread(a_id);
			if (!sequence || !a) {
				return false;
			}
			F4SE::GetTaskInterface()->AddTask([actor = a->actors.front(), sequence]() { StartSequenceOnScene(actor, sequence); });
			return true;
		}
	}  // namespace Natives::FourThread
	namespace Natives::FourActor
	{
		// The actor's thread, a starting one too (OStim's threads exist from Start).
		std::int32_t GetThreadID(std::monostate, RE::Actor* a_actor)
		{
			if (!a_actor) {
				return -1;
			}
			if (const auto id = ThreadIDOf(a_actor->GetFormID()); id >= 0) {
				return id;
			}
			std::scoped_lock lock(g_threadsLock);
			for (const auto& [id, record] : g_threads) {
				if (record.status == ThreadRecord::Status::kStarting && std::ranges::find(record.actors, a_actor->GetFormID()) != record.actors.end()) {
					return id;
				}
			}
			return -1;
		}
	}  // namespace Natives::FourActor

	// ---- OStim's OLibrary, OMetadata, OActor, OFurniture (docs/API.md) ----

	std::vector<std::uint32_t> IDsOf(const std::vector<RE::Actor*>& a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			if (actor) {
				ids.push_back(actor->GetFormID());
			}
		}
		return ids;
	}

	bool HasAnyOf(const std::vector<std::string>& a_have, const std::vector<std::string>& a_wanted)
	{
		return std::ranges::any_of(a_wanted, [&](const std::string& w) {
			return std::ranges::any_of(a_have, [&](const std::string& h) { return _stricmp(h.c_str(), w.c_str()) == 0; });
		});
	}

	bool HasAllOf(const std::vector<std::string>& a_have, const std::vector<std::string>& a_wanted)
	{
		return std::ranges::all_of(a_wanted, [&](const std::string& w) {
			return std::ranges::any_of(a_have, [&](const std::string& h) { return _stricmp(h.c_str(), w.c_str()) == 0; });
		});
	}

	// OStim's OLibrary.GetRandom...: a random scene these actors can play
	// in the order given (OStim's), on a_furniture ("" = off furniture), with
	// the scene tags, actions and (for role a_position, or -1: any role)
	// actor tags asked for; empty lists ask nothing. Not transitions or
	// scenes marked noRandomSelection, as OStim. "" if none.
	std::string RandomScene(const std::vector<RE::Actor*>& a_actors, const std::string& a_furniture, const std::vector<std::string>& a_anyTags,
		const std::vector<std::string>& a_allTags, const std::vector<std::string>& a_anyActions, const std::vector<std::string>& a_allActions,
		std::int32_t a_position, const std::vector<std::string>& a_anyActorTags, const std::vector<std::string>& a_allActorTags)
	{
		const auto cast = IDsOf(a_actors);
		if (cast.empty()) {
			return {};
		}
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
		for (const auto& scene : ScenesForActors(cast, LowerCopy(a_furniture))) {
			// As OStim's Node::fulfilledBy: the actors in the order given.
			if (scene->IsTransition() || scene->noRandomSelection || !SexesFit(*scene, cast)) {
				continue;
			}
			if ((!a_anyTags.empty() && !HasAnyOf(scene->tags, a_anyTags)) || !HasAllOf(scene->tags, a_allTags)) {
				continue;
			}
			if (!a_anyActions.empty() && std::ranges::none_of(a_anyActions, [&](const std::string& t) { return scene->HasAction(t); })) {
				continue;
			}
			if (!std::ranges::all_of(a_allActions, [&](const std::string& t) { return scene->HasAction(t); })) {
				continue;
			}
			if (!a_anyActorTags.empty() || !a_allActorTags.empty()) {
				bool fits = false;
				for (std::size_t role = 0; role < scene->actors.size() && !fits; ++role) {
					if (a_position >= 0 && static_cast<std::size_t>(a_position) != role) {
						continue;
					}
					const auto& tags = scene->actors[role].tags;
					fits = (a_anyActorTags.empty() || HasAnyOf(tags, a_anyActorTags)) && HasAllOf(tags, a_allActorTags);
				}
				if (!fits) {
					continue;
				}
			}
			pool.push_back(scene);
		}
		if (pool.empty()) {
			return {};
		}
		static std::mt19937 rng{ std::random_device{}() };
		return pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)]->id;
	}

	// ---- FourLibrary queries beyond FindRandomScene (OStim's PapyrusLibrary) ----
	// OStim builds a condition per OLibrary function and picks a random node
	// that fits the actors and meets it (GraphTable::getRandomNode: no
	// transitions, nothing marked noRandomSelection). These are the same
	// conditions over 4Stim's scenes, behind four natives the OLibrary-named
	// Papyrus functions wrap.

	// OStim's StringUtil::toTagVector (lowercase, split on ',') and
	// toTagMatrix (lists split on ';', one per actor; empty lists keep their
	// place). Items are trimmed of spaces, which OStim doesn't do.
	std::vector<std::string> TagList(const std::string& a_csv) { return SplitCSV(LowerCopy(a_csv)); }

	std::vector<std::vector<std::string>> TagMatrix(const std::string& a_text)
	{
		std::vector<std::vector<std::string>> out;
		std::string                           part;
		for (const char ch : a_text + ";") {
			if (ch == ';') {
				out.push_back(TagList(part));
				part.clear();
			} else {
				part += ch;
			}
		}
		return out;
	}

	// OStim's VectorUtil::stoim: lists of ints, ';' between lists, ',' within.
	std::vector<std::vector<int>> IntMatrix(const std::string& a_text)
	{
		std::vector<std::vector<int>> out;
		if (a_text.empty()) {
			return out;
		}
		for (const auto& list : TagMatrix(a_text)) {
			std::vector<int> ints;
			for (const auto& item : list) {
				try {
					ints.push_back(std::stoi(item));
				} catch (...) {
				}
			}
			out.push_back(std::move(ints));
		}
		return out;
	}

	bool RoleHasAnyTag(const SceneRegistry::Scene& a_scene, int a_role, const std::vector<std::string>& a_tags)
	{
		return a_role >= 0 && static_cast<std::size_t>(a_role) < a_scene.actors.size() && HasAnyOf(a_scene.actors[a_role].tags, a_tags);
	}

	bool RoleHasAllTags(const SceneRegistry::Scene& a_scene, int a_role, const std::vector<std::string>& a_tags)
	{
		return a_role >= 0 && static_cast<std::size_t>(a_role) < a_scene.actors.size() && HasAllOf(a_scene.actors[a_role].tags, a_tags);
	}

	// OStim's Node::hasOnlyListedActorTags: true past the last role.
	bool RoleHasOnlyTags(const SceneRegistry::Scene& a_scene, int a_role, const std::vector<std::string>& a_tags)
	{
		if (a_role < 0 || static_cast<std::size_t>(a_role) >= a_scene.actors.size()) {
			return true;
		}
		return HasAllOf(a_tags, a_scene.actors[a_role].tags);
	}

	bool ForAnyRole(std::size_t a_count, const std::function<bool(int)>& a_test)
	{
		for (std::size_t i = 0; i < a_count; ++i) {
			if (a_test(static_cast<int>(i))) {
				return true;
			}
		}
		return false;
	}

	bool ForAllRoles(std::size_t a_count, const std::function<bool(int)>& a_test)
	{
		for (std::size_t i = 0; i < a_count; ++i) {
			if (!a_test(static_cast<int>(i))) {
				return false;
			}
		}
		return true;
	}

	int RoleIndex(std::size_t a_role) { return a_role == SceneRegistry::SceneAction::kNoRole ? -1 : static_cast<int>(a_role); }

	// OStim's Action::isType (by the alias-resolved id).
	bool ActionIsType(const SceneRegistry::SceneAction& a_action, const std::string& a_type)
	{
		return a_action.type->id == Actions::ResolveAlias(a_type);
	}

	// A scene fitting the actors (and furniture) that meets a_condition, at
	// random, as OStim's randomScene.
	std::string RandomSceneWhere(const std::vector<RE::Actor*>& a_actors, const std::string& a_furniture,
		const std::function<bool(const SceneRegistry::Scene&)>& a_condition)
	{
		const auto cast = IDsOf(a_actors);
		if (cast.empty()) {
			return {};
		}
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
		for (const auto& scene : ScenesForActors(cast, LowerCopy(a_furniture))) {
			if (!scene->IsTransition() && !scene->noRandomSelection && SexesFit(*scene, cast) && a_condition(*scene)) {  // in the order given, as OStim's
				pool.push_back(scene);
			}
		}
		if (pool.empty()) {
			return {};
		}
		static std::mt19937 rng{ std::random_device{}() };
		return pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)]->id;
	}

	namespace Natives::FourLibrary
	{
		// OLibrary's (Any|All)MultiActorTag(s)For(Any|All) and the scene tag
		// combinations of them. a_actorTags: one list per role ("a,b;c"), each
		// role needing any (a_allPerRole false) or all of its list; for any role
		// or all of them (a_forAll). a_sceneMode: 0 no scene tags, 1 any of
		// a_sceneTags, 2 all of them.
		std::string FindRandomSceneByRoleTags(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::string a_actorTags,
			bool a_allPerRole, bool a_forAll, std::string a_sceneTags, std::int32_t a_sceneMode)
		{
			const auto matrix = TagMatrix(a_actorTags);
			const auto sceneTags = TagList(a_sceneTags);
			return RandomSceneWhere(a_actors, a_furniture, [&](const SceneRegistry::Scene& a_scene) {
				if (a_sceneMode == 1 && !HasAnyOf(a_scene.tags, sceneTags)) {
					return false;
				}
				if (a_sceneMode == 2 && !HasAllOf(a_scene.tags, sceneTags)) {
					return false;
				}
				const auto test = [&](int i) {
					return a_allPerRole ? RoleHasAllTags(a_scene, i, matrix[i]) : RoleHasAnyTag(a_scene, i, matrix[i]);
				};
				return a_forAll ? ForAllRoles(matrix.size(), test) : ForAnyRole(matrix.size(), test);
			});
		}

		// OLibrary's ...Action(s)ForActor / ForTarget / ForActorAndTarget: an
		// action of a type (any of a_types, or each of them with a_all) done by
		// role a_actor (when a_byActor) to role a_target (when a_byTarget), as
		// OStim's Node::findActionForActor and its siblings.
		std::string FindRandomSceneByActionRoles(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_types,
			bool a_all, bool a_byActor, std::int32_t a_actor, bool a_byTarget, std::int32_t a_target)
		{
			for (auto& type : a_types) {
				type = LowerCopy(type);
			}
			return RandomSceneWhere(a_actors, a_furniture, [&](const SceneRegistry::Scene& a_scene) {
				const auto has = [&](const std::string& a_type) {
					return std::ranges::any_of(a_scene.actions, [&](const SceneRegistry::SceneAction& a) {
						return (!a_byActor || RoleIndex(a.actor) == a_actor) && (!a_byTarget || RoleIndex(a.target) == a_target) && ActionIsType(a, a_type);
					});
				};
				return a_all ? std::ranges::all_of(a_types, has) : std::ranges::any_of(a_types, has);
			});
		}

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

		// args[a_first..a_first+7]: types, actors, targets, performers, mates any,
		// mates all, participants any, participants all.
		ActionRoleLists ActionListsFrom(const std::vector<std::string>& a_args, std::size_t a_first)
		{
			ActionRoleLists lists;
			lists.types = TagList(a_args[a_first]);
			lists.actors = IntMatrix(a_args[a_first + 1]);
			lists.targets = IntMatrix(a_args[a_first + 2]);
			lists.performers = IntMatrix(a_args[a_first + 3]);
			lists.matesAny = IntMatrix(a_args[a_first + 4]);
			lists.matesAll = IntMatrix(a_args[a_first + 5]);
			lists.participantsAny = IntMatrix(a_args[a_first + 6]);
			lists.participantsAll = IntMatrix(a_args[a_first + 7]);
			return lists;
		}

		// OLibrary.GetRandomSceneSuperloadCSV: a_args holds its 44 strings after
		// FurnitureType, in OStim's order (FourLibrary.psc packs them).
		std::string FindRandomSceneSuperload(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_args)
		{
			if (a_args.size() < 44) {
				REX::WARN("FourLibrary.GetRandomSceneSuperloadCSV: {} of its 44 filter strings", a_args.size());
				a_args.resize(44);
			}
			std::vector<std::function<bool(const SceneRegistry::Scene&)>> conditions;
			const auto& a = a_args;
			if (!a[0].empty()) {
				conditions.push_back([tags = TagList(a[0])](const SceneRegistry::Scene& s) { return HasAnyOf(s.tags, tags); });
			}
			if (!a[1].empty()) {
				conditions.push_back([tags = TagList(a[1])](const SceneRegistry::Scene& s) { return HasAllOf(s.tags, tags); });
			}
			if (!a[2].empty()) {  // OStim's Node::hasOnlyTags
				conditions.push_back([tags = TagList(a[2])](const SceneRegistry::Scene& s) { return HasAllOf(tags, s.tags); });
			}
			if (!a[3].empty()) {
				conditions.push_back([tags = TagList(a[3])](const SceneRegistry::Scene& s) { return !HasAnyOf(s.tags, tags); });
			}
			// Actor tags: any for any, any for all, all for any, all for all,
			// whitelist for any / all, blacklist for any / all.
			const auto roleTags = [&](std::size_t a_index, auto a_test, bool a_forAll) {
				if (a[a_index].empty()) {
					return;
				}
				conditions.push_back([m = TagMatrix(a[a_index]), a_test, a_forAll](const SceneRegistry::Scene& s) {
					const auto each = [&](int i) { return a_test(s, i, m[i]); };
					return a_forAll ? ForAllRoles(m.size(), each) : ForAnyRole(m.size(), each);
				});
			};
			const auto anyTag = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return RoleHasAnyTag(s, i, t); };
			const auto allTags = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return RoleHasAllTags(s, i, t); };
			const auto onlyTags = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return RoleHasOnlyTags(s, i, t); };
			const auto noTag = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return !RoleHasAnyTag(s, i, t); };
			roleTags(4, anyTag, false);
			roleTags(5, anyTag, true);
			roleTags(6, allTags, false);
			roleTags(7, allTags, true);
			roleTags(8, onlyTags, false);
			roleTags(9, onlyTags, true);
			roleTags(10, noTag, false);
			roleTags(11, noTag, true);
			if (!a[12].empty()) {
				conditions.push_back([l = ActionListsFrom(a, 12)](const SceneRegistry::Scene& s) { return l.SceneHasAny(s); });
			}
			if (!a[20].empty()) {
				conditions.push_back([l = ActionListsFrom(a, 20)](const SceneRegistry::Scene& s) { return l.SceneHasAll(s); });
			}
			if (!a[28].empty()) {
				conditions.push_back([l = ActionListsFrom(a, 28)](const SceneRegistry::Scene& s) { return l.SceneHasOnlyListed(s); });
			}
			if (!a[36].empty()) {
				conditions.push_back([l = ActionListsFrom(a, 36)](const SceneRegistry::Scene& s) { return !l.SceneHasAny(s); });
			}
			return RandomSceneWhere(a_actors, a_furniture, [&](const SceneRegistry::Scene& a_scene) {
				return std::ranges::all_of(conditions, [&](const auto& c) { return c(a_scene); });
			});
		}

		// OLibrary.GetScenesInRange (OStim's Node::getNodesInRange): the scenes
		// reachable from a_id in up to a_distance navigations whose scenes (any
		// transitions on the way included) fit a_actors, sorted by name. As in
		// OStim, a_id itself is only in it if a navigation leads back to it.
		std::vector<std::string> GetScenesInRange(std::monostate, std::string a_id, std::vector<RE::Actor*> a_actors, std::int32_t a_distance)
		{
			const auto start = SceneRegistry::Find(a_id);
			const auto cast = IDsOf(a_actors);
			if (!start || cast.empty()) {
				return {};
			}
			SceneRegistry::ListFilter filter;
			filter.actors = ConditionsOf(cast);
			std::unordered_set<std::string> fits;
			for (const auto& summary : SceneRegistry::List(cast.size(), filter)) {
				fits.insert(LowerCopy(summary.id));
			}
			// Where a navigation to a_to ends (past its transitions), or null
			// when something on the way doesn't fit the actors.
			const auto destination = [&](const std::string& a_to) -> std::shared_ptr<const SceneRegistry::Scene> {
				auto scene = SceneRegistry::Find(a_to);
				for (int hops = 0; scene && hops < 16; ++hops) {
					if (!fits.contains(LowerCopy(scene->id))) {
						return nullptr;
					}
					if (!scene->IsTransition()) {
						return scene;
					}
					scene = SceneRegistry::Find(scene->destination);
				}
				return nullptr;
			};
			std::vector<std::shared_ptr<const SceneRegistry::Scene>> found;
			std::vector<std::shared_ptr<const SceneRegistry::Scene>> level{ start };
			for (int i = 0; i < a_distance && !level.empty(); ++i) {
				std::vector<std::shared_ptr<const SceneRegistry::Scene>> next;
				for (const auto& scene : level) {
					for (const auto& nav : scene->navigations) {
						const auto to = destination(nav.to);
						if (to && std::ranges::find(found, to) == found.end()) {
							found.push_back(to);
							next.push_back(to);
						}
					}
				}
				level = std::move(next);
			}
			std::ranges::sort(found, [](const auto& x, const auto& y) { return LowerCopy(x->name) < LowerCopy(y->name); });
			std::vector<std::string> ids;
			for (const auto& scene : found) {
				ids.push_back(scene->id);
			}
			return ids;
		}

		// FourLibrary: one native the OLibrary-named functions wrap.
		std::string FindRandomScene(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_anyTags,
			std::vector<std::string> a_allTags, std::vector<std::string> a_anyActions, std::vector<std::string> a_allActions, std::int32_t a_position,
			std::vector<std::string> a_anyActorTags, std::vector<std::string> a_allActorTags)
		{
			return RandomScene(a_actors, a_furniture, a_anyTags, a_allTags, a_anyActions, a_allActions, a_position, a_anyActorTags, a_allActorTags);
		}

		std::vector<std::string> SplitCSV(std::monostate, std::string a_csv) { return ::SplitCSV(a_csv); }

		std::vector<std::string> GetAllScenes(std::monostate)
		{
			std::vector<std::string> ids;
			for (std::size_t count = 1; count <= 16; ++count) {
				for (const auto& summary : SceneRegistry::List(count)) {
					ids.push_back(summary.id);
				}
			}
			return ids;
		}
	}  // namespace Natives::FourLibrary

	// ---- FourMetadata: the rest of OMetadata (OStim's PapyrusMetadata) ----
	// Natives for what Papyrus can't do cheaply; FourMetadata.psc builds
	// OMetadata's 282 functions on them under OStim's names.

	namespace Natives::FourMetadata
	{
		std::vector<std::string> ScenesToNames(std::monostate, std::vector<std::string> a_ids)
		{
			std::vector<std::string> out;
			for (const auto& id : a_ids) {
				const auto scene = SceneRegistry::Find(id);
				out.push_back(scene ? scene->name : std::string{});
			}
			return out;
		}

		// OStim's GetAnimationId: the "animation" of speed a_index ("" for 4Stim's
		// "idles" speeds, which have no such name).
		std::string GetAnimationId(std::monostate, std::string a_id, std::int32_t a_index)
		{
			const auto scene = SceneRegistry::Find(a_id);
			if (!scene || a_index < 0 || static_cast<std::size_t>(a_index) >= scene->speedInfo.size()) {
				return {};
			}
			return scene->speedInfo[a_index].animation;
		}

		// 4Stim's: what the role at a_position needs (its own and its actions'
		// requirements), as OStim's actor condition reads them.
		std::vector<std::string> GetActorRequirements(std::monostate, std::string a_id, std::int32_t a_position)
		{
			const auto scene = SceneRegistry::Find(a_id);
			if (!scene || a_position < 0 || static_cast<std::size_t>(a_position) >= scene->actors.size()) {
				return {};
			}
			return scene->actors[a_position].requirements;
		}

		const SceneRegistry::SceneAction* ActionAt(const SceneRegistry::Scene* a_scene, std::int32_t a_index)
		{
			return a_scene && a_index >= 0 && static_cast<std::size_t>(a_index) < a_scene->actions.size() ? &a_scene->actions[a_index] : nullptr;
		}

		std::vector<std::string> GetActionTypes(std::monostate, std::string a_id)
		{
			std::vector<std::string> out;
			if (const auto scene = SceneRegistry::Find(a_id)) {
				for (const auto& action : scene->actions) {
					out.push_back(action.type->id);
				}
			}
			return out;
		}

		// a_which: 0 actor, 1 target, 2 performer.
		std::vector<std::int32_t> ActionRolesOf(const std::string& a_id, int a_which)
		{
			std::vector<std::int32_t> out;
			if (const auto scene = SceneRegistry::Find(a_id)) {
				for (const auto& action : scene->actions) {
					out.push_back(RoleIndex(a_which == 0 ? action.actor : a_which == 1 ? action.target : action.performer));
				}
			}
			return out;
		}
		std::vector<std::int32_t> GetActionActors(std::monostate, std::string a_id) { return ActionRolesOf(a_id, 0); }
		std::vector<std::int32_t> GetActionTargets(std::monostate, std::string a_id) { return ActionRolesOf(a_id, 1); }
		std::vector<std::int32_t> GetActionPerformers(std::monostate, std::string a_id) { return ActionRolesOf(a_id, 2); }

		std::vector<std::string> GetActionTags(std::monostate, std::string a_id, std::int32_t a_index)
		{
			const auto action = ActionAt(SceneRegistry::Find(a_id).get(), a_index);
			return action ? action->type->tags : std::vector<std::string>{};
		}

		// OStim's GetAllActionsTags: every action's tags, once each, sorted.
		std::vector<std::string> GetAllActionsTags(std::monostate, std::string a_id)
		{
			std::set<std::string> tags;
			if (const auto scene = SceneRegistry::Find(a_id)) {
				for (const auto& action : scene->actions) {
					tags.insert(action.type->tags.begin(), action.type->tags.end());
				}
			}
			return { tags.begin(), tags.end() };
		}

		// List helpers for FourMetadata.psc (case-insensitive, as Papyrus strings).
		bool ListHasAny(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted) { return HasAnyOf(a_have, a_wanted); }
		bool ListHasAll(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted) { return HasAllOf(a_have, a_wanted); }
		// What of a_have is in a_wanted, lowercase, once each, sorted (OStim's tag
		// overlaps return a std::set).
		std::vector<std::string> ListOverlap(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted)
		{
			std::set<std::string> out;
			for (const auto& h : a_have) {
				if (HasAnyOf(a_wanted, { h })) {
					out.insert(LowerCopy(h));
				}
			}
			return { out.begin(), out.end() };
		}

		// "1,2" -> [1, 2] / [1.0, 2.0] (OStim's VectorUtil::stoiv / stofv: bad items skipped).
		std::vector<std::int32_t> IntsOfCSV(std::monostate, std::string a_csv)
		{
			std::vector<std::int32_t> out;
			for (const auto& item : SplitCSV(a_csv)) {
				try {
					out.push_back(std::stoi(item));
				} catch (...) {
				}
			}
			return out;
		}
		std::vector<float> FloatsOfCSV(std::monostate, std::string a_csv)
		{
			std::vector<float> out;
			for (const auto& item : SplitCSV(a_csv)) {
				try {
					out.push_back(std::stof(item));
				} catch (...) {
				}
			}
			return out;
		}
		// OStim's VectorUtil::getOverlap: the items of a_have also in a_wanted, in a_have's order.
		std::vector<std::int32_t> IntListOverlap(std::monostate, std::vector<std::int32_t> a_have, std::vector<std::int32_t> a_wanted)
		{
			std::erase_if(a_have, [&](std::int32_t v) { return std::ranges::find(a_wanted, v) == a_wanted.end(); });
			return a_have;
		}
		std::vector<float> FloatListOverlap(std::monostate, std::vector<float> a_have, std::vector<float> a_wanted)
		{
			std::erase_if(a_have, [&](float v) { return std::ranges::find(a_wanted, v) == a_wanted.end(); });
			return a_have;
		}
		std::vector<std::string> StringListOverlap(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted)
		{
			std::erase_if(a_have, [&](const std::string& v) { return !HasAnyOf(a_wanted, { v }); });
			return a_have;
		}

		// OMetadata's FindAction(s)For... families: the indexes of the actions of
		// any of a_types (all actions when a_types is empty) whose roles fit
		// a_mode, as OStim's conditions: 0 any roles; 1 actor in a_positions;
		// 2 target in; 3 performer in; 4 actor in a_positions and target in
		// a_positions2; 5 actor or target in (mates any); 6 actor and target in
		// (mates all); 7 actor, target or performer in (participants any); 8 all
		// three in (participants all).
		std::vector<std::int32_t> FindActionsBy(std::monostate, std::string a_id, std::vector<std::string> a_types, std::int32_t a_mode,
			std::vector<std::int32_t> a_positions, std::vector<std::int32_t> a_positions2)
		{
			std::vector<std::int32_t> out;
			const auto scene = SceneRegistry::Find(a_id);
			if (!scene) {
				return out;
			}
			for (auto& type : a_types) {
				type = LowerCopy(type);
			}
			const auto in = [](const std::vector<std::int32_t>& a_list, int a_role) { return std::ranges::find(a_list, a_role) != a_list.end(); };
			for (std::size_t i = 0; i < scene->actions.size(); ++i) {
				const auto& a = scene->actions[i];
				if (!a_types.empty() && std::ranges::none_of(a_types, [&](const std::string& t) { return ActionIsType(a, t); })) {
					continue;
				}
				const int actor = RoleIndex(a.actor);
				const int target = RoleIndex(a.target);
				const int performer = RoleIndex(a.performer);
				bool fits = true;
				switch (a_mode) {
				case 1: fits = in(a_positions, actor); break;
				case 2: fits = in(a_positions, target); break;
				case 3: fits = in(a_positions, performer); break;
				case 4: fits = in(a_positions, actor) && in(a_positions2, target); break;
				case 5: fits = in(a_positions, actor) || in(a_positions, target); break;
				case 6: fits = in(a_positions, actor) && in(a_positions, target); break;
				case 7: fits = in(a_positions, actor) || in(a_positions, target) || in(a_positions, performer); break;
				case 8: fits = in(a_positions, actor) && in(a_positions, target) && in(a_positions, performer); break;
				default: break;
				}
				if (fits) {
					out.push_back(static_cast<std::int32_t>(i));
				}
			}
			return out;
		}

		const Actions::Side& SideOf(const SceneRegistry::SceneAction& a_action, int a_role)
		{
			return a_role == 0 ? a_action.type->actor : a_role == 1 ? a_action.type->target : a_action.type->performer;
		}

		// OMetadata.FindActionsSuperloadCSVv2's search: a_filters holds its 25
		// strings after Id, in OStim's order (FourMetadata.psc packs them).
		std::vector<std::int32_t> FindActionsSuperload(std::monostate, std::string a_id, std::vector<std::string> a_filters)
		{
			std::vector<std::int32_t> out;
			const auto scene = SceneRegistry::Find(a_id);
			if (!scene) {
				return out;
			}
			a_filters.resize(std::max<std::size_t>(a_filters.size(), 25));
			const auto& f = a_filters;
			const auto ints = [](const std::string& a_csv) {
				std::vector<int> list;
				for (const auto& item : SplitCSV(a_csv)) {
					try {
						list.push_back(std::stoi(item));
					} catch (...) {
					}
				}
				return list;
			};
			const auto in = [](const std::vector<int>& a_list, int a_role) { return std::ranges::find(a_list, a_role) != a_list.end(); };
			std::vector<std::function<bool(const SceneRegistry::SceneAction&)>> conditions;
			// Roles: actor, target, performer, mates any / all, participants any / all.
			for (int k = 0; k < 7; ++k) {
				if (f[k].empty()) {
					continue;
				}
				conditions.push_back([k, list = ints(f[k]), in](const SceneRegistry::SceneAction& a) {
					const int actor = RoleIndex(a.actor);
					const int target = RoleIndex(a.target);
					const int performer = RoleIndex(a.performer);
					switch (k) {
					case 0: return in(list, actor);
					case 1: return in(list, target);
					case 2: return in(list, performer);
					case 3: return in(list, actor) || in(list, target);
					case 4: return in(list, actor) && in(list, target);
					case 5: return in(list, actor) || in(list, target) || in(list, performer);
					default: return in(list, actor) && in(list, target) && in(list, performer);
					}
				});
			}
			if (!f[7].empty()) {
				conditions.push_back([types = TagList(f[7])](const SceneRegistry::SceneAction& a) {
					return std::ranges::any_of(types, [&](const std::string& t) { return ActionIsType(a, t); });
				});
			}
			if (!f[8].empty()) {
				conditions.push_back([tags = TagList(f[8])](const SceneRegistry::SceneAction& a) { return HasAnyOf(a.type->tags, tags); });
			}
			if (!f[9].empty()) {
				conditions.push_back([tags = TagList(f[9])](const SceneRegistry::SceneAction& a) { return HasAllOf(a.type->tags, tags); });
			}
			if (!f[10].empty()) {  // OStim's hasOnlyTags
				conditions.push_back([tags = TagList(f[10])](const SceneRegistry::SceneAction& a) { return HasAllOf(tags, a.type->tags); });
			}
			if (!f[11].empty()) {
				conditions.push_back([tags = TagList(f[11])](const SceneRegistry::SceneAction& a) { return !HasAnyOf(a.type->tags, tags); });
			}
			// Custom records: any / all keys per side ("actor keys;target keys;performer keys"),
			// for ints, floats, strings, int lists, float lists, string lists.
			const auto hasKey = [](const Actions::Side& s, int a_kind, const std::string& k) {
				switch (a_kind) {
				case 0: return s.ints.contains(k);
				case 1: return s.floats.contains(k);
				case 2: return s.strings.contains(k);
				case 3: return s.intLists.contains(k);
				case 4: return s.floatLists.contains(k);
				default: return s.stringLists.contains(k);
				}
			};
			for (int kind = 0; kind < 6; ++kind) {
				for (int all = 0; all < 2; ++all) {
					const auto& text = f[12 + kind * 2 + all];
					if (text.empty()) {
						continue;
					}
					const auto matrix = TagMatrix(text);
					for (int role = 0; role < 3 && role < static_cast<int>(matrix.size()); ++role) {
						if (matrix[role].empty()) {
							continue;
						}
						conditions.push_back([keys = matrix[role], role, kind, all, hasKey](const SceneRegistry::SceneAction& a) {
							const auto& side = SideOf(a, role);
							const auto has = [&](const std::string& k) { return hasKey(side, kind, k); };
							return all ? std::ranges::all_of(keys, has) : std::ranges::any_of(keys, has);
						});
					}
				}
			}
			for (std::size_t i = 0; i < scene->actions.size(); ++i) {
				if (std::ranges::all_of(conditions, [&](const auto& c) { return c(scene->actions[i]); })) {
					out.push_back(static_cast<std::int32_t>(i));
				}
			}
			return out;
		}

		// Custom action data (OStim's GetCustomAction{Actor,Target,Performer}...):
		// a_role 0 actor, 1 target, 2 performer; a_kind 0 int, 1 float, 2 string,
		// 3 int list, 4 float list, 5 string list.
		const Actions::Side* CustomSide(const std::string& a_id, std::int32_t a_index, std::int32_t a_role)
		{
			const auto action = ActionAt(SceneRegistry::Find(a_id).get(), a_index);
			return action && a_role >= 0 && a_role <= 2 ? &SideOf(*action, a_role) : nullptr;
		}
		bool HasCustomAction(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::int32_t a_kind, std::string a_record)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto k = LowerCopy(a_record);
			if (!side) {
				return false;
			}
			switch (a_kind) {
			case 0: return side->ints.contains(k);
			case 1: return side->floats.contains(k);
			case 2: return side->strings.contains(k);
			case 3: return side->intLists.contains(k);
			case 4: return side->floatLists.contains(k);
			default: return side->stringLists.contains(k);
			}
		}
		std::int32_t GetCustomActionInt(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, std::int32_t a_fallback)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto it = side ? side->ints.find(LowerCopy(a_record)) : decltype(side->ints)::const_iterator{};
			return side && it != side->ints.end() ? it->second : a_fallback;
		}
		float GetCustomActionFloat(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, float a_fallback)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto it = side ? side->floats.find(LowerCopy(a_record)) : decltype(side->floats)::const_iterator{};
			return side && it != side->floats.end() ? it->second : a_fallback;
		}
		std::string GetCustomActionString(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, std::string a_fallback)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto it = side ? side->strings.find(LowerCopy(a_record)) : decltype(side->strings)::const_iterator{};
			return side && it != side->strings.end() ? it->second : a_fallback;
		}
		std::vector<std::int32_t> GetCustomActionIntList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto it = side ? side->intLists.find(LowerCopy(a_record)) : decltype(side->intLists)::const_iterator{};
			return side && it != side->intLists.end() ? std::vector<std::int32_t>(it->second.begin(), it->second.end()) : std::vector<std::int32_t>{};
		}
		std::vector<float> GetCustomActionFloatList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto it = side ? side->floatLists.find(LowerCopy(a_record)) : decltype(side->floatLists)::const_iterator{};
			return side && it != side->floatLists.end() ? it->second : std::vector<float>{};
		}
		std::vector<std::string> GetCustomActionStringList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record)
		{
			const auto side = CustomSide(a_id, a_index, a_role);
			const auto it = side ? side->stringLists.find(LowerCopy(a_record)) : decltype(side->stringLists)::const_iterator{};
			return side && it != side->stringLists.end() ? it->second : std::vector<std::string>{};
		}

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
		std::int32_t GetCustomSceneActorInt(std::monostate, std::string a_id, std::int32_t a_position, std::string a_record, std::int32_t a_op, std::int32_t a_start)
		{
			return CustomSceneActorAggregate<std::int32_t>(a_id, a_position, a_record, a_op, a_start);
		}
		float GetCustomSceneActorFloat(std::monostate, std::string a_id, std::int32_t a_position, std::string a_record, std::int32_t a_op, float a_start)
		{
			return CustomSceneActorAggregate<float>(a_id, a_position, a_record, a_op, a_start);
		}

		// FourMetadata (OMetadata)
		std::string GetName(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->name : std::string{};
		}
		bool IsTransition(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene && scene->IsTransition();
		}
		std::string GetDestination(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->destination : std::string{};
		}
		std::vector<std::string> GetSceneTags(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->tags : std::vector<std::string>{};
		}
		std::string GetFurnitureType(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->furniture : std::string{};
		}
		std::int32_t GetMaxSpeed(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? static_cast<std::int32_t>(scene->speeds.size()) - 1 : 0;  // OStim's 0 for an unknown scene
		}
		std::int32_t GetDefaultSpeed(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->defaultSpeed : 0;  // OStim's 0 for an unknown scene
		}
		std::vector<std::string> GetNavigations(std::monostate, std::string a_id)
		{
			std::vector<std::string> out;
			if (const auto scene = SceneRegistry::Find(a_id)) {
				for (const auto& nav : scene->navigations) {
					out.push_back(nav.to);
				}
			}
			return out;
		}
		std::string GetAutoTransition(std::monostate, std::string a_id, std::string a_type)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->AutoTransition(LowerCopy(a_type)) : std::string{};
		}
		std::string GetAutoTransitionForActor(std::monostate, std::string a_id, std::int32_t a_position, std::string a_type)
		{
			const auto scene = SceneRegistry::Find(a_id);
			if (!scene || a_position < 0 || static_cast<std::size_t>(a_position) >= scene->actors.size()) {
				return {};
			}
			return scene->actors[a_position].AutoTransition(LowerCopy(a_type));
		}
		bool IsNoRandomSelection(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene && scene->noRandomSelection;
		}
		std::string GetModpack(std::monostate, std::string a_id)
		{
			const auto scene = SceneRegistry::Find(a_id);
			return scene ? scene->modpack : std::string{};
		}
	}  // namespace Natives::FourMetadata

	// FourActor (OActor): per-actor metadata, kept while they're in a scene.
	std::mutex                                                g_actorMetaLock;
	struct ActorMeta
	{
		std::vector<std::string>                     tags;
		std::unordered_map<std::string, float>       floats;
		std::unordered_map<std::string, std::string> strings;
	};
	std::unordered_map<std::uint32_t, ActorMeta> g_actorMeta;

	void ForgetActorMeta(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_actorMetaLock);
		for (const auto id : a_ids) {
			g_actorMeta.erase(id);
		}
	}

	namespace Natives::FourActor
	{
		bool HasMetadata(std::monostate, RE::Actor* a_actor, std::string a_meta)
		{
			std::scoped_lock lock(g_actorMetaLock);
			const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
			return it != g_actorMeta.end() && std::ranges::find(it->second.tags, LowerCopy(a_meta)) != it->second.tags.end();
		}
		void AddMetadata(std::monostate, RE::Actor* a_actor, std::string a_meta)
		{
			if (!a_actor || !FindActiveScene(a_actor->GetFormID())) {
				return;
			}
			std::scoped_lock lock(g_actorMetaLock);
			auto&      tags = g_actorMeta[a_actor->GetFormID()].tags;
			const auto meta = LowerCopy(a_meta);
			if (std::ranges::find(tags, meta) == tags.end()) {
				tags.push_back(meta);
			}
		}
		std::vector<std::string> GetMetadata(std::monostate, RE::Actor* a_actor)
		{
			std::scoped_lock lock(g_actorMetaLock);
			const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
			return it != g_actorMeta.end() ? it->second.tags : std::vector<std::string>{};
		}
		bool HasMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key)
		{
			std::scoped_lock lock(g_actorMetaLock);
			const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
			return it != g_actorMeta.end() && it->second.floats.contains(LowerCopy(a_key));
		}
		float GetMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key)
		{
			std::scoped_lock lock(g_actorMetaLock);
			const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
			if (it == g_actorMeta.end()) {
				return 0.0F;
			}
			const auto f = it->second.floats.find(LowerCopy(a_key));
			return f != it->second.floats.end() ? f->second : 0.0F;
		}
		void SetMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key, float a_value)
		{
			if (!a_actor || !FindActiveScene(a_actor->GetFormID())) {
				return;
			}
			std::scoped_lock lock(g_actorMetaLock);
			g_actorMeta[a_actor->GetFormID()].floats[LowerCopy(a_key)] = a_value;
		}
		bool HasMetaString(std::monostate, RE::Actor* a_actor, std::string a_key)
		{
			std::scoped_lock lock(g_actorMetaLock);
			const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
			return it != g_actorMeta.end() && it->second.strings.contains(LowerCopy(a_key));
		}
		std::string GetMetaString(std::monostate, RE::Actor* a_actor, std::string a_key)
		{
			std::scoped_lock lock(g_actorMetaLock);
			const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
			if (it == g_actorMeta.end()) {
				return {};
			}
			const auto f = it->second.strings.find(LowerCopy(a_key));
			return f != it->second.strings.end() ? f->second : std::string{};
		}
		void SetMetaString(std::monostate, RE::Actor* a_actor, std::string a_key, std::string a_value)
		{
			if (!a_actor || !FindActiveScene(a_actor->GetFormID())) {
				return;
			}
			std::scoped_lock lock(g_actorMetaLock);
			g_actorMeta[a_actor->GetFormID()].strings[LowerCopy(a_key)] = std::move(a_value);
		}
		// OActor.PlayExpression: the expression's duration in seconds (-1 if
		// the event has none), as OStim's (it returned a bool before 0.5.0).
		float PlayExpression(std::monostate, RE::Actor* a_actor, std::string a_expression)
		{
			return a_actor ? Expressions::PlayEvent(a_actor->GetFormID(), LowerCopy(a_expression), true) : -1.0F;
		}
		void SetExpressionsEnabled(std::monostate, RE::Actor* a_actor, bool a_enabled, bool a_allowOverride)
		{
			if (a_actor) {
				Expressions::SetEnabled(a_actor->GetFormID(), a_enabled, a_allowOverride);
			}
		}
		bool HasExpressionOverride(std::monostate, RE::Actor* a_actor)
		{
			return a_actor && Expressions::HasOverride(a_actor->GetFormID());
		}
		void UndressPartial(std::monostate, RE::Actor* a_actor, std::int32_t a_mask)
		{
			if (a_actor && FindActiveScene(a_actor->GetFormID())) {
				Undress::StripMask(a_actor->GetFormID(), static_cast<std::uint32_t>(a_mask));
			}
		}
		void RedressPartial(std::monostate, RE::Actor* a_actor, std::int32_t a_mask)
		{
			if (a_actor && FindActiveScene(a_actor->GetFormID())) {
				Undress::RedressMask(a_actor->GetFormID(), static_cast<std::uint32_t>(a_mask));
			}
		}
		void ClearExpression(std::monostate, RE::Actor* a_actor)
		{
			if (a_actor) {
				Expressions::ClearEvent(a_actor->GetFormID());
			}
		}
	}  // namespace Natives::FourActor

	// FourFurniture (OFurniture): furniture near a_center (OStim's
	// FindFurniture: the pieces a scene for a_actorCount could go on, nearest
	// first; FindFurnitureOfType: the nearest of a type). a_sameFloor: how far
	// up or down (0 = 4Stim's fFurnitureHeight). Main thread: Papyrus waits
	// for the answer.
	std::vector<Furniture::Found> FurnitureAround(RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor)
	{
		std::vector<Furniture::Found> found;
		if (!a_center) {
			return found;
		}
		const auto     center = a_center->data.location;
		const float    radius = a_radius > 0.0F ? a_radius : g_settings.furnitureRadius;
		const float    height = a_sameFloor > 0.0F ? a_sameFloor : g_settings.furnitureHeight;
		auto search = [&]() {
			auto list = Furniture::FindNear(center, radius, height, false, a_center);
			std::erase_if(list, [](const Furniture::Found& f) {
				return f.ref && (FurnitureBlocked(f.ref) || std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a) { return a.furnitureRef == f.ref; }));
			});
			return list;
		};
		if (std::this_thread::get_id() == g_mainThread) {
			return search();
		}
		// Shared, so a late answer after the wait gave up has somewhere to go.
		struct Answer
		{
			std::mutex                    lock;
			std::condition_variable       cv;
			bool                          ready = false;
			std::vector<Furniture::Found> found;
		};
		auto answer = std::make_shared<Answer>();
		F4SE::GetTaskInterface()->AddTask([answer, center, radius, height, centerID = a_center->GetFormID()]() {
			const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(centerID);
			auto       list = Furniture::FindNear(center, radius, height, false, ref);
			std::erase_if(list, [](const Furniture::Found& f) {
				return f.ref && (FurnitureBlocked(f.ref) || std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a) { return a.furnitureRef == f.ref; }));
			});
			{
				std::scoped_lock lock(answer->lock);
				answer->found = std::move(list);
				answer->ready = true;
			}
			answer->cv.notify_one();
		});
		std::unique_lock lock(answer->lock);
		answer->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return answer->ready; });
		found = std::move(answer->found);
		return found;
	}
	namespace Natives::FourFurniture
	{
		std::vector<RE::TESObjectREFR*> FindFurniture(std::monostate, std::int32_t a_actorCount, RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor)
		{
			std::vector<RE::TESObjectREFR*> out;
			for (const auto& f : FurnitureAround(a_center, a_radius, a_sameFloor)) {
				if (!f.ref) {
					continue;  // walls are spots, not objects
				}
				const bool any = a_actorCount <= 0 || std::ranges::any_of(SceneRegistry::List(static_cast<std::size_t>(a_actorCount)),
					[&](const SceneRegistry::SceneSummary& s) { return !s.furniture.empty() && FurnitureFits(s.furniture, f.type); });
				if (any) {
					out.push_back(RE::TESForm::GetFormByID<RE::TESObjectREFR>(f.ref));
				}
			}
			return out;
		}
		RE::TESObjectREFR* FindFurnitureOfType(std::monostate, std::string a_type, RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor)
		{
			const auto type = LowerCopy(a_type);
			for (const auto& f : FurnitureAround(a_center, a_radius, a_sameFloor)) {
				if (f.ref && Furniture::IsA(f.type, type)) {
					return RE::TESForm::GetFormByID<RE::TESObjectREFR>(f.ref);
				}
			}
			return nullptr;
		}
		// OFurniture.GetOffset: where a scene goes on the piece, in its own
		// frame, as OStim's [x, y, z, rotation, scale] (its marker offset plus
		// the type's offsets; here 4Stim's spot for the type: markers, else its
		// bounds, plus the type's offset). Rotation in radians, as OStim's.
		// Empty for something that isn't furniture a scene type knows.
		std::vector<float> GetOffset(std::monostate, RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) {
				return {};
			}
			const auto type = Furniture::Classify(a_ref);
			if (type.empty()) {
				return {};
			}
			const auto spot = Furniture::SpotFor(a_ref, type, a_ref->data.location);
			const auto d = spot.position - a_ref->data.location;
			const float yaw = a_ref->data.angle.z;
			// Into the piece's frame (the game's heading turns clockwise from north).
			const float x = d.x * std::cos(yaw) - d.y * std::sin(yaw);
			const float y = d.x * std::sin(yaw) + d.y * std::cos(yaw);
			return { x, y, d.z, spot.heading - yaw, Furniture::MultiplyScale(type) };
		}
	}  // namespace Natives::FourFurniture
	namespace Natives::FourActorUtil
	{
		// ---- FourActorUtil (OStim's OActorUtil, docs/API.md) ----
		bool FulfillsCondition(std::monostate, RE::Actor* a_actor, RE::BGSPerk* a_condition)
		{
			return a_actor && a_condition && a_condition->perkConditions.IsTrue(a_actor, a_actor);
		}
		bool FulfillsAnyCondition(std::monostate, RE::Actor* a_actor, std::vector<RE::BGSPerk*> a_conditions)
		{
			return a_actor && std::ranges::any_of(a_conditions, [&](RE::BGSPerk* a_perk) { return a_perk && a_perk->perkConditions.IsTrue(a_actor, a_actor); });
		}
		bool FulfillsAllConditions(std::monostate, RE::Actor* a_actor, std::vector<RE::BGSPerk*> a_conditions)
		{
			return a_actor && std::ranges::all_of(a_conditions, [&](RE::BGSPerk* a_perk) { return !a_perk || a_perk->perkConditions.IsTrue(a_actor, a_actor); });
		}
		std::vector<RE::Actor*> EmptyArray(std::monostate) { return {}; }
		std::vector<RE::Actor*> CreateArray(std::monostate, std::int32_t a_size, RE::Actor* a_filler)
		{
			return a_size > 0 ? std::vector<RE::Actor*>(static_cast<std::size_t>(std::min(a_size, 128)), a_filler) : std::vector<RE::Actor*>{};
		}
		std::vector<RE::Actor*> ToArray(std::monostate, RE::Actor* a1, RE::Actor* a2, RE::Actor* a3, RE::Actor* a4, RE::Actor* a5, RE::Actor* a6, RE::Actor* a7,
			RE::Actor* a8, RE::Actor* a9, RE::Actor* a10)
		{
			std::vector<RE::Actor*> out;
			for (const auto a : { a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 }) {
				if (a) {
					out.push_back(a);
				}
			}
			return out;
		}
		// OActorUtil.Sort (OStim's ActorUtil::sort): the dominant actors first,
		// then those with a penis (otherwise in the order given); then the player
		// moved to a_playerIndex, or for two without one, where the
		// PlayerAlways... settings put them.
		std::vector<RE::Actor*> Sort(std::monostate, std::vector<RE::Actor*> a_actors, std::vector<RE::Actor*> a_dominant, std::int32_t a_playerIndex)
		{
			std::erase(a_actors, nullptr);
			const auto dominant = [&](RE::Actor* a) { return std::ranges::find(a_dominant, a) != a_dominant.end(); };
			std::ranges::stable_sort(a_actors, [&](RE::Actor* a, RE::Actor* b) {
				if (dominant(a) != dominant(b)) {
					return dominant(a);
				}
				return ActorProperties::HasPenis(a) && !ActorProperties::HasPenis(b);
			});
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto at = std::ranges::find(a_actors, player);
			if (at == a_actors.end()) {
				return a_actors;
			}
			int want = a_playerIndex;
			if (want < 0 || static_cast<std::size_t>(want) >= a_actors.size()) {
				std::vector<std::uint32_t> ids;
				for (const auto a : a_actors) {
					ids.push_back(a->GetFormID());
				}
				want = PlayerAlwaysIndex(ids);
			}
			if (want >= 0 && static_cast<std::size_t>(want) < a_actors.size()) {
				a_actors.erase(at);
				a_actors.insert(a_actors.begin() + want, player);
			}
			return a_actors;
		}
		// OActorUtil.GetActorsInRangeV2: actors around a_center, nearest first;
		// not disabled or ghosts; a_condition: only those who fulfill the perk's
		// conditions; a_onlyKnown: only actors with an actor type (OStim's
		// OStimActorsOnly).
		std::vector<RE::Actor*> GetActorsInRangeV2(std::monostate, RE::TESObjectREFR* a_center, float a_range, bool a_includeCenter, bool a_includePlayer,
			bool a_onlyKnown, RE::BGSPerk* a_condition)
		{
			std::vector<RE::Actor*> out;
			if (!a_center) {
				return out;
			}
			const auto center = a_center->data.location;
			Compat::ForEachReferenceInRange(center, a_range, [&](RE::TESObjectREFR* a_ref) {
				const auto actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
				if (!actor || (!a_includeCenter && a_ref == a_center) || (!a_includePlayer && IsPlayer(actor)) || actor->IsDisabled()) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				if (a_condition && !a_condition->perkConditions.IsTrue(actor, actor)) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				if (a_onlyKnown && ActorProperties::Type(actor).empty()) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				out.push_back(actor);
				return RE::BSContainer::ForEachResult::kContinue;
			});
			std::ranges::sort(out, {}, [&](RE::Actor* a) { return a->data.location.GetSquaredDistance(center); });
			return out;
		}
		std::vector<std::string> ActorsToNames(std::monostate, std::vector<RE::Actor*> a_actors)
		{
			std::vector<std::string> out;
			for (const auto a : a_actors) {
				const char* name = a ? a->GetDisplayFullName() : nullptr;
				out.emplace_back(name ? name : "");
			}
			return out;
		}
	}  // namespace Natives::FourActorUtil

	// ---- FourSequence (OStim's OSequence) ----
	// OStim's GraphTable::getRandomSequence: a random sequence for these
	// actors in this order, on this furniture type or a supertype of it ("":
	// sequences without furniture), passing a_mode's tag test (0 none, 1 has
	// a_tags[0], 2 any of them, 3 all of them). "" if none.
	std::string SQ_Random(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_tags, std::string a_csv, std::int32_t a_mode)
	{
		std::erase(a_actors, nullptr);
		if (a_actors.empty()) {
			return {};
		}
		if (!a_csv.empty()) {
			a_tags = SplitCSV(a_csv);
		}
		for (auto& tag : a_tags) {
			tag = LowerCopy(tag);
		}
		const auto                    furniture = LowerCopy(a_furniture);
		SceneRegistry::ListFilter     filter;
		for (const auto a : a_actors) {
			filter.actors.push_back(ActorProperties::Condition(a));
		}
		filter.fixedOrder = true;
		std::vector<std::string> picks;
		for (const auto& summary : SceneRegistry::ListSequences(a_actors.size(), filter)) {
			const auto sequence = SceneRegistry::FindSequence(summary.id);
			if (!sequence) {
				continue;
			}
			if (furniture.empty() ? !sequence->furniture.empty() : (sequence->furniture.empty() || !Furniture::IsA(furniture, sequence->furniture))) {
				continue;
			}
			const auto has = [&](const std::string& t) { return std::ranges::find(sequence->tags, t) != sequence->tags.end(); };
			const bool ok = a_mode == 0 || (a_mode == 1 && !a_tags.empty() && has(a_tags[0])) || (a_mode == 2 && std::ranges::any_of(a_tags, has)) ||
			                (a_mode == 3 && std::ranges::all_of(a_tags, has));
			if (ok) {
				picks.push_back(sequence->id);
			}
		}
		if (picks.empty()) {
			return {};
		}
		static std::mt19937 rng{ std::random_device{}() };
		return picks[std::uniform_int_distribution<std::size_t>(0, picks.size() - 1)(rng)];
	}

	namespace Natives::FourFurniture
	{
		// OFurniture.GetSceneID: the thread using the piece, -1 if none.
		std::int32_t GetSceneID(std::monostate, RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) {
				return -1;
			}
			for (const auto& active : g_activeScenes) {
				if (active.furnitureRef == a_ref->GetFormID()) {
					return active.threadID;
				}
			}
			return -1;
		}
	}  // namespace Natives::FourFurniture

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

		std::mutex                                                   g_apiLock;
		std::unordered_map<std::uint32_t, std::unique_ptr<ActorImpl>> g_apiActors;

		ActorImpl* ActorFor(std::uint32_t a_id)
		{
			std::scoped_lock lock(g_apiLock);
			auto& slot = g_apiActors[a_id];
			if (!slot) {
				slot = std::make_unique<ActorImpl>(a_id);
			}
			return slot.get();
		}

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

		std::unordered_map<std::int32_t, std::unique_ptr<ThreadImpl>> g_apiThreads;

		ThreadImpl* ThreadFor(std::int32_t a_id)
		{
			std::scoped_lock lock(g_apiLock);
			auto& slot = g_apiThreads[a_id];
			if (!slot) {
				slot = std::make_unique<ThreadImpl>(a_id);
			}
			return slot.get();
		}

		// The thread object for a running scene, brought up to date.
		ThreadImpl* ThreadOf(const ActiveScene& a_active)
		{
			const auto thread = ThreadFor(a_active.threadID);
			thread->Refresh(a_active);
			return thread;
		}

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
		ThreadListeners                                     g_onStart, g_onSpeed, g_onNode, g_onStop;
		std::vector<FourStimAPI::ThreadActorEventListener*> g_onClimax;

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

		InterfaceMapImpl* Map()
		{
			static InterfaceMapImpl map;
			static bool             filled = false;
			if (!filled) {
				filled = true;
				static ThreadInterfaceImpl    threads;
				static AlignmentInterfaceImpl alignment;
				map.addInterface(FourStimAPI::ThreadInterface::NAME, &threads);
				map.addInterface(FourStimAPI::AlignmentInterface::NAME, &alignment);
				map.addInterface(FourStimAPI::SexToyInterface::NAME, SexToys::Interface());
			}
			return &map;
		}

		// Another plugin asks for the map (OStim's UnspecificedSenderMessageHandler).
		void OnPluginMessage(F4SE::MessagingInterface::Message* a_msg)
		{
			if (a_msg && a_msg->type == FourStimAPI::InterfaceExchangeMessage::MESSAGE_TYPE && a_msg->data &&
				a_msg->dataLen >= sizeof(FourStimAPI::InterfaceExchangeMessage)) {
				static_cast<FourStimAPI::InterfaceExchangeMessage*>(a_msg->data)->interfaceMap = Map();
				REX::INFO("Plugin interface: handed to {}", a_msg->sender ? a_msg->sender : "a plugin");
			}
		}

		// A game loads: no thread runs any more.
		void ForgetThreads()
		{
			std::scoped_lock lock(g_apiLock);
			for (auto& [id, t] : g_apiThreads) {
				t->running = false;
			}
		}

		// SceneEvents' native listener: the threads' listeners (main thread).
		void NotifyNG(FourStimNG_API::Thread::ThreadEvent a_event, std::uint32_t a_threadID);

		void OnSceneEvent(SceneEvents::Kind a_kind, const std::vector<std::uint32_t>& a_actors, std::uint32_t a_actor)
		{
			// The NG thread API's events: the player's thread only, as OStim's
			// (ThreadManager's listeners); the player's is thread 0.
			if (std::ranges::any_of(a_actors, [](std::uint32_t a) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a)); })) {
				using E = FourStimNG_API::Thread::ThreadEvent;
				switch (a_kind) {
				case SceneEvents::Kind::kStart: NotifyNG(E::ThreadStarted, 0); break;
				case SceneEvents::Kind::kChange: NotifyNG(E::NodeChanged, 0); break;
				case SceneEvents::Kind::kEnd: NotifyNG(E::ThreadEnded, 0); break;
				default: break;
				}
			}
			ThreadImpl* thread = nullptr;
			if (a_kind == SceneEvents::Kind::kEnd) {
				// The scene's gone already: the thread with these actors.
				std::scoped_lock lock(g_apiLock);
				for (auto& [id, t] : g_apiThreads) {
					if (t->running && t->actors == a_actors) {
						thread = t.get();
						break;
					}
				}
				if (!thread) {
					return;  // never asked for, so never known: nobody can be holding it
				}
			} else {
				const auto active = a_actors.empty() ? nullptr : FindActiveScene(a_actors.front());
				if (!active) {
					return;
				}
				thread = ThreadOf(*active);
			}
			ThreadListeners                                     listeners;
			std::vector<FourStimAPI::ThreadActorEventListener*> climax;
			{
				std::scoped_lock lock(g_apiLock);
				switch (a_kind) {
				case SceneEvents::Kind::kStart:
					listeners = g_onStart;
					break;
				case SceneEvents::Kind::kChange:
					listeners = g_onNode;
					break;
				case SceneEvents::Kind::kSpeed:
					listeners = g_onSpeed;
					break;
				case SceneEvents::Kind::kEnd:
					listeners = g_onStop;
					break;
				case SceneEvents::Kind::kClimax:
					climax = g_onClimax;
					break;
				}
			}
			for (const auto listener : listeners) {
				listener->listen(thread);
			}
			for (const auto listener : climax) {
				listener->listen(thread, ActorFor(a_actor));
			}
			if (a_kind == SceneEvents::Kind::kEnd) {
				thread->running = false;
			}
		}

		// ---- OStim's second C++ API, OstimNG-API (include/4Stim/FourStimNG-API.h) ----
		// OStim's ModAPI: a Scene interface (start / stop scenes) and a Thread
		// interface for external scene UIs, from the exports
		// RequestPluginAPI_Scene and RequestPluginAPI_Thread.

		namespace NG = FourStimNG_API;

		// OStim's Scene API starts a thread from its actors, furniture and starting scene.
		NG::Scene::APIResult StartNGScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene,
			std::vector<RE::Actor*> a_actors, std::uint32_t* a_threadID)
		{
			if (a_actors.empty() || std::ranges::any_of(a_actors, [](RE::Actor* a) { return a == nullptr; })) {
				return NG::Scene::APIResult::Invalid;
			}
			const auto builder = BuilderCreate(std::move(a_actors));
			if (builder < 0) {
				return NG::Scene::APIResult::Failed;
			}
			if (a_furniture) {
				Natives::FourThreadBuilder::SetFurniture({}, builder, a_furniture);
			}
			if (a_scene && SceneRegistry::Find(a_scene)) {
				Natives::FourThreadBuilder::SetStartingAnimation({}, builder, a_scene);
			}
			const auto id = Natives::FourThreadBuilder::Start({}, builder);
			if (id < 0) {
				return NG::Scene::APIResult::Failed;
			}
			if (a_threadID) {
				*a_threadID = static_cast<std::uint32_t>(id);
			}
			REX::INFO("Scene API: thread {} started by {}", id, a_plugin ? a_plugin : "a plugin");
			return NG::Scene::APIResult::OK;
		}

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

		// The scene a navigation ends up at (past its transitions).
		std::shared_ptr<const SceneRegistry::Scene> NavDestination(const std::string& a_to)
		{
			const auto scene = SceneRegistry::Find(a_to);
			return scene ? SceneRegistry::Settled(scene) : nullptr;
		}

		FourStim::FocusedScene AsFocused(const ActiveScene& a_active)
		{
			FourStim::FocusedScene f;
			f.role0 = a_active.actors.empty() ? 0 : a_active.actors[0];
			f.role1 = a_active.actors.size() > 1 ? a_active.actors[1] : 0;
			if (a_active.actors.size() > 2) {
				f.more.assign(a_active.actors.begin() + 2, a_active.actors.end());
			}
			f.sceneID = a_active.sceneID;
			f.speed = a_active.speed;
			f.furniture = a_active.furnitureType;
			return f;
		}

		std::atomic<bool> g_externalUI = false;

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

		NGThreadInterface* NGThreads()
		{
			static NGThreadInterface threads;
			static AlignmentInterfaceImpl alignment;
			threads.alignment = &alignment;
			return &threads;
		}

		NGSceneInterface* NGScenes()
		{
			static NGSceneInterface scenes;
			return &scenes;
		}

		void NotifyNG(FourStimNG_API::Thread::ThreadEvent a_event, std::uint32_t a_threadID)
		{
			NGThreads()->Notify(a_event, a_threadID);
		}

		// OStim's ThreadManager::migrateThread: the old thread ends, and once
		// its actors are free (and a_delayMs more), a new one starts with
		// a_actors on the same furniture, auto mode as it was, the actors'
		// excitement carried over. a_onComplete gets the new ID or -1.
		std::int32_t NGThreadInterface::MigrateThread(std::uint32_t a_id, const std::uint32_t* a_actors, std::uint32_t a_count,
			void (*a_onComplete)(std::int32_t, void*), void* a_context, int a_delayMs) noexcept
		{
			const auto fail = [&]() {
				if (a_onComplete) {
					a_onComplete(-1, a_context);
				}
				return -1;
			};
			const auto oldID = static_cast<std::int32_t>(a_id);
			const auto active = FindThread(oldID);
			if (!active || !a_actors || a_count == 0) {
				return fail();
			}
			std::vector<std::uint32_t> cast(a_actors, a_actors + a_count);
			if (std::ranges::any_of(cast, [](std::uint32_t a) { return !RE::TESForm::GetFormByID<RE::Actor>(a); })) {
				return fail();
			}
			if (!a_onComplete && std::this_thread::get_id() == g_mainThread) {
				REX::WARN("Thread API: MigrateThread without a callback on the main thread would wait on itself: refused");
				return -1;
			}
			const auto furniture = active->furnitureRef;
			const bool autoMode = active->autoMode.on;
			std::vector<std::pair<std::uint32_t, float>> excitement;
			for (const auto id : cast) {
				excitement.emplace_back(id, Excitement::Get(id));
			}
			auto promise = std::make_shared<std::promise<std::int32_t>>();
			auto future = promise->get_future();
			const auto done = [a_onComplete, a_context, promise](std::int32_t a_newID) {
				if (a_onComplete) {
					a_onComplete(a_newID, a_context);
				} else {
					promise->set_value(a_newID);
				}
			};
			WithThread(oldID, [](ActiveScene& a) { EndActiveScene(a, "moved to a new thread by another plugin (Thread API MigrateThread)"); });
			std::thread([oldID, cast, furniture, autoMode, excitement, done, a_delayMs]() {
				// Wait for the old thread's actors to be released (10 s at most).
				for (int i = 0; i < 100; ++i) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					auto free = std::make_shared<std::promise<bool>>();
					auto answer = free->get_future();
					F4SE::GetTaskInterface()->AddTask([free, oldID, cast]() {
						free->set_value(!FindThread(oldID) && std::ranges::none_of(cast, [](std::uint32_t a) { return FindActiveScene(a) != nullptr; }));
					});
					if (answer.wait_for(std::chrono::seconds(2)) == std::future_status::ready && answer.get()) {
						std::this_thread::sleep_for(std::chrono::milliseconds(std::max(a_delayMs, 0)));
						F4SE::GetTaskInterface()->AddTask([cast, furniture, autoMode, excitement, done]() {
							std::vector<RE::Actor*> actors;
							for (const auto id : cast) {
								actors.push_back(RE::TESForm::GetFormByID<RE::Actor>(id));
							}
							const auto builder = std::ranges::any_of(actors, [](RE::Actor* a) { return !a; }) ? -1 : BuilderCreate(actors);
							if (builder < 0) {
								done(-1);
								return;
							}
							if (const auto ref = furniture ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(furniture) : nullptr) {
								Natives::FourThreadBuilder::SetFurniture({}, builder, ref);
							}
							if (!autoMode) {
								Natives::FourThreadBuilder::NoAutoMode({}, builder);
							}
							const auto id = Natives::FourThreadBuilder::Start({}, builder);
							if (id >= 0) {
								for (const auto& [actor, value] : excitement) {
									Excitement::Set(actor, value);
								}
								if (autoMode) {
									Natives::FourThread::StartAutoMode({}, id);
								}
							}
							REX::INFO("Thread API: MigrateThread: new thread {}", id);
							done(id);
						});
						return;
					}
				}
				REX::WARN("Thread API: MigrateThread: thread {}'s actors weren't released in time", oldID);
				done(-1);
			}).detach();
			if (a_onComplete) {
				return 0;
			}
			return future.get();
		}

		// FourStimTest.PluginAPI's check: 4Stim's own exports, found the way
		// another plugin finds them (GetProcAddress on 4Stim.dll), and the
		// player's thread read through them. The event and control callbacks
		// it registers log to 4Stim.log from then on.
		void LogNGEvent(NG::Thread::ThreadEvent a_event, std::uint32_t a_thread, void*)
		{
			REX::INFO("Thread API test: event {} on thread {}", static_cast<int>(a_event), a_thread);
		}
		void LogNGControl(NG::Thread::Controls a_control, std::uint32_t a_thread, void*)
		{
			REX::INFO("Thread API test: control {} on thread {}", static_cast<int>(a_control), a_thread);
		}

		std::string SelfTest()
		{
			std::string out;
			const auto  line = [&](bool a_ok, const std::string& a_text) {
				out += std::format("{} {}\n", a_ok ? "ok" : "FAIL", a_text);
				REX::INFO("Thread API test: {} {}", a_ok ? "ok" : "FAIL", a_text);
			};
			const auto module = REX::W32::GetModuleHandleA("4Stim.dll");
			const auto mapFn = module ? reinterpret_cast<FourStimAPI::InterfaceMap* (*)()>(REX::W32::GetProcAddress(module, "requestInterfaceMap")) : nullptr;
			const auto sceneFn = module ? reinterpret_cast<NG::Scene::_RequestPluginAPI_Scene>(REX::W32::GetProcAddress(module, "RequestPluginAPI_Scene")) : nullptr;
			const auto threadFn = module ? reinterpret_cast<NG::Thread::_RequestPluginAPI_Thread>(REX::W32::GetProcAddress(module, "RequestPluginAPI_Thread")) : nullptr;
			line(module != nullptr, "4Stim.dll found");
			line(mapFn && mapFn() && mapFn()->queryInterface(FourStimAPI::ThreadInterface::NAME), "requestInterfaceMap: \"Threads\" in it");
			const auto scenes = sceneFn ? sceneFn(NG::Scene::InterfaceVersion::V1, "4StimSelfTest", { 1, 0, 0, 0 }) : nullptr;
			const auto threads = threadFn ? threadFn(NG::Thread::InterfaceVersion::V1, "4StimSelfTest", { 1, 0, 0, 0 }) : nullptr;
			line(scenes != nullptr, "RequestPluginAPI_Scene");
			line(threads != nullptr, "RequestPluginAPI_Thread");
			if (!threads || !scenes) {
				return out;
			}
			threads->UnregisterEventCallback(LogNGEvent);
			threads->RegisterEventCallback(LogNGEvent, nullptr);
			threads->UnregisterControlCallback(LogNGControl);
			threads->RegisterControlCallback(LogNGControl, nullptr);
			NG::Thread::KeyData keys{};
			threads->GetKeyData(&keys);
			line(true, std::format("GetKeyData: end {:X}, search {:X}, align {:X}, up {:X}", keys.keyEnd, keys.keySearch, keys.keyAlignment, keys.keyUp));
			const std::uint32_t id = threads->GetPlayerThreadID();
			if (!threads->IsThreadValid(id)) {
				line(true, "no player thread: start a scene and run it again for the rest");
				return out;
			}
			const auto sceneID = threads->GetCurrentSceneID(id);
			line(sceneID != nullptr, std::format("GetCurrentSceneID: {}", sceneID ? sceneID : "(none)"));
			const auto name = threads->GetCurrentNodeName(id);
			line(name != nullptr, std::format("GetCurrentNodeName: {}", name ? name : "(none)"));
			NG::Thread::ActorData actors[8]{};
			const auto count = threads->GetActors(id, actors, 8);
			line(count == threads->GetActorCount(id) && count > 0, std::format("GetActors: {} (excitement {:.0f}, female {}, schlong {})", count, actors[0].excitement, actors[0].isFemale, actors[0].hasSchlong));
			line(threads->GetActorPosition(id, actors[0].formID) == 0 && threads->IsActorInAnyThread(actors[0].formID), "GetActorPosition / IsActorInAnyThread");
			std::vector<std::uint32_t> ids;
			for (std::uint32_t i = 0; i < count; ++i) {
				ids.push_back(actors[i].formID);
			}
			line(threads->HasCompatibleNode(id, ids.data(), count), "HasCompatibleNode (the same actors)");
			NG::Thread::NavigationData navs[32]{};
			const auto navCount = threads->GetNavigationOptions(id, navs, 32);
			line(navCount == threads->GetNavigationCount(id), std::format("GetNavigationOptions: {}{}", navCount, navCount ? std::format(" (first: {} -> {}, \"{}\")", navs[0].sceneId, navs[0].destinationId, navs[0].description) : ""));
			line(true, std::format("speed {} of {}, transition {}, sequence {}, auto mode {}, player control off {}", threads->GetCurrentSpeed(id), threads->GetMaxSpeed(id),
				threads->IsTransition(id), threads->IsInSequence(id), threads->IsAutoMode(id), threads->IsPlayerControlDisabled(id)));
			bool autoMode = false;
			line(scenes->TryGetAutoMode("4StimSelfTest", id, &autoMode) == NG::Scene::APIResult::OK && autoMode == threads->IsAutoMode(id), "Scene API TryGetAutoMode");
			NG::Thread::ActorAlignmentData align{};
			const bool aligned = threads->GetActorAlignment(id, 0, &align);  // before the text: argument order isn't left to right
			line(aligned, std::format("GetActorAlignment 0: {:.1f} {:.1f} {:.1f}, scale {:.2f}, rotation {:.1f}, bend {:.1f}", align.offsetX, align.offsetY, align.offsetZ, align.scale, align.rotation, align.sosBend));
			NG::Thread::SceneSearchResult found[8]{};
			const auto hits = threads->SearchScenes("a", found, 8);
			line(true, std::format("SearchScenes \"a\": {}{}", hits, hits ? std::format(" (first: {})", found[0].name) : ""));
			NG::Thread::SceneSearchResult info{};
			line(sceneID && threads->GetSceneInfo(sceneID, &info) && info.actorCount == count, "GetSceneInfo (the current scene)");
			NG::Thread::OptionsMenuItem items[32]{};
			const auto itemCount = threads->GetOptionsItems(items, 32);
			line(itemCount == threads->GetOptionsItemCount() + 1 && std::string_view(items[0].id) == "-1", std::format("GetOptionsItems: back / exit + {}, at root {}", itemCount - 1, threads->IsOptionsAtRoot()));
			return out;
		}

		// A navigation-type key pressed in a scene the player is in or
		// watches: OStim's UIState::HandleControl hands it to the thread
		// API's control callbacks. Main thread.
		void NotifyNGControl(FourStimNG_API::Thread::Controls a_control)
		{
			const auto focused = GetPlayerScene();
			const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
			if (active) {
				NGThreads()->NotifyControl(a_control, static_cast<std::uint32_t>(active->threadID));
			}
		}
	}

	// FourStim.TestPluginAPI (FourStimTest.PluginAPI): the C++ API's self-check.
	std::string TestPluginAPI(std::monostate) { return API::SelfTest(); }

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		g_vm = a_vm;

		REX::INFO("RegisterPapyrusFunctions called, binding {}", SCRIPT_NAME);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartScene"sv, StartScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "TestPluginAPI"sv, TestPluginAPI);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopScene"sv, StopScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SuppressInteraction"sv, SuppressInteraction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RestoreInteraction"sv, RestoreInteraction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "LockActor"sv, LockActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnlockActor"sv, UnlockActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "MoveActorTo"sv, MoveActorTo);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSelectedActorID"sv, GetSelectedActorID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlacePair"sv, PlacePair);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IgnorePairCollision"sv, IgnorePairCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RestoreCollision"sv, RestoreCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlaySceneIdles"sv, PlaySceneIdles);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActorCount"sv, GetSceneActorCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "QueueSequence"sv, QueueSequenceNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartSequenceOnScene"sv, StartSequenceOnSceneNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopSequence"sv, StopSequenceNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ReloadScenes"sv, ReloadScenes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneHasAction"sv, SceneHasAction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneHasActionTag"sv, SceneHasActionTag);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActionCount"sv, GetSceneActionCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "FindSceneAction"sv, FindSceneAction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActionType"sv, GetSceneActionType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActionRole"sv, GetSceneActionRole);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionTypes"sv, GetActionTypes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "CallEvent"sv, CallEvent);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEvents"sv, GetEvents);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionName"sv, GetActionName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionTags"sv, GetActionTags);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ActionHasTag"sv, ActionHasTag);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetExcitement"sv, GetExcitement);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetExcitement"sv, SetExcitement);
		a_vm->BindNativeMethod(SCRIPT_NAME, "AddExcitement"sv, AddExcitement);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetTimesClimaxed"sv, GetTimesClimaxed);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Climax"sv, Climax);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StallClimax"sv, StallClimax);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsClimaxStalled"sv, IsClimaxStalled);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetExcitementMultiplier"sv, GetExcitementMultiplier);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetExcitementMultiplier"sv, SetExcitementMultiplier);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetTimeUntilClimax"sv, GetTimeUntilClimax);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetAutoMode"sv, SetAutoMode);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NoteStripped"sv, NoteStripped);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UndressActor"sv, UndressActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RedressActor"sv, RedressActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsInScene"sv, IsInScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsAutoMode"sv, IsAutoMode);
		a_vm->BindNativeMethod(SCRIPT_NAME, "AutoTransition"sv, AutoTransition);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneActorHasTag"sv, SceneActorHasTag);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActorTags"sv, GetSceneActorTags);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopPair"sv, StopPair);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SaveStartView"sv, SaveStartView);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenScenePicker"sv, OpenScenePicker);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenNPCScenePicker"sv, OpenNPCScenePicker);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenSceneNavigation"sv, OpenSceneNavigation);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ChangeSceneSpeed"sv, ChangeSceneSpeed);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NavigateScene"sv, NavigateScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "BeginSceneCamera"sv, BeginSceneCamera);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UseFades"sv, UseFades);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetSceneFade"sv, SetSceneFade);
		a_vm->BindNativeMethod(SCRIPT_NAME, "EndSceneCamera"sv, EndSceneCamera);
		a_vm->BindNativeMethod(SCRIPT_NAME, "LockPlayerControls"sv, LockPlayerControls);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnlockPlayerControls"sv, UnlockPlayerControls);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetPlayerSceneActorID"sv, GetPlayerSceneActorID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetPlayerSceneActorCount"sv, GetPlayerSceneActorCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IgnoreGroupCollision"sv, IgnoreGroupCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlaceGroup"sv, PlaceGroup);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlayGroupIdles"sv, PlayGroupIdles);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopGroup"sv, StopGroup);
		a_vm->BindNativeMethod(SCRIPT_NAME, "EquipObject"sv, EquipObject);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnequipObject"sv, UnequipObject);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsObjectEquipped"sv, IsObjectEquipped);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetObjectVariant"sv, SetObjectVariant);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnsetObjectVariant"sv, UnsetObjectVariant);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectTypes"sv, GetEquipObjectTypes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "HasPenis"sv, HasPenis);
		a_vm->BindNativeMethod(SCRIPT_NAME, "HeelsChanged"sv, HeelsChanged);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActorType"sv, GetActorType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NoteFurnitureBlocked"sv, NoteFurnitureBlocked);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ApplyFurnitureChange"sv, ApplyFurnitureChangeNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ChangeFurniture"sv, ChangeFurnitureNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetFurnitureType"sv, GetFurnitureType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsFurnitureChildOf"sv, IsFurnitureChildOf);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsEventChildOf"sv, IsEventChildOf);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetUndressingSlotMask"sv, GetUndressingSlotMask);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetUndressingSlotMask"sv, SetUndressingSlotMask);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Localize"sv, LocalizeText);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneFurniture"sv, GetSceneFurniture);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneFurnitureType"sv, GetSceneFurnitureType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenFurnitureMenu"sv, OpenFurnitureMenu);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ReloadSettings"sv, ReloadSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ExportSettings"sv, ExportSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ImportSettings"sv, ImportSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UseIniSettings"sv, UseIniSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PressKey"sv, PressKey);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Mute"sv, Mute);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Unmute"sv, Unmute);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsMuted"sv, IsMuted);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneReleased"sv, SceneReleased);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetVoiceSetPairs"sv, GetVoiceSetPairs);
		{
			using K = StimOverrides::Kind;
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActions"sv, GetActions);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionStimulation"sv, GetStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetActionStimulation"sv, SetStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionMaxStimulation"sv, GetMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetActionMaxStimulation"sv, SetMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionDefaultStimulation"sv, GetDefaultStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionDefaultMaxStimulation"sv, GetDefaultMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetActionStimulation"sv, ResetStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetActionMaxStimulation"sv, ResetMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventStimulation"sv, GetStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetEventStimulation"sv, SetStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventMaxStimulation"sv, GetMaxStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetEventMaxStimulation"sv, SetMaxStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventDefaultStimulation"sv, GetDefaultStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventDefaultMaxStimulation"sv, GetDefaultMaxStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetEventStimulation"sv, ResetStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetEventMaxStimulation"sv, ResetMaxStim<K::kEvent>);
		}
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetVoiceSetName"sv, GetVoiceSetName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetVoiceSet"sv, SetVoiceSet);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetActorVoiceSet"sv, SetActorVoiceSet);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActorVoiceSetName"sv, GetActorVoiceSetName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RememberSceneWeapon"sv, RememberSceneWeapon);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SheatheForScene"sv, SheatheForScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RemoveWeaponsAtStart"sv, RemoveWeaponsAtStart);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlayerAIDrivenInScenes"sv, PlayerAIDrivenInScenes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "TakeSceneWeaponID"sv, TakeSceneWeaponID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetEquipObjectID"sv, SetEquipObjectID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectID"sv, GetEquipObjectID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectPairs"sv, GetEquipObjectPairs);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectName"sv, GetEquipObjectName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetFormEquipObjectID"sv, SetFormEquipObjectID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RegisterForSceneEvents"sv, RegisterForSceneEvents);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnregisterForSceneEvents"sv, UnregisterForSceneEvents);
		// OStim's API scripts (docs/API.md).
		constexpr auto THREAD = "FourThread"sv;
		a_vm->BindNativeMethod(THREAD, "QuickStart"sv, Natives::FourThread::QuickStart);
		a_vm->BindNativeMethod(THREAD, "IsRunning"sv, Natives::FourThread::IsRunning);
		a_vm->BindNativeMethod(THREAD, "Stop"sv, Natives::FourThread::Stop);
		a_vm->BindNativeMethod(THREAD, "GetThreadCount"sv, Natives::FourThread::GetThreadCount);
		a_vm->BindNativeMethod(THREAD, "GetAllThreadIDs"sv, Natives::FourThread::GetAllThreadIDs);
		a_vm->BindNativeMethod(THREAD, "GetScene"sv, Natives::FourThread::GetScene);
		a_vm->BindNativeMethod(THREAD, "NavigateTo"sv, Natives::FourThread::NavigateTo);
		a_vm->BindNativeMethod(THREAD, "WarpTo"sv, Natives::FourThread::WarpTo);
		a_vm->BindNativeMethod(THREAD, "GetSpeed"sv, Natives::FourThread::GetSpeed);
		a_vm->BindNativeMethod(THREAD, "SetSpeed"sv, Natives::FourThread::SetSpeed);
		a_vm->BindNativeMethod(THREAD, "GetActors"sv, Natives::FourThread::GetActors);
		a_vm->BindNativeMethod(THREAD, "GetActor"sv, Natives::FourThread::GetActor);
		a_vm->BindNativeMethod(THREAD, "GetActorPosition"sv, Natives::FourThread::GetActorPosition);
		a_vm->BindNativeMethod(THREAD, "StallClimax"sv, Natives::FourThread::StallClimax);
		a_vm->BindNativeMethod(THREAD, "PermitClimax"sv, Natives::FourThread::PermitClimax);
		a_vm->BindNativeMethod(THREAD, "IsClimaxStalled"sv, Natives::FourThread::IsClimaxStalled);
		a_vm->BindNativeMethod(THREAD, "GetFurniture"sv, Natives::FourThread::GetFurniture);
		a_vm->BindNativeMethod(THREAD, "GetFurnitureType"sv, Natives::FourThread::GetFurnitureType);
		a_vm->BindNativeMethod(THREAD, "IsInAutoMode"sv, Natives::FourThread::IsInAutoMode);
		a_vm->BindNativeMethod(THREAD, "StartAutoMode"sv, Natives::FourThread::StartAutoMode);
		a_vm->BindNativeMethod(THREAD, "StopAutoMode"sv, Natives::FourThread::StopAutoMode);
		a_vm->BindNativeMethod(THREAD, "HasMetadata"sv, Natives::FourThread::HasMetadata);
		a_vm->BindNativeMethod(THREAD, "AddMetadata"sv, Natives::FourThread::AddMetadata);
		a_vm->BindNativeMethod(THREAD, "GetMetadata"sv, Natives::FourThread::GetMetadata);
		a_vm->BindNativeMethod(THREAD, "HasMetaFloat"sv, Natives::FourThread::HasMetaFloat);
		a_vm->BindNativeMethod(THREAD, "GetMetaFloat"sv, Natives::FourThread::GetMetaFloat);
		a_vm->BindNativeMethod(THREAD, "SetMetaFloat"sv, Natives::FourThread::SetMetaFloat);
		a_vm->BindNativeMethod(THREAD, "HasMetaString"sv, Natives::FourThread::HasMetaString);
		a_vm->BindNativeMethod(THREAD, "GetMetaString"sv, Natives::FourThread::GetMetaString);
		a_vm->BindNativeMethod(THREAD, "SetMetaString"sv, Natives::FourThread::SetMetaString);
		a_vm->BindNativeMethod(THREAD, "PlaySequence"sv, Natives::FourThread::PlaySequence);
		constexpr auto BUILDER = "FourThreadBuilder"sv;
		a_vm->BindNativeMethod(BUILDER, "Create"sv, Natives::FourThreadBuilder::Create);
		a_vm->BindNativeMethod(BUILDER, "SetDominantActors"sv, Natives::FourThreadBuilder::SetDominantActors);
		a_vm->BindNativeMethod(BUILDER, "SetFurniture"sv, Natives::FourThreadBuilder::SetFurniture);
		a_vm->BindNativeMethod(BUILDER, "SetDuration"sv, Natives::FourThreadBuilder::SetDuration);
		a_vm->BindNativeMethod(BUILDER, "SetStartingAnimation"sv, Natives::FourThreadBuilder::SetStartingAnimation);
		a_vm->BindNativeMethod(BUILDER, "AddStartingAnimation"sv, Natives::FourThreadBuilder::AddStartingAnimation);
		a_vm->BindNativeMethod(BUILDER, "SetStartingSequence"sv, Natives::FourThreadBuilder::SetStartingSequence);
		a_vm->BindNativeMethod(BUILDER, "ConcatStartingSequence"sv, Natives::FourThreadBuilder::ConcatStartingSequence);
		a_vm->BindNativeMethod(BUILDER, "EndAfterSequence"sv, Natives::FourThreadBuilder::EndAfterSequence);
		a_vm->BindNativeMethod(BUILDER, "UndressActors"sv, Natives::FourThreadBuilder::UndressActors);
		a_vm->BindNativeMethod(BUILDER, "NoAutoMode"sv, Natives::FourThreadBuilder::NoAutoMode);
		a_vm->BindNativeMethod(BUILDER, "NoPlayerControl"sv, Natives::FourThreadBuilder::NoPlayerControl);
		a_vm->BindNativeMethod(BUILDER, "NoUndressing"sv, Natives::FourThreadBuilder::NoUndressing);
		a_vm->BindNativeMethod(BUILDER, "NoPostDialogue"sv, Natives::FourThreadBuilder::NoPostDialogue);
		a_vm->BindNativeMethod(BUILDER, "NoFurniture"sv, Natives::FourThreadBuilder::NoFurniture);
		a_vm->BindNativeMethod(BUILDER, "SetMetadata"sv, Natives::FourThreadBuilder::SetMetadata);
		a_vm->BindNativeMethod(BUILDER, "SetMetadataCSV"sv, Natives::FourThreadBuilder::SetMetadataCSV);
		a_vm->BindNativeMethod(BUILDER, "Start"sv, Natives::FourThreadBuilder::Start);
		a_vm->BindNativeMethod(BUILDER, "Cancel"sv, Natives::FourThreadBuilder::Cancel);
		constexpr auto LIBRARY = "FourLibrary"sv;
		a_vm->BindNativeMethod(LIBRARY, "FindRandomScene"sv, Natives::FourLibrary::FindRandomScene);
		a_vm->BindNativeMethod(LIBRARY, "FindRandomSceneByRoleTags"sv, Natives::FourLibrary::FindRandomSceneByRoleTags);
		a_vm->BindNativeMethod(LIBRARY, "FindRandomSceneByActionRoles"sv, Natives::FourLibrary::FindRandomSceneByActionRoles);
		a_vm->BindNativeMethod(LIBRARY, "FindRandomSceneSuperload"sv, Natives::FourLibrary::FindRandomSceneSuperload);
		a_vm->BindNativeMethod(LIBRARY, "GetScenesInRange"sv, Natives::FourLibrary::GetScenesInRange);
		a_vm->BindNativeMethod(LIBRARY, "SplitCSV"sv, Natives::FourLibrary::SplitCSV);
		a_vm->BindNativeMethod(LIBRARY, "GetAllScenes"sv, Natives::FourLibrary::GetAllScenes);
		constexpr auto METADATA = "FourMetadata"sv;
		a_vm->BindNativeMethod(METADATA, "GetName"sv, Natives::FourMetadata::GetName);
		a_vm->BindNativeMethod(METADATA, "IsTransition"sv, Natives::FourMetadata::IsTransition);
		a_vm->BindNativeMethod(METADATA, "GetDestination"sv, Natives::FourMetadata::GetDestination);
		a_vm->BindNativeMethod(METADATA, "GetSceneTags"sv, Natives::FourMetadata::GetSceneTags);
		a_vm->BindNativeMethod(METADATA, "GetFurnitureType"sv, Natives::FourMetadata::GetFurnitureType);
		a_vm->BindNativeMethod(METADATA, "GetMaxSpeed"sv, Natives::FourMetadata::GetMaxSpeed);
		a_vm->BindNativeMethod(METADATA, "GetDefaultSpeed"sv, Natives::FourMetadata::GetDefaultSpeed);
		a_vm->BindNativeMethod(METADATA, "GetNavigations"sv, Natives::FourMetadata::GetNavigations);
		a_vm->BindNativeMethod(METADATA, "GetAutoTransition"sv, Natives::FourMetadata::GetAutoTransition);
		a_vm->BindNativeMethod(METADATA, "GetAutoTransitionForActor"sv, Natives::FourMetadata::GetAutoTransitionForActor);
		a_vm->BindNativeMethod(METADATA, "IsNoRandomSelection"sv, Natives::FourMetadata::IsNoRandomSelection);
		a_vm->BindNativeMethod(METADATA, "GetModpack"sv, Natives::FourMetadata::GetModpack);
		a_vm->BindNativeMethod(METADATA, "ScenesToNames"sv, Natives::FourMetadata::ScenesToNames);
		a_vm->BindNativeMethod(METADATA, "IntsOfCSV"sv, Natives::FourMetadata::IntsOfCSV);
		a_vm->BindNativeMethod(METADATA, "FloatsOfCSV"sv, Natives::FourMetadata::FloatsOfCSV);
		a_vm->BindNativeMethod(METADATA, "IntListOverlap"sv, Natives::FourMetadata::IntListOverlap);
		a_vm->BindNativeMethod(METADATA, "FloatListOverlap"sv, Natives::FourMetadata::FloatListOverlap);
		a_vm->BindNativeMethod(METADATA, "StringListOverlap"sv, Natives::FourMetadata::StringListOverlap);
		a_vm->BindNativeMethod(METADATA, "GetAnimationId"sv, Natives::FourMetadata::GetAnimationId);
		a_vm->BindNativeMethod(METADATA, "GetActorRequirements"sv, Natives::FourMetadata::GetActorRequirements);
		a_vm->BindNativeMethod(METADATA, "GetActionTypes"sv, Natives::FourMetadata::GetActionTypes);
		a_vm->BindNativeMethod(METADATA, "GetActionActors"sv, Natives::FourMetadata::GetActionActors);
		a_vm->BindNativeMethod(METADATA, "GetActionTargets"sv, Natives::FourMetadata::GetActionTargets);
		a_vm->BindNativeMethod(METADATA, "GetActionPerformers"sv, Natives::FourMetadata::GetActionPerformers);
		a_vm->BindNativeMethod(METADATA, "GetActionTags"sv, Natives::FourMetadata::GetActionTags);
		a_vm->BindNativeMethod(METADATA, "GetAllActionsTags"sv, Natives::FourMetadata::GetAllActionsTags);
		a_vm->BindNativeMethod(METADATA, "ListHasAny"sv, Natives::FourMetadata::ListHasAny);
		a_vm->BindNativeMethod(METADATA, "ListHasAll"sv, Natives::FourMetadata::ListHasAll);
		a_vm->BindNativeMethod(METADATA, "ListOverlap"sv, Natives::FourMetadata::ListOverlap);
		a_vm->BindNativeMethod(METADATA, "FindActionsBy"sv, Natives::FourMetadata::FindActionsBy);
		a_vm->BindNativeMethod(METADATA, "FindActionsSuperload"sv, Natives::FourMetadata::FindActionsSuperload);
		a_vm->BindNativeMethod(METADATA, "HasCustomAction"sv, Natives::FourMetadata::HasCustomAction);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionInt"sv, Natives::FourMetadata::GetCustomActionInt);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionFloat"sv, Natives::FourMetadata::GetCustomActionFloat);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionString"sv, Natives::FourMetadata::GetCustomActionString);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionIntList"sv, Natives::FourMetadata::GetCustomActionIntList);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionFloatList"sv, Natives::FourMetadata::GetCustomActionFloatList);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionStringList"sv, Natives::FourMetadata::GetCustomActionStringList);
		a_vm->BindNativeMethod(METADATA, "GetCustomSceneActorInt"sv, Natives::FourMetadata::GetCustomSceneActorInt);
		a_vm->BindNativeMethod(METADATA, "GetCustomSceneActorFloat"sv, Natives::FourMetadata::GetCustomSceneActorFloat);
		constexpr auto ACTOR = "FourActor"sv;
		a_vm->BindNativeMethod(ACTOR, "GetThreadID"sv, Natives::FourActor::GetThreadID);
		a_vm->BindNativeMethod(ACTOR, "HasMetadata"sv, Natives::FourActor::HasMetadata);
		a_vm->BindNativeMethod(ACTOR, "AddMetadata"sv, Natives::FourActor::AddMetadata);
		a_vm->BindNativeMethod(ACTOR, "GetMetadata"sv, Natives::FourActor::GetMetadata);
		a_vm->BindNativeMethod(ACTOR, "HasMetaFloat"sv, Natives::FourActor::HasMetaFloat);
		a_vm->BindNativeMethod(ACTOR, "GetMetaFloat"sv, Natives::FourActor::GetMetaFloat);
		a_vm->BindNativeMethod(ACTOR, "SetMetaFloat"sv, Natives::FourActor::SetMetaFloat);
		a_vm->BindNativeMethod(ACTOR, "HasMetaString"sv, Natives::FourActor::HasMetaString);
		a_vm->BindNativeMethod(ACTOR, "GetMetaString"sv, Natives::FourActor::GetMetaString);
		a_vm->BindNativeMethod(ACTOR, "SetMetaString"sv, Natives::FourActor::SetMetaString);
		a_vm->BindNativeMethod(ACTOR, "PlayExpression"sv, Natives::FourActor::PlayExpression);
		a_vm->BindNativeMethod(ACTOR, "ClearExpression"sv, Natives::FourActor::ClearExpression);
		a_vm->BindNativeMethod(ACTOR, "SetExpressionsEnabled"sv, Natives::FourActor::SetExpressionsEnabled);
		a_vm->BindNativeMethod(ACTOR, "HasExpressionOverride"sv, Natives::FourActor::HasExpressionOverride);
		a_vm->BindNativeMethod(ACTOR, "UndressPartial"sv, Natives::FourActor::UndressPartial);
		a_vm->BindNativeMethod(ACTOR, "RedressPartial"sv, Natives::FourActor::RedressPartial);
		constexpr auto FURNITURE = "FourFurniture"sv;
		a_vm->BindNativeMethod(FURNITURE, "FindFurniture"sv, Natives::FourFurniture::FindFurniture);
		a_vm->BindNativeMethod(FURNITURE, "FindFurnitureOfType"sv, Natives::FourFurniture::FindFurnitureOfType);
		a_vm->BindNativeMethod(FURNITURE, "GetOffset"sv, Natives::FourFurniture::GetOffset);
		a_vm->BindNativeMethod(FURNITURE, "GetSceneID"sv, Natives::FourFurniture::GetSceneID);
		constexpr auto ACTORUTIL = "FourActorUtil"sv;
		a_vm->BindNativeMethod(ACTORUTIL, "FulfillsCondition"sv, Natives::FourActorUtil::FulfillsCondition);
		a_vm->BindNativeMethod(ACTORUTIL, "FulfillsAnyCondition"sv, Natives::FourActorUtil::FulfillsAnyCondition);
		a_vm->BindNativeMethod(ACTORUTIL, "FulfillsAllConditions"sv, Natives::FourActorUtil::FulfillsAllConditions);
		a_vm->BindNativeMethod(ACTORUTIL, "EmptyArray"sv, Natives::FourActorUtil::EmptyArray);
		a_vm->BindNativeMethod(ACTORUTIL, "CreateArray"sv, Natives::FourActorUtil::CreateArray);
		a_vm->BindNativeMethod(ACTORUTIL, "ToArray"sv, Natives::FourActorUtil::ToArray);
		a_vm->BindNativeMethod(ACTORUTIL, "Sort"sv, Natives::FourActorUtil::Sort);
		a_vm->BindNativeMethod(ACTORUTIL, "GetActorsInRangeV2"sv, Natives::FourActorUtil::GetActorsInRangeV2);
		a_vm->BindNativeMethod(ACTORUTIL, "ActorsToNames"sv, Natives::FourActorUtil::ActorsToNames);
		a_vm->BindNativeMethod("FourSequence"sv, "RandomSequence"sv, SQ_Random);
		constexpr auto MOD_EVENTS = "FourModEvents"sv;
		a_vm->BindNativeMethod(MOD_EVENTS, "RegisterForModEvent"sv, Natives::FourModEvents::Register);
		a_vm->BindNativeMethod(MOD_EVENTS, "UnregisterForModEvent"sv, Natives::FourModEvents::Unregister);
		a_vm->BindNativeMethod(MOD_EVENTS, "UnregisterForAllModEvents"sv, Natives::FourModEvents::UnregisterAll);
		a_vm->BindNativeMethod(MOD_EVENTS, "SendModEvent"sv, Natives::FourModEvents::Send);
		constexpr auto JSON = "FourJSON"sv;
		a_vm->BindNativeMethod(JSON, "GetActors"sv, Natives::FourJSON::GetActors);
		a_vm->BindNativeMethod(JSON, "GetScene"sv, Natives::FourJSON::GetScene);
		a_vm->BindNativeMethod(JSON, "GetMetadata"sv, Natives::FourJSON::GetMetadata);
		REX::INFO("Papyrus functions bound");
		return true;
	}

	// ---- Save data (F4SE co-save) ----
	// The scenes running when the game is saved: who, which scene, speed,
	// furniture, auto mode, excitement and what was taken off. On load they
	// start again (bResumeScenes=1), one by one once the loading screen is
	// gone; otherwise, or if someone's missing, everyone in them is let go
	// (unrestrained, interaction back, dressed again).

	constexpr std::uint32_t SAVE_UID = 'FSTM';
	constexpr std::uint32_t SCENES_RECORD = 'SCNS';
	constexpr std::uint32_t CHOICES_RECORD = 'EQCH';  // each actor's chosen equip objects (OStim saves them in its actor data)
	constexpr std::uint32_t CHOICES_VERSION = 1;
	constexpr std::uint32_t VOICES_RECORD = 'VOIC';  // voice sets chosen per actor base (OStim's Serialization voice sets)
	constexpr std::uint32_t VOICES_VERSION = 1;
	constexpr std::uint32_t STIM_RECORD = 'STIM';  // per actor base stimulation overrides (OStim's Serialization action / event stimulation)
	constexpr std::uint32_t STIM_VERSION = 1;
	constexpr std::uint32_t SCENES_VERSION = 6;  // 2: each actor's own scale (alignment scales them); 3: equip objects given; 4: wall spot, furniture blocked, clutter; 5: weapon taken off; 6: erect bodysuit given

	struct SavedScene
	{
		std::vector<std::uint32_t>              actors;  // role order
		std::string                             sceneID;
		std::int32_t                            speed = 0;
		std::uint32_t                           furnitureRef = 0;
		std::string                             furnitureType;
		bool                                    autoMode = false;
		std::vector<float>                      excitement;  // per actor
		std::vector<std::vector<std::uint32_t>> stripped;    // per actor
		std::vector<float>                      baseScale;   // per actor: their own scale (0 = unknown, version 1)
		std::vector<std::vector<std::uint32_t>> given;       // per actor: equip objects 4Stim gave them (version 3)
		std::optional<Furniture::Spot>          furnitureFixedSpot;  // a wall (version 4)
		bool                                    furnitureBlocked = false;  // 4Stim blocked the furniture (version 4)
		std::vector<Furniture::ClutterItem>     clutter;             // version 4
		std::vector<std::uint32_t>              weapons;             // per actor: the weapon taken off, 0 = none (version 5)
		std::vector<std::uint32_t>              suits;               // per actor: the erect bodysuit 4Stim gave, 0 = none (version 6)
	};
	std::mutex              g_savedLock;
	std::vector<SavedScene> g_loadedScenes;  // read from the save being loaded

	void WriteString(const F4SE::SerializationInterface* a_intfc, const std::string& a_text)
	{
		const auto length = static_cast<std::uint32_t>(a_text.size());
		a_intfc->WriteRecordData(length);
		a_intfc->WriteRecordData(a_text.data(), length);
	}

	bool ReadString(const F4SE::SerializationInterface* a_intfc, std::string& a_out)
	{
		std::uint32_t length = 0;
		if (a_intfc->ReadRecordData(length) != sizeof(length) || length > 4096) {
			return false;
		}
		a_out.resize(length);
		return a_intfc->ReadRecordData(a_out.data(), length) == length;
	}

	void WriteVoices(const F4SE::SerializationInterface* a_intfc);

	void WriteStimOverrides(const F4SE::SerializationInterface* a_intfc)
	{
		const auto entries = StimOverrides::All();
		if (entries.empty() || !a_intfc->OpenRecord(STIM_RECORD, STIM_VERSION)) {
			return;
		}
		a_intfc->WriteRecordData(static_cast<std::uint32_t>(entries.size()));
		for (const auto& e : entries) {
			a_intfc->WriteRecordData(e.kind);
			a_intfc->WriteRecordData(e.role);
			a_intfc->WriteRecordData(e.max);
			a_intfc->WriteRecordData(e.base);
			WriteString(a_intfc, e.id);
			a_intfc->WriteRecordData(e.value);
		}
	}

	void ReadStimOverrides(const F4SE::SerializationInterface* a_intfc)
	{
		std::uint32_t count = 0;
		a_intfc->ReadRecordData(count);
		std::vector<StimOverrides::Entry> entries;
		for (std::uint32_t i = 0; i < count && i < 100000; ++i) {
			StimOverrides::Entry e;
			if (!a_intfc->ReadRecordData(e.kind) || !a_intfc->ReadRecordData(e.role) || !a_intfc->ReadRecordData(e.max) ||
				!a_intfc->ReadRecordData(e.base) || !ReadString(a_intfc, e.id) || !a_intfc->ReadRecordData(e.value)) {
				break;
			}
			e.base = a_intfc->ResolveFormID(e.base).value_or(0);  // its plugin gone: dropped
			if (e.base) {
				entries.push_back(std::move(e));
			}
		}
		StimOverrides::Restore(entries);
		REX::INFO("Save data: {} stimulation override(s)", entries.size());
	}

	void OnGameSaved(const F4SE::SerializationInterface* a_intfc)
	{
		if (!a_intfc->OpenRecord(SCENES_RECORD, SCENES_VERSION)) {
			REX::WARN("Save data: couldn't write the scenes record");
			return;
		}
		const auto count = static_cast<std::uint32_t>(g_activeScenes.size());
		a_intfc->WriteRecordData(count);
		for (const auto& active : g_activeScenes) {
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(active.actors.size()));
			for (std::size_t role = 0; role < active.actors.size(); ++role) {
				const auto id = active.actors[role];
				a_intfc->WriteRecordData(id);
				a_intfc->WriteRecordData(role < active.baseScale.size() ? active.baseScale[role] : 1.0F);
				a_intfc->WriteRecordData(std::max(Excitement::Get(id), 0.0F));
				const auto items = Undress::Stripped(id);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(items.size()));
				for (const auto item : items) {
					a_intfc->WriteRecordData(item);
				}
				const auto given = EquipObjects::Given(id);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(given.size()));
				for (const auto item : given) {
					a_intfc->WriteRecordData(item);
				}
				a_intfc->WriteRecordData(SceneWeaponOf(id));  // version 5
				a_intfc->WriteRecordData(Erection::Given(id));  // version 6
			}
			WriteString(a_intfc, active.sceneID);
			a_intfc->WriteRecordData(static_cast<std::int32_t>(active.speed));
			a_intfc->WriteRecordData(active.furnitureRef);
			WriteString(a_intfc, active.furnitureType);
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(active.autoMode.on ? 1 : 0));
			// Version 4.
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(active.furnitureFixedSpot ? 1 : 0));
			if (active.furnitureFixedSpot) {
				const auto& spot = *active.furnitureFixedSpot;
				a_intfc->WriteRecordData(spot.position.x);
				a_intfc->WriteRecordData(spot.position.y);
				a_intfc->WriteRecordData(spot.position.z);
				a_intfc->WriteRecordData(spot.heading);
			}
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(FurnitureBlocked(active.furnitureRef) ? 1 : 0));
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(active.clutter.size()));
			for (const auto& item : active.clutter) {
				a_intfc->WriteRecordData(item.ref);
				a_intfc->WriteRecordData(item.position.x);
				a_intfc->WriteRecordData(item.position.y);
				a_intfc->WriteRecordData(item.position.z);
				a_intfc->WriteRecordData(item.angle.x);
				a_intfc->WriteRecordData(item.angle.y);
				a_intfc->WriteRecordData(item.angle.z);
			}
		}
		if (count > 0) {
			REX::INFO("Save data: {} running scene(s) saved", count);
		}
		// The equip objects chosen for actors (the HUD's options,
		// FourActor.SetEquipObjectID).
		const auto chosen = EquipObjects::AllChosen();
		if (!chosen.empty() && a_intfc->OpenRecord(CHOICES_RECORD, CHOICES_VERSION)) {
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(chosen.size()));
			for (const auto& [id, types] : chosen) {
				a_intfc->WriteRecordData(id);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(types.size()));
				for (const auto& [type, object] : types) {
					WriteString(a_intfc, type);
					WriteString(a_intfc, object);
				}
			}
		}
		WriteVoices(a_intfc);
		WriteStimOverrides(a_intfc);
	}

	// The VOICES_RECORD: actor base -> voice set key. A key that's a form ID
	// (OStim's: the voice set's target) is written as one, so a changed load
	// order still finds it; a name (4Stim's sets without a target) as text.
	void WriteVoices(const F4SE::SerializationInterface* a_intfc)
	{
		const auto voices = Sounds::AllVoiceChoices();
		if (voices.empty() || !a_intfc->OpenRecord(VOICES_RECORD, VOICES_VERSION)) {
			return;
		}
		a_intfc->WriteRecordData(static_cast<std::uint32_t>(voices.size()));
		for (const auto& [base, key] : voices) {
			a_intfc->WriteRecordData(base);
			std::uint32_t form = 0;
			bool          isForm = !key.empty() && std::ranges::all_of(key, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '-'; });
			if (isForm) {
				try {
					form = static_cast<std::uint32_t>(std::stol(key));
				} catch (...) {
					isForm = false;
				}
			}
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(isForm ? 1 : 0));
			if (isForm) {
				a_intfc->WriteRecordData(form);
			} else {
				WriteString(a_intfc, key);
			}
		}
	}

	void ReadVoices(const F4SE::SerializationInterface* a_intfc)
	{
		std::vector<std::pair<std::uint32_t, std::string>> voices;
		std::uint32_t count = 0;
		a_intfc->ReadRecordData(count);
		for (std::uint32_t i = 0; i < count && i < 65536; ++i) {
			std::uint32_t base = 0;
			std::uint8_t  isForm = 0;
			a_intfc->ReadRecordData(base);
			a_intfc->ReadRecordData(isForm);
			std::string key;
			if (isForm) {
				std::uint32_t form = 0;
				a_intfc->ReadRecordData(form);
				const auto resolved = a_intfc->ResolveFormID(form).value_or(0);
				key = resolved ? std::to_string(static_cast<std::int32_t>(resolved)) : std::string{};
			} else if (!ReadString(a_intfc, key)) {
				REX::WARN("Save data: the voice set choices are damaged, the rest skipped");
				break;
			}
			const auto resolvedBase = a_intfc->ResolveFormID(base).value_or(0);
			if (resolvedBase && !key.empty()) {
				voices.emplace_back(resolvedBase, std::move(key));
			}
		}
		REX::INFO("Save data: voice sets chosen for {} actor(s)", voices.size());
		Sounds::RestoreVoiceChoices(std::move(voices));
	}

	// The CHOICES_RECORD: actor -> type -> object id.
	void ReadChoices(const F4SE::SerializationInterface* a_intfc)
	{
		std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> chosen;
		std::uint32_t actors = 0;
		a_intfc->ReadRecordData(actors);
		for (std::uint32_t a = 0; a < actors && a < 65536; ++a) {
			std::uint32_t id = 0, types = 0;
			a_intfc->ReadRecordData(id);
			a_intfc->ReadRecordData(types);
			// OStim's per-sex pseudo IDs (0x0 men, 0x1 women) aren't forms.
			const auto resolved = id <= EquipObjects::FEMALE ? std::optional<std::uint32_t>(id) : a_intfc->ResolveFormID(id);
			for (std::uint32_t t = 0; t < types && t < 256; ++t) {
				std::string type, object;
				if (!ReadString(a_intfc, type) || !ReadString(a_intfc, object)) {
					REX::WARN("Save data: the equip object choices are damaged, the rest skipped");
					EquipObjects::RestoreChosen(std::move(chosen));
					return;
				}
				if (resolved) {
					chosen[*resolved][type] = object;
				}
			}
		}
		REX::INFO("Save data: equip objects chosen for {} actor(s)", chosen.size());
		EquipObjects::RestoreChosen(std::move(chosen));
	}

	void OnGameLoadedData(const F4SE::SerializationInterface* a_intfc)
	{
		std::vector<SavedScene> scenes;
		std::uint32_t type = 0, version = 0, length = 0;
		auto resolve = [&](std::uint32_t a_id) { return a_intfc->ResolveFormID(a_id).value_or(0); };
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type == CHOICES_RECORD && version == CHOICES_VERSION) {
				ReadChoices(a_intfc);
				continue;
			}
			if (type == VOICES_RECORD && version == VOICES_VERSION) {
				ReadVoices(a_intfc);
				continue;
			}
			if (type == STIM_RECORD && version == STIM_VERSION) {
				ReadStimOverrides(a_intfc);
				continue;
			}
			if (type != SCENES_RECORD || version < 1 || version > SCENES_VERSION) {
				continue;
			}
			std::uint32_t count = 0;
			a_intfc->ReadRecordData(count);
			for (std::uint32_t i = 0; i < count && i < 64; ++i) {
				SavedScene saved;
				std::uint32_t actors = 0;
				a_intfc->ReadRecordData(actors);
				bool ok = actors > 0 && actors <= 16;
				for (std::uint32_t a = 0; ok && a < actors; ++a) {
					std::uint32_t id = 0, items = 0;
					float         excitement = 0.0F, baseScale = 0.0F;
					a_intfc->ReadRecordData(id);
					if (version >= 2) {
						a_intfc->ReadRecordData(baseScale);
					}
					a_intfc->ReadRecordData(excitement);
					a_intfc->ReadRecordData(items);
					std::vector<std::uint32_t> stripped;
					for (std::uint32_t n = 0; n < items && n < 64; ++n) {
						std::uint32_t item = 0;
						a_intfc->ReadRecordData(item);
						if (const auto resolved = resolve(item)) {
							stripped.push_back(resolved);
						}
					}
					std::vector<std::uint32_t> given;
					if (version >= 3) {
						std::uint32_t givenCount = 0;
						a_intfc->ReadRecordData(givenCount);
						for (std::uint32_t n = 0; n < givenCount && n < 64; ++n) {
							std::uint32_t item = 0;
							a_intfc->ReadRecordData(item);
							if (const auto resolved = resolve(item)) {
								given.push_back(resolved);
							}
						}
					}
					std::uint32_t weapon = 0;
					if (version >= 5) {
						a_intfc->ReadRecordData(weapon);
						weapon = weapon ? resolve(weapon) : 0;
					}
					saved.weapons.push_back(weapon);
					std::uint32_t suit = 0;
					if (version >= 6) {
						a_intfc->ReadRecordData(suit);
						suit = suit ? resolve(suit) : 0;
					}
					saved.suits.push_back(suit);
					saved.given.push_back(std::move(given));
					saved.actors.push_back(resolve(id));  // 0 if its plugin is gone
					saved.excitement.push_back(excitement);
					saved.stripped.push_back(std::move(stripped));
					saved.baseScale.push_back(baseScale);
				}
				std::int32_t speed = 0;
				std::uint32_t furniture = 0;
				std::uint8_t  autoMode = 0;
				ok = ok && ReadString(a_intfc, saved.sceneID);
				a_intfc->ReadRecordData(speed);
				a_intfc->ReadRecordData(furniture);
				ok = ok && ReadString(a_intfc, saved.furnitureType);
				a_intfc->ReadRecordData(autoMode);
				if (ok && version >= 4) {
					std::uint8_t fixed = 0, blocked = 0;
					a_intfc->ReadRecordData(fixed);
					if (fixed) {
						Furniture::Spot spot;
						a_intfc->ReadRecordData(spot.position.x);
						a_intfc->ReadRecordData(spot.position.y);
						a_intfc->ReadRecordData(spot.position.z);
						a_intfc->ReadRecordData(spot.heading);
						saved.furnitureFixedSpot = spot;
					}
					a_intfc->ReadRecordData(blocked);
					saved.furnitureBlocked = blocked != 0;
					std::uint32_t items = 0;
					a_intfc->ReadRecordData(items);
					ok = items <= 4096;
					for (std::uint32_t n = 0; ok && n < items; ++n) {
						Furniture::ClutterItem item;
						a_intfc->ReadRecordData(item.ref);
						a_intfc->ReadRecordData(item.position.x);
						a_intfc->ReadRecordData(item.position.y);
						a_intfc->ReadRecordData(item.position.z);
						a_intfc->ReadRecordData(item.angle.x);
						a_intfc->ReadRecordData(item.angle.y);
						a_intfc->ReadRecordData(item.angle.z);
						if ((item.ref = resolve(item.ref)) != 0) {
							saved.clutter.push_back(item);
						}
					}
				}
				if (!ok) {
					REX::WARN("Save data: the scenes record is damaged, scenes from it skipped");
					break;
				}
				saved.speed = speed;
				saved.furnitureRef = furniture ? resolve(furniture) : 0;
				saved.autoMode = autoMode != 0;
				scenes.push_back(std::move(saved));
			}
		}
		if (!scenes.empty()) {
			REX::INFO("Save data: {} scene(s) were running in this save", scenes.size());
		}
		std::scoped_lock lock(g_savedLock);
		g_loadedScenes = std::move(scenes);
	}

	void OnGameReverted(const F4SE::SerializationInterface*)
	{
		EquipObjects::ClearChosen();  // before a save's are read, or a new game
		Sounds::ClearVoiceChoices();
		StimOverrides::Clear();
		g_positionBefore.clear();  // spots from the game being left (a furniture scene's player spot among them)
		std::scoped_lock lock(g_savedLock);
		g_loadedScenes.clear();
	}

	// Lets everyone in a saved scene go: unrestrained, interaction back, and
	// what was taken off back on. Main thread.
	void ReleaseSavedScene(const SavedScene& a_saved, const char* a_why)
	{
		REX::INFO("Save data: \"{}\" not started again ({}): letting its actors go", a_saved.sceneID, a_why);
		// Its furniture free again, its clutter back.
		if (a_saved.furnitureBlocked && a_saved.furnitureRef) {
			{
				std::scoped_lock lock(g_blockedFurnitureLock);
				g_blockedFurniture.insert(a_saved.furnitureRef);
			}
			UnblockFurniture(a_saved.furnitureRef);
		}
		if (g_settings.resetClutter && !a_saved.clutter.empty()) {
			Furniture::RestoreClutter(a_saved.clutter);
		}
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			const auto id = a_saved.actors[i];
			const auto actor = id ? RE::TESForm::GetFormByID<RE::Actor>(id) : nullptr;
			if (!actor) {
				continue;
			}
			if (a_saved.baseScale[i] > 0.0F) {
				CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, a_saved.baseScale[i]);  // undo the alignment's size
			}
			EquipObjects::TakeBack(id, a_saved.given[i]);  // before dressing again
			if (!a_saved.stripped[i].empty()) {
				Undress::NoteStripped(id, a_saved.stripped[i]);
				Undress::Redress(id, true);
			}
			if (i < a_saved.weapons.size() && a_saved.weapons[i] && g_vm) {
				NoteSceneWeapon(id, a_saved.weapons[i]);
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "RearmByID"sv, callback, static_cast<std::int32_t>(id));
			}
			if (g_vm) {
				// The player too (OStim's freeActor(byGameLoad) unlocks
				// everyone); their controls, camera and AI driving are
				// ResetPlayerSceneState's.
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "ReleaseAfterLoad"sv, callback, static_cast<std::int32_t>(id));
			}
		}
	}

	// Starts a saved scene again (or lets its actors go). Main thread.
	void ResumeSavedScene(const SavedScene& a_saved)
	{
		// Erections from before the save go first (LooksMenu keeps morphs in
		// its own save data, and a bodysuit 4Stim gave is still in the
		// inventory); the scene, if it starts again, puts them back.
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			if (a_saved.actors[i] && RE::TESForm::GetFormByID<RE::Actor>(a_saved.actors[i])) {
				Erection::AfterLoad(a_saved.actors[i], i < a_saved.suits.size() ? a_saved.suits[i] : 0);
			}
		}
		const auto scene = SceneRegistry::Find(a_saved.sceneID);
		bool       everyone = true;
		bool       free = true;
		for (const auto id : a_saved.actors) {
			const auto actor = id ? RE::TESForm::GetFormByID<RE::Actor>(id) : nullptr;
			everyone = everyone && actor && !actor->IsDead(false) && actor->Get3D();
			free = free && !FindActiveScene(id);
		}
		if (!g_settings.resumeScenes || !scene || !everyone || !free || !g_vm) {
			ReleaseSavedScene(a_saved, !g_settings.resumeScenes ? "bResumeScenes=0" : !scene ? "scene no longer loaded" :
			                           !everyone ? "an actor is missing" : !free ? "an actor is in another scene" : "no script engine");
			return;
		}
		auto sorted = a_saved.actors;
		std::ranges::sort(sorted);
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			if (!a_saved.stripped[i].empty()) {
				Undress::NoteStripped(a_saved.actors[i], a_saved.stripped[i]);
			}
			if (i < a_saved.weapons.size() && a_saved.weapons[i]) {
				NoteSceneWeapon(a_saved.actors[i], a_saved.weapons[i]);  // back on when the scene ends
			}
			EquipObjects::TakeBack(a_saved.actors[i], a_saved.given[i]);  // the scene puts them on again
			// Their own size back first: the scene measures it again when it starts.
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_saved.actors[i]);
			if (actor && a_saved.baseScale[i] > 0.0F) {
				CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, a_saved.baseScale[i]);
			}
		}
		{
			std::scoped_lock lock(g_pendingResumeLock);
			PendingResume resume{ sorted, a_saved.speed, a_saved.autoMode, {} };
			for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
				resume.excitement.emplace_back(a_saved.actors[i], a_saved.excitement[i]);
			}
			g_pendingResume.Put(sorted, std::move(resume));
		}
		if (a_saved.furnitureRef || a_saved.furnitureFixedSpot) {
			std::scoped_lock lock(g_pendingFurnitureLock);
			PendingFurniture pending{ a_saved.furnitureRef, a_saved.furnitureType, sorted };
			pending.offset = scene->furnitureOffset;
			pending.floorScene = scene->furniture.empty();
			pending.fixedSpot = a_saved.furnitureFixedSpot;
			g_pendingFurniture.Put(sorted, std::move(pending));
		}
		// Still blocked from before the save: 4Stim's to unblock when the
		// scene leaves it (FourStimFurniture.Block leaves blocked furniture be).
		if (a_saved.furnitureBlocked && a_saved.furnitureRef) {
			std::scoped_lock lock(g_blockedFurnitureLock);
			g_blockedFurniture.insert(a_saved.furnitureRef);
		}
		// The clutter as it lay before the scene, not as the scene left it.
		if (!a_saved.clutter.empty()) {
			std::scoped_lock lock(g_pendingClutterLock);
			g_pendingClutter.Put(sorted, PendingClutter{ sorted, a_saved.clutter, true });
		}
		const bool   withPlayer = std::ranges::any_of(a_saved.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		std::vector<std::int32_t> ids(a_saved.actors.begin(), a_saved.actors.end());
		const auto   anchor = withPlayer ? 0 : ids.front();
		REX::INFO("Save data: starting \"{}\" again ({} actor(s))", a_saved.sceneID, ids.size());
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "StartPickedCast"sv, callback, a_saved.sceneID, ids, anchor);
	}

	// After a save is loaded: once the loading screen and fade are gone, the
	// saved scenes one at a time (each takes the furniture slot and a few
	// seconds of Papyrus to set up).
	void ResumeSavedScenes()
	{
		std::vector<SavedScene> scenes;
		{
			std::scoped_lock lock(g_savedLock);
			scenes = std::move(g_loadedScenes);
			g_loadedScenes.clear();
		}
		if (scenes.empty()) {
			return;
		}
		// A second, then until the loading screen and fades are gone (checked
		// every 100 ms, a minute at most), 1.5 s more, then one scene every 4 s.
		Scheduler::After(std::chrono::seconds(1), [scenes = std::move(scenes)]() {
			Scheduler::WhileThenOnMain(std::chrono::milliseconds(100), 600, []() { return LoadingOrFading(); }, std::chrono::milliseconds(1500), [scenes]() {
				for (std::size_t i = 0; i < scenes.size(); ++i) {
					Scheduler::AfterOnMain(std::chrono::seconds(4) * static_cast<int>(i), [saved = scenes[i]]() { ResumeSavedScene(saved); });
				}
			});
		});
	}

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
				AnimEventHook::Install();
				static bool menuRegistered = false;
				if (!menuRegistered) {
					if (const auto ui = RE::UI::GetSingleton()) {
						ui->RegisterMenu(PICKER_MENU, PickerMenu::Create);
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
