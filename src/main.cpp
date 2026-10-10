#include <atomic>
#include <functional>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <fstream>
#include <unordered_map>

#include "Actions.h"
#include "Excitement.h"
#include "Undress.h"
#include "Alignment.h"
#include "Bridge.h"
#include "HUD.h"
#include "SceneEvents.h"
#include "Furniture.h"
#include "SceneRegistry.h"

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
	};
	std::unordered_map<std::uint32_t, GuardKicks> g_guardKicks;

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
		std::uint32_t hotkey = 0x4E;           // Windows virtual-key code; 0x4E = N
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
		float         npcSceneDuration = 300.0F; // NPCSceneDuration (OStim: ms): scenes without the player end after this many seconds (0 = never)
		std::uint32_t speedUpKey = 0xBB;       // virtual-key code; 0xBB = the =/+ key
		std::uint32_t speedDownKey = 0xBD;     // 0xBD = the -/_ key
		// OStim's other scene keys, unbound (0) by default: pull out, end the
		// scene, the search, the Align tab, the free camera, hide the HUD.
		std::uint32_t pullOutKey = 0;
		std::uint32_t endKey = 0;
		std::uint32_t searchKey = 0;
		std::uint32_t alignmentKey = 0;
		std::uint32_t freeCamKey = 0;
		std::uint32_t hideUIKey = 0;
		std::string   hudTheme = "Color";     // file name in Data\Interface\4Stim\Themes\, without .json
		bool          hudEnabled = true;
		bool          logAnimEvents = false;  // log scene actors' animation events (for authors)
		float         transitionLead = 0.0F;   // seconds before a transition's length to move on (see 4Stim.ini)
		bool          matchSex = true;         // only offer scenes whose roles' sexes the actors fit
		float         actorRadius = 1500.0F;     // how far from the player to list NPCs for a new scene
		float         furnitureRadius = 1600.0F; // how far from the player to look for furniture (OStim's SetFurnitureSearchDistance 15)
		float         furnitureHeight = 100.0F;  // and how far up or down
		bool          logFurniture = false;      // log every candidate object when looking
		std::uint32_t npcSceneKey = 0;           // opens the picker for an NPC-only scene, even in a scene (0 = Shift + the hotkey)
		bool          resumeScenes = false;      // scenes running in a save start again when it's loaded (OStim ends them)
	};
	Settings g_settings;

	// [AutoMode] in 4Stim.ini (see "Auto mode" below).
	struct AutoConfig
	{
		bool          player = false;        // scenes with the player start in auto mode
		bool          npc = true;            // scenes without the player do
		std::uint32_t key = 0;               // toggles it for the scene you're in / watching (0 = no key)
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

	void LoadSettings()
	{
		std::ifstream in("Data/F4SE/Plugins/4Stim.ini");
		if (!in) {
			REX::INFO("Settings: no 4Stim.ini, using defaults");
			return;
		}
		auto trim = [](std::string s) {
			const auto b = s.find_first_not_of(" \t\r");
			const auto e = s.find_last_not_of(" \t\r");
			return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
		};
		std::string line;
		while (std::getline(in, line)) {
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
				} else if (key == "buseintroscenes") {
					g_settings.useIntroScenes = std::stoi(value) != 0;
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
				} else if (key == "bmatchsex") {
					g_settings.matchSex = std::stoi(value) != 0;
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
				} else if (key == "bendnpcscenesonclimax") {
					Excitement::Settings().endNPCScenes = std::stoi(value) != 0;
				} else if (key == "fclimaxenddelay") {
					Excitement::Settings().endDelay = std::clamp(std::stof(value), 0.0F, 60.0F);
				} else if (key == "fclimaxshake") {
					Excitement::Settings().shake = std::clamp(std::stof(value), 0.0F, 5.0F);
				} else if (key == "fclimaxblur") {
					Excitement::Settings().blur = std::clamp(std::stof(value), 0.0F, 1.0F);
				} else if (key == "fclimaxflash") {
					Excitement::Settings().flash = std::clamp(std::stof(value), 0.0F, 1.0F);
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
				} else if (key == "bautomodeplayer") {
					g_autoConfig.player = std::stoi(value) != 0;
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
				} else if (key == "fautospeedexcitementmin") {
					g_autoConfig.speedExcitementMin = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "fautospeedexcitementmax") {
					g_autoConfig.speedExcitementMax = std::clamp(std::stof(value), 0.0F, 100.0F);
				} else if (key == "ftransitionlead") {
					g_settings.transitionLead = std::clamp(std::stof(value), -2.0F, 5.0F);
				}
			} catch (...) {
				REX::WARN("Settings: couldn't read \"{}\" for {}", value, key);
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
		REX::INFO("Settings: furniture within {} (height {}), log {}", g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.logFurniture ? "on" : "off");
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

	// ---- Sexes ----
	// Scenes say who may take each role ("sex" on each actor, SCENES.md);
	// with bMatchSex on, only scenes the actors fit are offered or played.

	std::vector<SceneRegistry::Sex> SexesOf(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<SceneRegistry::Sex> sexes;
		for (const auto id : a_ids) {
			sexes.push_back(SceneRegistry::SexOf(RE::TESForm::GetFormByID<RE::Actor>(id)));
		}
		return sexes;
	}

	// The filter for the picker's lists: these actors, roles open (a new
	// scene) or already given (a running one).
	SceneRegistry::ListFilter SexFilter(const std::vector<std::uint32_t>& a_ids, bool a_fixedOrder)
	{
		SceneRegistry::ListFilter filter;
		filter.fixedOrder = a_fixedOrder;
		if (g_settings.matchSex) {
			filter.sexes = SexesOf(a_ids);
		}
		return filter;
	}

	// Whether the actors of a running scene (role order) may play a_scene.
	bool SexesFit(const SceneRegistry::Scene& a_scene, const std::vector<std::uint32_t>& a_ids)
	{
		return !g_settings.matchSex || SceneRegistry::Fits(a_scene.actors, SexesOf(a_ids));
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

	// The furniture the next scene started for these actors goes on (set
	// when the picker starts a furniture scene).
	struct PendingFurniture
	{
		std::uint32_t              ref = 0;
		std::string                type;
		std::vector<std::uint32_t> actors;  // sorted
		std::array<float, 4>       offset{};  // the scene's own offset from the spot
	};
	std::mutex                      g_pendingFurnitureLock;
	std::optional<PendingFurniture> g_pendingFurniture;

	std::optional<PendingFurniture> PendingFurnitureFor(std::vector<std::uint32_t> a_actors, bool a_take)
	{
		std::ranges::sort(a_actors);
		std::scoped_lock lock(g_pendingFurnitureLock);
		if (!g_pendingFurniture || g_pendingFurniture->actors != a_actors) {
			return std::nullopt;
		}
		auto out = g_pendingFurniture;
		if (a_take) {
			g_pendingFurniture.reset();
		}
		return out;
	}

	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot);

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
		const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(pending->ref);
		if (!ref) {
			REX::WARN("Furniture: {:08X} is gone, the scene is played where the actors are", pending->ref);
			return false;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		auto       spot = Furniture::SpotFor(ref, pending->type, player ? player->GetPosition() : ref->GetPosition());
		if (const auto& o = pending->offset; o[0] != 0.0F || o[1] != 0.0F || o[2] != 0.0F || o[3] != 0.0F) {
			// In the spot's frame: x to its right, y forward.
			const float c = std::cos(spot.heading), s = std::sin(spot.heading);
			spot.position.x += o[0] * c + o[1] * s;
			spot.position.y += -o[0] * s + o[1] * c;
			spot.position.z += o[2];
			spot.heading += o[3] * PI_F / 180.0F;
			REX::INFO("Furniture: scene offset ({}, {}, {}, {} deg)", o[0], o[1], o[2], o[3]);
		}
		for (const auto actor : a_actors) {
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

		// Speed from before a transition, for its destination (the
		// transition itself has only one speed).
		int carrySpeed = 0;

		// A scene the player picked during a transition: played when the
		// transition ends, instead of its destination.
		std::string queuedScene;

		// The furniture it's played on (0 / "" = none).
		std::uint32_t furnitureRef = 0;
		std::string   furnitureType;

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
		};
		std::vector<AwaitingClimax> awaitingClimax;

		// When its idles were last played (the scene guard leaves it alone
		// for a moment after).
		std::chrono::steady_clock::time_point lastPlayed = std::chrono::steady_clock::now();
		// The scene's spot: where its animations are played around (every
		// actor of a paired animation shares it), and the heading they face.
		// Each actor is locked there for the whole scene (see "Scene lock").
		RE::NiPoint3                          center{};
		float                                 heading = 0.0F;  // radians
		std::chrono::steady_clock::time_point lastRelock{};
		std::vector<float>                    baseScale;     // per role: the actor's own scale when the scene started
		std::vector<float>                    appliedScale;  // per role: the scale the alignment last set (0 = none)
		std::string                           alignKey;      // OStim's actor set key for alignment (Alignment::KeyFor)

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
	};

	// Remembers the outgoing speed when a scene enters a transition.
	void NoteCarrySpeed(ActiveScene& a_active, const std::string& a_newSceneID)
	{
		const auto from = SceneRegistry::Find(a_active.sceneID);
		const auto to = SceneRegistry::Find(a_newSceneID);
		if (to && to->IsTransition() && !(from && from->IsTransition())) {
			a_active.carrySpeed = a_active.speed;
		}
	}
	std::vector<ActiveScene> g_activeScenes;

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
	std::optional<PendingSequence> g_pendingSequence;

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
	std::optional<PendingResume> g_pendingResume;

	void LockScene(ActiveScene& a_active);
	void UnlockActors(const std::vector<std::uint32_t>& a_ids);
	void RestoreScales(const ActiveScene& a_active);
	void MoveApart(const ActiveScene& a_ended);
	void ArmAutoplay(ActiveScene& a_scene);
	void StartAutoMode(ActiveScene& a_active);
	void StopAutoMode(ActiveScene& a_active);
	void AutoModeTick(ActiveScene& a_active, float a_seconds);
	bool IsPlayer(const RE::Actor* a_actor);
	void StartAutoplayTicks();
	void RefreshExcitement(const ActiveScene& a_scene);
	bool IsPlayer(const RE::Actor* a_actor);
	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed);

	ActiveScene* FindActiveScene(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		return it != g_activeScenes.end() ? &*it : nullptr;
	}

	void TrackSceneStart(std::vector<std::uint32_t> a_actors, const std::string& a_sceneID)
	{
		// An actor can only be in one scene: a new scene replaces an old one.
		std::erase_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			const bool replaced = std::ranges::any_of(a_scene.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
			if (replaced) {
				std::vector<std::uint32_t> left;
				std::ranges::copy_if(a_scene.actors, std::back_inserter(left), [&](std::uint32_t id) { return std::ranges::find(a_actors, id) == a_actors.end(); });
				Excitement::Leave(left);
				UnlockActors(left);
				RestoreScales(a_scene);
				for (const auto id : left) {
					Undress::Redress(id, false, true);
				}
			}
			return replaced;
		});
		ActiveScene scene{ a_actors, a_sceneID };
		if (const auto furniture = PendingFurnitureFor(a_actors, true)) {
			scene.furnitureRef = furniture->ref;
			scene.furnitureType = furniture->type;
			REX::INFO("Scene \"{}\": on furniture {:08X} ({})", a_sceneID, furniture->ref, furniture->type);
		}
		{
			std::scoped_lock lock(g_pendingSequenceLock);
			if (g_pendingSequence) {
				auto sorted = a_actors;
				auto wanted = g_pendingSequence->actors;
				std::ranges::sort(sorted);
				std::ranges::sort(wanted);
				const auto& first = g_pendingSequence->sequence->entries.front();
				if (sorted == wanted) {
					if (_stricmp(first.scene.c_str(), a_sceneID.c_str()) == 0) {
						scene.sequence = g_pendingSequence->sequence;
						REX::INFO("Sequence \"{}\": started ({} scene(s))", scene.sequence->id, scene.sequence->entries.size());
					}
					g_pendingSequence.reset();
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
		for (const auto id : a_actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (IsPlayer(actor)) {
				scene.center = actor->data.location;  // the player is never moved: the scene is built on their spot
				scene.heading = actor->data.angle.z;
			}
		}
		std::vector<Alignment::ActorInfo> alignActors;
		for (const auto id : a_actors) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			scene.baseScale.push_back(actor && actor->refScale ? actor->refScale / 100.0F : 1.0F);
			scene.appliedScale.push_back(0.0F);
			alignActors.push_back({ SceneRegistry::SexOf(actor), scene.baseScale.back(), 0.0F });  // no heels system in Fallout 4 yet
		}
		scene.alignKey = Alignment::KeyFor(alignActors);
		g_activeScenes.push_back(std::move(scene));
		LockScene(g_activeScenes.back());
		SceneEvents::SceneStarted(a_actors, a_sceneID);
		WatchAnimEvents(a_actors, a_sceneID);
		auto& added = g_activeScenes.back();
		RefreshExcitement(added);
		if (const auto first = SceneRegistry::Find(added.sceneID)) {
			Undress::SceneEntered(added.actors, *first, true);
		}
		if (added.sequence && added.sequence->entries.front().speed != 0) {
			PlayOnActiveScene(added, added.sequence->entries.front().scene, added.sequence->entries.front().speed);
		}
		ArmAutoplay(added);
		const bool withPlayer = std::ranges::any_of(added.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		if (!withPlayer && g_settings.npcSceneDuration > 0.0F) {
			added.stopTimer = g_settings.npcSceneDuration;
			StartAutoplayTicks();
		}
		if (withPlayer ? g_autoConfig.player : g_autoConfig.npc) {
			StartAutoMode(added);
		}
		// Started again from a save: back to where it was.
		std::optional<PendingResume> resume;
		{
			std::scoped_lock lock(g_pendingResumeLock);
			auto sorted = added.actors;
			std::ranges::sort(sorted);
			if (g_pendingResume && g_pendingResume->actors == sorted) {
				resume = std::move(g_pendingResume);
				g_pendingResume.reset();
			}
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
	}

	void TrackSceneStop(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		if (it == g_activeScenes.end()) {
			return;
		}
		const auto ended = std::move(*it);
		g_activeScenes.erase(it);
		UnlockActors(ended.actors);
		MoveApart(ended);
		RestoreScales(ended);
		Excitement::Leave(ended.actors);
		for (const auto id : ended.actors) {
			Undress::Redress(id, false, true);
		}
		SceneEvents::SceneEnded(ended.actors, ended.sceneID);
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
		if (moved) {
			NoteCarrySpeed(*scene, a_sceneID);
			scene->queuedScene.clear();
			WatchAnimEvents(scene->actors, a_sceneID);
		}
		scene->sceneID = a_sceneID;
		scene->speed = a_speed;
		scene->lastPlayed = std::chrono::steady_clock::now();
		RefreshExcitement(*scene);
		if (const auto entered = moved ? SceneRegistry::Find(a_sceneID) : nullptr) {
			Undress::SceneEntered(scene->actors, *entered, false);
			LockScene(*scene);  // this scene's alignment
		}
		if (moved) {
			if (scene->sequence) {
				REX::INFO("Sequence \"{}\": stopped, the scene was moved to \"{}\"", scene->sequence->id, a_sceneID);
				scene->sequence.reset();
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
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() != a_active.actors.size()) {
			REX::WARN("Autoplay: can't play \"{}\" on this scene's {} actor(s)", a_sceneID, a_active.actors.size());
			return false;
		}
		const int  speed = std::clamp(a_speed, 0, static_cast<int>(scene->speeds.size()) - 1);
		const auto& idles = scene->speeds[speed];
		std::string results;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role]);
			const auto process = actor ? actor->currentProcess : nullptr;
			const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
			results += std::format("{}{:08X} {}", role ? ", " : "", a_active.actors[role], ok ? "ok" : "refused");
		}
		REX::INFO("Autoplay: \"{}\" speed {}: PlayIdle {}", scene->id, speed + 1, results);
		WatchAnimEvents(a_active.actors, scene->id);
		const auto previous = a_active.sceneID;
		NoteCarrySpeed(a_active, scene->id);
		a_active.sceneID = scene->id;
		a_active.speed = speed;
		a_active.lastPlayed = std::chrono::steady_clock::now();
		RefreshExcitement(a_active);
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			Undress::SceneEntered(a_active.actors, *scene, false);
			LockScene(a_active);  // this scene's alignment
		}

		auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_active.actors[0]) {
			focused.sceneID = scene->id;
			focused.speed = speed;
			SetPlayerScene(focused);
		}
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			SceneEvents::SceneChanged(a_active.actors, previous, scene->id);
		}
		return true;
	}

	// a_active's time is up: on to the next scene.
	void AdvanceAutoplay(ActiveScene& a_active)
	{
		if (!a_active.queuedScene.empty()) {
			const auto queued = std::exchange(a_active.queuedScene, std::string{});
			if (a_active.sequence) {
				REX::INFO("Sequence \"{}\": stopped, the player picked \"{}\"", a_active.sequence->id, queued);
				a_active.sequence.reset();
			}
			REX::INFO("Transition \"{}\" -> \"{}\" (picked during the transition)", a_active.sceneID, queued);
			if (PlayOnActiveScene(a_active, queued, a_active.carrySpeed)) {
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
		if (PlayOnActiveScene(a_active, scene->destination, a_active.carrySpeed)) {
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
			g_vm->DispatchStaticCall("Debug"sv, "Notification"sv, callback, a_text);
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
			if (a_menOnly && SceneRegistry::SexOf(RE::TESForm::GetFormByID<RE::Actor>(id)) != SceneRegistry::Sex::kMale) {
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
		g_vm->DispatchStaticCall("FourStimMenu"sv, "FadeToBlack"sv, callback);
		std::thread([role0 = a_active.actors[0], target = a_target, route = a_route]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(700));
			F4SE::GetTaskInterface()->AddTask([role0, target, route]() {
				if (const auto active = FindActiveScene(role0)) {
					AutoGoToNow(*active, target, route);
				}
			});
			std::this_thread::sleep_for(std::chrono::milliseconds(550));
			if (g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> after;
				g_vm->DispatchStaticCall("FourStimMenu"sv, "FadeFromBlack"sv, after, 0.0F);
			}
		}).detach();
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
		sequence->actorCount = a_active.actors.size();
		for (std::size_t i = 0; i < a_route.size(); ++i) {
			const auto scene = SceneRegistry::Find(a_route[i]);
			if (!scene) {
				return 0.0F;
			}
			const bool last = i + 1 == a_route.size();
			const float duration = scene->IsTransition() && scene->length > 0.0F ? scene->length : last ? 0.1F : 0.5F;
			const int   speed = last ? finalSpeed : std::min(a_active.speed, static_cast<int>(scene->speeds.size()) - 1);
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
			const bool male = SceneRegistry::SexOf(RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[0])) == SceneRegistry::Sex::kMale;
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

		// Faster as excitement rises.
		if (g_autoConfig.autoSpeed && (state.speedCooldown -= a_seconds) <= 0.0F) {
			state.speedCooldown = AutoRandom(g_autoConfig.speedIntervalMin, g_autoConfig.speedIntervalMax);
			const float excitement = MaxExcitement(a_active, false);
			const float span = std::max(g_autoConfig.speedExcitementMax - g_autoConfig.speedExcitementMin, 1.0F);
			const int   chance = static_cast<int>(std::clamp((excitement - g_autoConfig.speedExcitementMin) * 100.0F / span, 0.0F, 100.0F));
			const int   count = static_cast<int>(scene->speeds.size());
			if (a_active.speed + 1 < count && AutoChance(chance) && PlayOnActiveScene(a_active, scene->id, a_active.speed + 1)) {
				SceneEvents::SpeedChanged(a_active.actors, scene->id, a_active.speed, count);
			}
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

	ActorSpot SpotOf(const ActiveScene& a_active, std::size_t a_role)
	{
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		const auto settled = scene ? SceneRegistry::Settled(scene) : nullptr;
		const auto id = settled ? settled->id : a_active.sceneID;
		const auto o = Alignment::Get(a_active.alignKey, id, a_role);
		// As OStim's alignActor: the role's offset from the scene file plus
		// the alignment.
		SceneRegistry::Position base;
		if (settled && a_role < settled->actors.size()) {
			base = settled->actors[a_role].offset;
		}
		const float x = o.x + base.x, y = o.y + base.y, z = o.z + base.z, r = o.rot + base.r;
		const float c = std::cos(a_active.heading), s = std::sin(a_active.heading);
		ActorSpot spot;
		// Facing heading h, forward is (sin h, cos h) and right is (cos h, -sin h).
		spot.position = { a_active.center.x + x * c + y * s, a_active.center.y - x * s + y * c, a_active.center.z + z };
		spot.heading = a_active.heading + r * PI_F / 180.0F;
		spot.scale = o.scale;
		return spot;
	}

	void LockScene(ActiveScene& a_active)
	{
		if (!g_vm) {
			return;
		}
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto id = a_active.actors[role];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			const auto spot = SpotOf(a_active, role);
			// Alignment scale, only when it changes.
			if (role < a_active.baseScale.size()) {
				const float wanted = a_active.baseScale[role] * spot.scale;
				const float current = a_active.appliedScale[role] > 0.0F ? a_active.appliedScale[role] : a_active.baseScale[role];
				if (std::fabs(wanted - current) > 0.001F) {
					CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, wanted);
					a_active.appliedScale[role] = wanted;
				}
			}
			if (!g_lockScenes) {
				continue;
			}
			FaceHeading(actor, spot.heading);  // face the right way first; the translation then holds it
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			g_vm->DispatchStaticCall("FourStimScene"sv, "LockInPlace"sv, callback, static_cast<std::int32_t>(id), spot.position.x, spot.position.y,
				spot.position.z, spot.heading * 180.0F / PI_F);
		}
	}

	// Actors leaving a scene: their own scale back.
	void RestoreScales(const ActiveScene& a_active)
	{
		for (std::size_t role = 0; role < a_active.actors.size() && role < a_active.appliedScale.size(); ++role) {
			if (a_active.appliedScale[role] > 0.0F) {
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role])) {
					CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, a_active.baseScale[role]);
				}
			}
		}
	}

	void UnlockActors(const std::vector<std::uint32_t>& a_ids)
	{
		if (!g_vm) {
			return;
		}
		for (const auto id : a_ids) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			g_vm->DispatchStaticCall("FourStimScene"sv, "Unlock"sv, callback, static_cast<std::int32_t>(id));
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
	// speed, OStim's setPosition). The player isn't moved, as the scene was
	// built on the player's own spot. That also takes them out of each other
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
			if (!actor || IsPlayer(actor)) {
				continue;
			}
			if (before != g_positionBefore.end()) {
				if (g_resetPosition) {
					const auto& b = before->second;
					REX::INFO("Scene end: {:08X} back to where they stood before the scene", id);
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					g_vm->DispatchStaticCall("FourStimScene"sv, "ResetPosition"sv, callback, static_cast<std::int32_t>(id), b.position.x, b.position.y,
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
				g_vm->DispatchStaticCall("FourStimScene"sv, "MoveApart"sv, callback, static_cast<std::int32_t>(actor->GetFormID()), to.x, to.y, to.z);
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
			a_actor->StopInteractingQuick(true, false, true);
		}
		if (a_doNothing || furniture) {
			a_actor->InitiateDoNothingPackage();
		}
		return furniture != nullptr;
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
			for (const auto id : active.actors) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				if (!actor || IsPlayer(actor)) {
					continue;
				}
				const auto process = actor->currentProcess;
				const auto data = process ? process->middleHigh : nullptr;
				if (!data) {
					continue;
				}
				data->packageIdleTimer = 30.0F;  // no package idle for a while; renewed every check
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
				if (auto& kicks = g_guardKicks[id]; kicks.count < 3 && now - kicks.last > std::chrono::seconds(5) && FurnitureOf(actor)) {
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
		std::thread([generation]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(2500));
			F4SE::GetTaskInterface()->AddTask([generation]() {
				if (g_slowMoGeneration == generation) {
					EndSlowMotion();
				}
			});
		}).detach();
	}

	// The player's climax effects: shake, blur, glow, rumble, slow motion
	// (Excitement::Config). Slow motion only in scenes with the player, as
	// OStim's (its player thread); the rest also in a scene being watched.
	void PlayClimaxEffects(bool a_withPlayer)
	{
		const auto& config = Excitement::Settings();
		const bool  freeCamera = FreeCamera() != nullptr;
		if (config.slowMo && a_withPlayer) {
			StartSlowMotion();
		}
		if (config.shake > 0.0F && freeCamera) {
			if (g_climaxShake.active) {
				// Already shaking: restart it from where the camera is now.
				g_climaxShake.start = std::chrono::steady_clock::now();
			} else {
				g_climaxShake = { true, std::chrono::steady_clock::now(), 0.8F, config.shake };
			}
			StartAutoplayTicks();
		}
		if (config.blurOn && config.blur > 0.0F) {
			// Depth of field with no in-focus range: everything blurs, for
			// a moment.
			RE::ImageSpaceModifierInstanceDOF::Trigger(0.0F, 0.0F, 0.0F, 0.0F,
				RE::ImageSpaceModifierInstanceDOF::DepthOfFieldMode::kFrontBack, std::min(config.blur, 1.0F), 0.5F);
		}
		if (config.flash > 0.0F) {
			HUD::PlayClimax(std::min(config.flash, 1.0F));
		}
		// The normal cameras' shake and the rumble go through Papyrus.
		const bool papyrusShake = config.shake > 0.0F && !freeCamera;
		if ((papyrusShake || config.rumble) && g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			g_vm->DispatchStaticCall("FourStimMenu"sv, "ClimaxEffects"sv, callback, papyrusShake ? config.shake : 0.0F, config.rumble);
		}
	}

	// Hands a running scene's current scene and speed to the excitement
	// system (rates come from its actions), and keeps the clock running.
	void RefreshExcitement(const ActiveScene& a_scene)
	{
		const auto scene = SceneRegistry::Find(a_scene.sceneID);
		if (!scene) {
			return;
		}
		std::vector<SceneRegistry::Sex> sexes;
		for (const auto id : a_scene.actors) {
			sexes.push_back(SceneRegistry::SexOf(RE::TESForm::GetFormByID<RE::Actor>(id)));
		}
		Excitement::Enter(a_scene.actors, sexes, *scene, a_scene.speed);
		if (Excitement::Settings().enabled) {
			StartAutoplayTicks();
		}
	}

	// Ends a running scene through Papyrus: the focused scene the way the
	// HUD's "End scene" does, any other without touching the camera.
	void EndActiveScene(const ActiveScene& a_scene, std::string_view a_why)
	{
		if (!g_vm || a_scene.actors.empty()) {
			return;
		}
		REX::INFO("Scene \"{}\": ending, {}", a_scene.sceneID, a_why);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_scene.actors[0]) {
			g_vm->DispatchStaticCall("FourStimMenu"sv, "EndPlayerScene"sv, callback);
		} else {
			std::vector<std::int32_t> ids;
			for (const auto id : a_scene.actors) {
				ids.push_back(static_cast<std::int32_t>(id));
			}
			g_vm->DispatchStaticCall("FourStimMenu"sv, "EndSceneOf"sv, callback, ids);
		}
	}

	// a_actorID, in a_active, climaxes now (OStim's climaxInner): counts it,
	// a man drops to the slowest speed, other mods are told, the player sees
	// the effects, and the scene ends if the settings say so.
	void ClimaxNow(ActiveScene& a_active, std::uint32_t a_actorID)
	{
		const auto& config = Excitement::Settings();
		std::erase_if(a_active.awaitingClimax, [&](const auto& a_entry) { return a_entry.actor == a_actorID; });
		Excitement::Climaxed(a_actorID);
		if (a_active.autoMode.on && a_active.autoMode.stage == ActiveScene::Auto::Stage::kPullout) {
			a_active.autoMode.stage = ActiveScene::Auto::Stage::kMain;
			a_active.autoMode.cooldown = std::min(a_active.autoMode.cooldown, 3.0F);
		}
		const int  times = Excitement::TimesClimaxed(a_actorID);
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actorID);
		const auto sex = SceneRegistry::SexOf(actor);
		const auto role = static_cast<std::size_t>(std::ranges::find(a_active.actors, a_actorID) - a_active.actors.begin());
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		REX::INFO("Climax: {:08X} (role {}) in \"{}\", climax {}", a_actorID, role, a_active.sceneID, times);

		// As OStim: a man's climax drops the scene to its slowest speed (still
		// in the climax animation: the scene it goes back to starts slowest).
		if (sex == SceneRegistry::Sex::kMale && scene) {
			if (scene->IsTransition()) {
				a_active.carrySpeed = 0;
			} else if (a_active.speed > 0 && PlayOnActiveScene(a_active, a_active.sceneID, 0)) {
				SceneEvents::SpeedChanged(a_active.actors, a_active.sceneID, 0, static_cast<int>(scene->speeds.size()));
			}
		}

		SceneEvents::Climaxed(a_actorID, a_active.actors, a_active.sceneID, times);

		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		const auto focused = GetPlayerScene();
		const bool watched = focused.Active() && focused.role0 == a_active.actors[0];
		if (withPlayer || watched) {
			PlayClimaxEffects(withPlayer);
		}

		bool end = false;
		if (!withPlayer) {
			end = config.endNPCScenes;
		} else if (config.endOnAll) {
			end = std::ranges::all_of(a_active.actors, [](std::uint32_t id) { return Excitement::TimesClimaxed(id) > 0; });
		} else {
			end = (IsPlayer(actor) && config.endOnPlayer) ||
			      (sex == SceneRegistry::Sex::kMale && config.endOnMale) ||
			      (sex == SceneRegistry::Sex::kFemale && config.endOnFemale);
		}
		if (end && a_active.endIn < 0.0F) {
			a_active.endIn = config.endDelay;
		}
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
			PlayOnActiveScene(a_active, climaxScene, 0)) {
			if (SceneRegistry::SexOf(RE::TESForm::GetFormByID<RE::Actor>(a_actorID)) == SceneRegistry::Sex::kMale) {
				a_active.carrySpeed = 0;  // back at the slowest speed after it
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
		for (const auto& entry : a_active.awaitingClimax) {
			if (entry.annotated || _stricmp(entry.sceneID.c_str(), a_active.sceneID.c_str()) != 0) {
				due.push_back(entry.actor);
			}
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
			for (const auto id : Excitement::Tick(elapsed)) {
				if (const auto active = FindActiveScene(id)) {
					HandleClimax(*active, id);
				} else {
					Excitement::Climaxed(id);
				}
			}
			for (auto& active : g_activeScenes) {
				ResolveAwaitingClimaxes(active);
			}
			UpdateClimaxShake();
			GuardScenes(elapsed);
			Undress::Tick(elapsed);
			for (auto& active : g_activeScenes) {
				AutoModeTick(active, elapsed);
			}
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
		const bool waiting = std::ranges::any_of(g_activeScenes, [](const ActiveScene& a_scene) {
			return a_scene.remaining >= 0.0F || a_scene.endIn >= 0.0F || a_scene.stopTimer >= 0.0F || !a_scene.awaitingClimax.empty() || a_scene.autoMode.on ||
			       Excitement::Settings().enabled;
		});
		if (!waiting && !g_climaxShake.active) {
			g_autoplayTicking = false;
		}
	}

	// Main thread. Starts the ticker thread if it isn't running.
	void StartAutoplayTicks()
	{
		if (g_autoplayTicking.exchange(true)) {
			return;
		}
		g_autoplayLast = std::chrono::steady_clock::now();
		std::thread([]() {
			while (g_autoplayTicking) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				if (g_autoplayTicking && !g_autoplayTickQueued.exchange(true)) {
					F4SE::GetTaskInterface()->AddTask(AutoplayTick);
				}
			}
		}).detach();
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
		ArmAutoplay(*active);
		return true;
	}

	// Queues a_sequence for the next scene these actors start (its first
	// scene), from any thread.
	void QueueSequence(std::shared_ptr<const SceneRegistry::Sequence> a_sequence, std::vector<std::uint32_t> a_actors)
	{
		std::scoped_lock lock(g_pendingSequenceLock);
		g_pendingSequence = PendingSequence{ std::move(a_sequence), std::move(a_actors) };
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
		return g_vm->DispatchMethodCall(
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
	void StartScene(std::monostate, RE::Actor* a_actor, RE::Actor*, std::string a_sceneID)
	{
		if (!a_actor) {
			REX::WARN("StartScene called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene) {
			REX::WARN("StartScene: no scene \"{}\" (check the Scenes folder and this log's load summary)", a_sceneID);
			g_focusNextScene = false;
			return;
		}
		const auto idle = scene->speeds[0][0];
		const auto sceneID = scene->id;

		REX::INFO("StartScene: {:08X} <- scene \"{}\" (idle {:08X})", a_actor->GetFormID(), scene->id, idle->GetFormID());
		PlaceOnPendingFurniture({ a_actor });

		// Papyrus calls arrive on a script thread; run engine animation code on the main thread.
		F4SE::GetTaskInterface()->AddTask([a_actor, idle, sceneID]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StartScene: actor has no AI process");
				return;
			}
			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StartScene: PlayIdle returned {}", ok);
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor);
			if (ok) {
				TrackSceneStart({ a_actor->GetFormID() }, sceneID);
				if (focus) {
					PlayerScene focused{ a_actor->GetFormID(), 0, sceneID, 0 };
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
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (actor && !IsPlayer(actor) && LeaveFurniture(actor, true)) {
				REX::INFO("SuppressInteraction: {:08X} was in furniture or an idle marker, taken out", id);
			}
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

	// Original filter per actor, restored at scene end. Not save-persisted:
	// character controllers are rebuilt on load anyway.
	std::unordered_map<std::uint32_t, std::uint32_t> g_savedFilters;

	RE::bhkCharacterController* GetCharController(RE::Actor* a_actor)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		const auto middleHigh = process ? process->middleHigh : nullptr;
		return middleHigh ? middleHigh->charController.get() : nullptr;
	}

	bool ApplyFilter(RE::Actor* a_actor, std::uint32_t a_filter)
	{
		const auto controller = GetCharController(a_actor);
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
			const auto c0 = GetCharController(a_actor0);
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
		std::thread([a_actor]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(700));
			F4SE::GetTaskInterface()->AddTask([a_actor]() { RestoreCollisionNow(a_actor); });
		}).detach();
	}

	void RestoreCollisionNow(RE::Actor* a_actor)
	{
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
			g_focusNextScene = false;
			return;
		}
		const auto idle0 = scene->speeds[0][0];
		const auto idle1 = scene->speeds[0][1];
		const auto sceneID = scene->id;

		REX::INFO("PlaySceneIdles: scene \"{}\": {:08X} <- {:08X}, {:08X} <- {:08X}",
			scene->id, a_actor0->GetFormID(), idle0->GetFormID(), a_actor1->GetFormID(), idle1->GetFormID());

		F4SE::GetTaskInterface()->AddTask([a_actor0, idle0, a_actor1, idle1, sceneID]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			if (!p0 || !p1) {
				REX::WARN("PlaySceneIdles: an actor has no AI process");
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
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor0) || IsPlayer(a_actor1);
			if (!ok0 && !ok1) {
				return;
			}
			TrackSceneStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, sceneID);
			if (focus) {
				PlayerScene focused{ a_actor0->GetFormID(), a_actor1->GetFormID(), sceneID, 0 };
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
		const auto type = a_type.empty() ? nullptr : Actions::Find(a_type);
		if (!a_type.empty() && !type) {
			return -1;
		}
		for (std::size_t i = 0; i < scene->actions.size(); ++i) {
			const auto& action = scene->actions[i];
			if ((!type || action.type->id == type->id) &&
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

	void Climax(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			Excitement::SetStalled(a_actor->GetFormID(), false);
			Excitement::Set(a_actor->GetFormID(), 100.0F);
		}
	}

	void StallClimax(std::monostate, RE::Actor* a_actor, bool a_stall)
	{
		if (a_actor) {
			Excitement::SetStalled(a_actor->GetFormID(), a_stall);
		}
	}

	bool IsClimaxStalled(std::monostate, RE::Actor* a_actor)
	{
		return a_actor && Excitement::IsStalled(a_actor->GetFormID());
	}

	float GetExcitementMultiplier(std::monostate, RE::Actor* a_actor)
	{
		return a_actor ? Excitement::Multiplier(a_actor->GetFormID()) : 1.0F;
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
			if (const auto active = FindActiveScene(id); active && PlayOnActiveScene(*active, dest, 0)) {
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
		const auto count = SceneRegistry::Reload();
		Alignment::Reload();
		HUD::LoadConfig();  // theme and Utility entries too, for HUD authors
		return count;
	}

	bool RegisterForSceneEvents(std::monostate, RE::TESForm* a_receiver)
	{
		return SceneEvents::Register(a_receiver);
	}

	void UnregisterForSceneEvents(std::monostate, RE::TESForm* a_receiver)
	{
		SceneEvents::Unregister(a_receiver);
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
			std::uint32_t group = 0;
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
			g_focusNextScene = false;
			return;
		}
		const auto idles = scene->speeds[0];
		const auto sceneID = scene->id;
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		F4SE::GetTaskInterface()->AddTask([ids, idles, sceneID]() {
			bool        any = false;
			bool        withPlayer = false;
			std::string results;
			for (std::size_t role = 0; role < ids.size(); ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
				any = any || ok;
				withPlayer = withPlayer || IsPlayer(actor);
				results += std::format("{}{:08X} {}", role ? ", " : "", ids[role], ok ? "ok" : "refused");
			}
			REX::INFO("PlayGroupIdles: \"{}\": {}", sceneID, results);
			const bool focus = g_focusNextScene.exchange(false) || withPlayer;
			if (!any) {
				return;
			}
			TrackSceneStart(ids, sceneID);
			if (focus) {
				auto focused = FocusFor(ids, sceneID);
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
		std::thread([task = std::move(a_task)]() mutable {
			for (int i = 0; i < 400 && LoadingOrFading(); ++i) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			// A little longer: the game finishes settling after the menu closes.
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			F4SE::GetTaskInterface()->AddTask(std::move(task));
		}).detach();
	}

	void BeginSceneCamera(std::monostate)
	{
		AfterLoading([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (!camera || g_sceneCameraActive) {
				return;
			}
			if (!g_startViewSaved) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
			}
			g_sceneCameraActive = true;
			SetHUDVisible(false);
			SetSceneFOV();
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
				// Back to the view the player started in, or first person with
				// OStim's SetForceFirstPerson.
				const bool firstPerson = g_startedFirstPerson || g_settings.forceFirstPerson;
				const auto restore = firstPerson ? RE::CameraState::kFirstPerson : RE::CameraState::k3rdPerson;
				if (!camera->QCameraEquals(restore)) {
					camera->SetState(camera->cameraStates[restore].get());
				}
			}
			RestoreFreeCameraSpeed();
			RestoreFOV();
			SetHUDVisible(true);
			REX::INFO("Scene camera: restored ({})", g_startedFirstPerson || g_settings.forceFirstPerson ? "first person" : "third person");
		});
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
			const auto ids = next.ActorIDs();
			for (std::size_t role = 0; role < idles.size() && role < ids.size(); ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				if (process) {
					process->PlayIdle(*actor, idles[role], nullptr);
				}
			}
			SetPlayerScene(next);
			REX::INFO("Player scene: \"{}\" speed {}", next.sceneID, next.speed + 1);
			TrackSceneChange(next.role0, next.sceneID, next.speed);
			if (next.sceneID != previous.sceneID) {
				SceneEvents::SceneChanged(next.ActorIDs(), previous.sceneID, next.sceneID);
			} else if (next.speed != previous.speed) {
				SceneEvents::SpeedChanged(next.ActorIDs(), next.sceneID, next.speed, count);
			}
		});
	}

	// Changes the player's scene speed by a_delta. Returns the new speed
	// (1-based), or 0 if the player isn't in a scene.
	std::int32_t ChangeSceneSpeedImpl(std::int32_t a_delta)
	{
		const auto current = GetPlayerScene();
		const auto scene = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		if (!scene) {
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
	bool NavigateSceneImpl(const std::string& a_destination)
	{
		const auto current = GetPlayerScene();
		const auto from = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		const auto to = SceneRegistry::Find(a_destination);
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
		const int speed = std::min(current.speed, static_cast<int>(to->speeds.size()) - 1);
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
			Notify("4Stim: you're in a scene; end it to watch another");
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
			Notify("4Stim: that scene has ended");
			return;
		}
		if (a_command == "@watch") {
			WatchScene(a_role0);
		} else if (a_command == "@auto") {
			active->autoMode.on ? StopAutoMode(*active) : StartAutoMode(*active);
			Notify(active->autoMode.on ? "Auto mode on" : "Auto mode off");
		} else if (a_command == "@endscene") {
			EndActiveScene(*active, "ended from the picker");
		}
	}

	enum class PickerMode
	{
		kStart,     // start a new scene: who, where, then an idle (see StartStep)
		kNavigate,  // the focused scene's navigation links, plus "End scene" (used when the HUD isn't available)
		kSearch     // every scene with the focused scene's actor count; picking one moves the scene there
	};
	std::atomic<PickerMode>    g_pickerMode = PickerMode::kStart;

	// ---- Starting a scene: who, where, then an idle (like OStim) ----
	// kStart walks through steps in the same picker list: the actors (the
	// player plus any nearby NPCs picked, one at a time), then where (right
	// here or a piece of furniture near), and then starts a random "idle"
	// scene that fits them; or the full list of scenes that fit, to browse.
	enum class StartStep
	{
		kActors,
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
	std::size_t                g_maxCast = 2;       // the most actors any scene has

	// The player (unless left out), then the picked NPCs.
	std::vector<std::uint32_t> CastIDs()
	{
		std::vector<std::uint32_t> ids;
		std::scoped_lock lock(g_castLock);
		if (g_includePlayer) {
			ids.push_back(RE::PlayerCharacter::GetSingleton()->GetFormID());
		}
		ids.insert(ids.end(), g_cast.begin(), g_cast.end());
		return ids;
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
		const auto tes = RE::TES::GetSingleton();
		if (!player || !tes) {
			return out;
		}
		const auto center = player->GetPosition();
		tes->ForEachReferenceInRange(center, a_radius, [&](RE::TESObjectREFR* a_ref) {
			const auto actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
			if (!actor || actor == player || actor->GetDelete() || (actor->formFlags & 0x800) != 0 || !actor->Get3D()) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			// Same rules as FourStimScene.CanUseActor; children never.
			if (actor->IsChild() || actor->IsDead(false) || actor->IsInCombat() || FindActiveScene(actor->GetFormID())) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto npc = actor->GetNPC();
			const bool human = (actor->race && actor->race->HasKeywordString("ActorTypeNPC")) || (npc && npc->HasKeywordString("ActorTypeNPC"));
			if (!human) {
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
			// Send the list right away rather than waiting for the menu to ask
			// (its RequestScenes call remains as a backup).
			SendScenes();
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
					if (!HandleStartPick(id)) {
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
			const auto mode = g_pickerMode.load();
			if (mode == PickerMode::kNavigate) {
				SendNavigation();
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
				seq.name = "Sequence: " + seq.name;
				seq.tags = seq.tags.empty() ? "sequence" : "sequence, " + seq.tags;
			}
			std::ranges::move(SceneRegistry::List(actorCount, filter), std::back_inserter(scenes));

			// Furniture: a running scene stays on what it's on (or off
			// furniture); a new one goes where the cast was placed.
			const auto here = search ? GetPlayerScene().furniture : ChosenFurnitureType();
			const auto found = search ? std::vector<Furniture::Found>{} : PickerFurniture();
			std::erase_if(scenes, [&](SceneRegistry::SceneSummary& a_scene) {
				if (!FurnitureFits(a_scene.furniture, here)) {
					return true;
				}
				const auto full = a_scene.id.starts_with(SEQUENCE_PREFIX) ? nullptr : SceneRegistry::Find(a_scene.id);
				return full && !SceneFitsFurniture(*full, here);
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

			const std::string title = search ? "Change scene" : solo ? "Solo scenes" : std::format("Scenes for {}", actorCount);
			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = title.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
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
				std::string who = withMe ? "You" : "";
				for (const auto id : picked) {
					const auto it = std::ranges::find_if(candidates, [&](const Candidate& c) { return c.id == id; });
					who += (who.empty() ? "" : ", ") + (it != candidates.end() ? withSex(*it) : std::format("{:08X}", id));
				}
				const std::size_t castSize = picked.size() + (withMe ? 1 : 0);
				if (castSize == 0) {
					rows.push_back({ "@none", "Pick someone first", "a scene without you needs at least one NPC" });
				} else {
					rows.push_back({ "@go", castSize == 1 && withMe ? "Continue alone" : std::format("Continue with {}", castSize), who });
				}
				if (busy) {
					rows.push_back({ "@none", "You: not in it (NPCs only)", "you're in a scene already" });
				} else {
					rows.push_back({ "@me", withMe ? "You: in the scene" : "You: not in it (NPCs only)", "choose to switch" });
				}
				for (const auto& c : candidates) {
					if (std::ranges::find(picked, c.id) != picked.end()) {
						rows.push_back({ std::format("@drop:{:X}", c.id), "[x] " + withSex(c), std::format("picked, {:.0f} m away: choose to remove", c.distance / 70.0F) });
					}
				}
				if (running > 0) {
					rows.push_back({ "@running", std::format("Running scenes ({})...", running), "watch, end or auto-play a scene" });
				}
				if (castSize < maxCast) {
					for (const auto& c : candidates) {
						if (std::ranges::find(picked, c.id) == picked.end()) {
							rows.push_back({ std::format("@add:{:X}", c.id), withSex(c), std::format("{:.0f} m away", c.distance / 70.0F) });
						}
					}
				}
				title = picked.empty() ? "Who's in the scene?" : "Add someone else?";
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
						rows.push_back({ "@watch", "Watch", "follow it with the free camera and the HUD" });
					}
					rows.push_back({ "@auto", it->autoMode ? "Auto mode: on" : "Auto mode: off", "choose to switch" });
					rows.push_back({ "@endscene", "End scene", "" });
					rows.push_back({ "@running", "Back", "all running scenes" });
					title = it->label;
				} else {
					for (const auto& r : running) {
						rows.push_back({ std::format("@scene:{:X}", r.actors.front()), r.label,
							std::string(r.withPlayer ? "you're in it" : "NPCs") + (r.autoMode ? ", auto mode" : "") });
					}
					rows.push_back({ "@back", "Back", "start a scene" });
					title = "Running scenes";
				}
			} else {
				// Where: right here, or a piece of furniture near, if any
				// scene fits this cast there.
				if (!ScenesForCast(cast, {}).empty()) {
					const bool withMe = std::ranges::find(cast, RE::PlayerCharacter::GetSingleton()->GetFormID()) != cast.end();
					rows.push_back({ "@here", withMe ? "Right here" : "Where they are", "no furniture" });
				}
				const auto found = PickerFurniture();
				for (std::size_t i = 0; i < found.size(); ++i) {
					if (!ScenesForCast(cast, found[i].type).empty()) {
						rows.push_back({ std::format("@furn:{}", i), found[i].name, std::format("{:.0f} m away", found[i].distance / 70.0F) });
					}
				}
				if (rows.empty()) {
					rows.push_back({ "@back", "No scenes for this cast", "choose to go back" });
				}
				rows.push_back({ "@browse", "Browse all scenes...", std::format("for {} actor(s)", cast.size()) });
				rows.push_back({ "@back", "Back", "change who's in it" });
				title = "Where?";
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
				g_startStep = StartStep::kPlace;
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

		// The scene a new scene starts with, as OStim's thread starters pick
		// it: a random scene that fits the cast and the place (no transitions,
		// nothing marked noRandomSelection). With the player in it, one tagged
		// "intro" (SetUseIntroScenes) or else "idle"; without the player, any
		// (OStim's NPC threads). Off furniture it must have someone standing,
		// on a bed no one standing. Departure from OStim, which gives up when
		// nothing qualifies: 4Stim falls back step by step (intro -> idle ->
		// any scene, then without the standing rule), since most converted
		// packs have no intro scenes yet.
		static void StartIdleScene()
		{
			const auto cast = CastIDs();
			const auto here = ChosenFurnitureType();
			auto       scenes = ScenesForCast(cast, here);
			std::erase_if(scenes, [](const auto& a_scene) { return a_scene->IsTransition() || a_scene->noRandomSelection; });
			bool withPlayer;
			{
				std::scoped_lock lock(g_castLock);
				withPlayer = g_includePlayer;
			}
			const bool onBed = !here.empty() && Furniture::IsA(here, "bed");
			auto standingRule = [&](const SceneRegistry::Scene& a_scene) {
				const bool standing = std::ranges::any_of(a_scene.actors, [](const SceneRegistry::SceneActor& a) { return a.HasTag("standing"); });
				return here.empty() ? standing : onBed ? !standing : true;
			};
			std::vector<std::string> tags;
			if (withPlayer) {
				if (g_settings.useIntroScenes) {
					tags.emplace_back("intro");
				}
				tags.emplace_back("idle");
			}
			tags.emplace_back("");  // any scene
			std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
			std::string                                              picked;
			for (const bool rule : { true, false }) {
				for (const auto& tag : tags) {
					pool.clear();
					std::ranges::copy_if(scenes, std::back_inserter(pool), [&](const auto& a_scene) {
						return (tag.empty() || HasTag(*a_scene, tag)) && (!rule || standingRule(*a_scene));
					});
					if (!pool.empty()) {
						picked = (tag.empty() ? std::string("any") : tag) + (rule ? "" : ", without the standing rule");
						break;
					}
				}
				if (!pool.empty()) {
					break;
				}
			}
			if (pool.empty()) {
				REX::WARN("Picker: no scene fits {} actor(s) {}", cast.size(), here.empty() ? "here" : "on " + here);
				Close();
				return;
			}
			static std::mt19937 rng{ std::random_device{}() };
			const auto& scene = pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)];
			REX::INFO("Picker: starting \"{}\" ({}: {} {} scene(s))", scene->id, picked, pool.size(), here.empty() ? "floor" : here);
			StartPicked(scene->id);
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
			std::string title = "Scene";
			if (scene) {
				// During a transition, the options of where it's going.
				const auto settled = SceneRegistry::Settled(scene);
				for (const auto& nav : settled->navigations) {
					const auto dest = SceneRegistry::Find(nav.to);
					if (dest && (!SexesFit(*dest, current.ActorIDs()) || !SceneFitsFurniture(*dest, current.furniture))) {
						continue;
					}
					add(nav.to, FormatNavLabel(nav.label, current), dest ? dest->name : nav.to);
				}
				title = scene->name + "   (speed " + std::to_string(current.speed + 1) + "/" + std::to_string(scene->speeds.size()) + ")";
			}
			add(END_SCENE_ID, "End scene", "");

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
				NavigateSceneImpl(a_sceneID);
				return;
			}
			if (g_pickerMode == PickerMode::kNavigate) {
				if (a_sceneID == END_SCENE_ID) {
					if (g_vm) {
						RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
						g_vm->DispatchStaticCall("FourStimMenu"sv, "EndPlayerScene"sv, callback);
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
				g_pendingFurniture.reset();
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
					std::ranges::sort(pending.actors);
					REX::INFO("Picker: \"{}\" goes on {:08X} ({}, {:.0f} away)", a_sceneID, piece.ref, piece.type, piece.distance);
					g_pendingFurniture = std::move(pending);
				}
			}

			// Who takes which role: the cast's own order (the player first)
			// unless only another order fits the roles' sexes.
			auto order = SceneRegistry::AssignRoles(roles, SexesOf(cast));
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
				g_vm->DispatchStaticCall("FourStimMenu"sv, "StartPickedCast"sv, callback, std::move(a_sceneID), ordered, anchor);
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
				auto candidates = FindCandidates(g_settings.actorRadius);
				auto running = SnapshotRunning();
				std::scoped_lock lock(g_castLock);
				g_cast.clear();
				g_placeChoice = -1;
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
			if (const auto player = RE::PlayerCharacter::GetSingleton()) {
				Furniture::Reload();
				found = Furniture::FindNear(player->GetPosition(), g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.logFurniture);
				for (const auto& f : found) {
					REX::INFO("Furniture: nearest {}: {:08X}, {:.0f} away", f.type, f.ref, f.distance);
				}
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

		// The HUD, while focused, gets the navigation keys (both press and
		// release), unless the console, a pausing menu or the picker is up.
		if (const auto ui = RE::UI::GetSingleton(); HUD::IsFocused() && ui && !ui->GetMenuOpen(PICKER_MENU) &&
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

		if (!a_event->QJustPressed()) {
			return;
		}

		// Escape hatch: Esc (or the controller's B) always closes the picker,
		// whatever its AS3 side is doing.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
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

		// Keyboard codes are Windows virtual-key codes (N = 0x4E).
		if (a_event->device.get() != RE::INPUT_DEVICE::kKeyboard) {
			return;
		}

		// Speed keys, while the player is in a scene.
		if (const auto code = static_cast<std::uint32_t>(a_event->idCode); code == g_settings.speedUpKey || code == g_settings.speedDownKey) {
			if (GetPlayerScene().role0 != 0) {
				ChangeSceneSpeedImpl(code == g_settings.speedUpKey ? 1 : -1);
			}
			return;
		}
		// Auto mode key: on / off for the scene you're in or watching.
		if (g_autoConfig.key != 0 && static_cast<std::uint32_t>(a_event->idCode) == g_autoConfig.key) {
			const auto focused = GetPlayerScene();
			if (const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr) {
				if (active->autoMode.on) {
					StopAutoMode(*active);
				} else {
					StartAutoMode(*active);
				}
				Notify(active->autoMode.on ? "Auto mode on" : "Auto mode off");
			}
			return;
		}
		// OStim's other scene keys, for the scene you're in or watching (all
		// unbound by default): pull out, end, search, align, free camera, hide
		// the HUD. As OStim's EventListener.
		if (const auto code = static_cast<std::uint32_t>(a_event->idCode); GetPlayerScene().Active() &&
			(code == g_settings.pullOutKey || code == g_settings.endKey || code == g_settings.searchKey ||
				code == g_settings.alignmentKey || code == g_settings.freeCamKey || code == g_settings.hideUIKey) &&
			code != 0) {
			const auto focused = GetPlayerScene();
			if (code == g_settings.pullOutKey) {
				if (const auto active = FindActiveScene(focused.role0); active && !AutoPullOut(*active)) {
					Notify("4Stim: nowhere to pull out to from here");
				}
			} else if (code == g_settings.endKey) {
				if (g_vm) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					g_vm->DispatchStaticCall("FourStimMenu"sv, "EndPlayerScene"sv, callback);
				}
			} else if (code == g_settings.searchKey) {
				ShowPicker(PickerMode::kSearch);
			} else if (code == g_settings.alignmentKey) {
				HUD::ToggleTab("align");
			} else if (code == g_settings.freeCamKey) {
				ToggleSceneFreeCamera();
			} else if (code == g_settings.hideUIKey) {
				HUD::ToggleHidden();
			}
			return;
		}
		// The NPC scene key (or Shift + the hotkey): the picker for a scene
		// without you, whether or not you're in one.
		{
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const bool shift = g_shiftDown;
			if (g_vm && (g_settings.npcSceneKey != 0 ? code == g_settings.npcSceneKey : (code == g_settings.hotkey && shift))) {
				REX::INFO("NPC scene key pressed");
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				g_vm->DispatchStaticCall("FourStimMenu"sv, "OnNPCSceneHotkey"sv, callback,
					g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
				return;
			}
		}
		if (static_cast<std::uint32_t>(a_event->idCode) != g_settings.hotkey) {
			return;
		}
		// In a scene with the HUD up, the hotkey switches the HUD's focus.
		if (GetPlayerScene().Active() && HUD::IsOpen()) {
			HUD::SetFocus(!HUD::IsFocused());
			return;
		}
		if (!g_vm) {
			return;
		}
		REX::INFO("Hotkey pressed");
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		g_vm->DispatchStaticCall("FourStimMenu"sv, "OnHotkey"sv, callback,
			g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
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
			const auto id = ref->GetFormID();
			std::string sceneID;
			float       seconds = 0.0F;
			{
				std::scoped_lock lock(g_animWatchLock);
				const auto it = g_animWatch.find(id);
				if (it == g_animWatch.end()) {
					return;
				}
				seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - it->second.start).count();
				if (seconds > 60.0F) {  // long enough for any transition
					g_animWatch.erase(it);
					return;
				}
				sceneID = it->second.sceneID;
			}
			if (g_settings.logAnimEvents && seconds <= 5.0F) {
				REX::INFO("AnimEvent: {:08X} +{:.2f}s in \"{}\": \"{}\" ({})", id, seconds, sceneID,
					a_event.tag.c_str(), a_event.payload.c_str());
			}
			if (_stricmp(a_event.tag.c_str(), "IdleStop") == 0) {
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
			} else if (_stricmp(a_event.tag.c_str(), "4StimClimax") == 0) {
				// The climax annotation (OStim's OStimClimax): this actor
				// climaxes now, as OStim's climax() does when its annotation
				// comes, whether or not they were waiting for it.
				F4SE::GetTaskInterface()->AddTask([id]() {
					if (const auto active = FindActiveScene(id)) {
						REX::INFO("Climax: {:08X} 4StimClimax annotation", id);
						ClimaxNow(*active, id);
					}
				});
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

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		g_vm = a_vm;

		REX::INFO("RegisterPapyrusFunctions called, binding {}", SCRIPT_NAME);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartScene"sv, StartScene);
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
		a_vm->BindNativeMethod(SCRIPT_NAME, "RegisterForSceneEvents"sv, RegisterForSceneEvents);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnregisterForSceneEvents"sv, UnregisterForSceneEvents);
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
	constexpr std::uint32_t SCENES_VERSION = 2;  // 2: each actor's own scale (alignment scales them)

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
			}
			WriteString(a_intfc, active.sceneID);
			a_intfc->WriteRecordData(static_cast<std::int32_t>(active.speed));
			a_intfc->WriteRecordData(active.furnitureRef);
			WriteString(a_intfc, active.furnitureType);
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(active.autoMode.on ? 1 : 0));
		}
		if (count > 0) {
			REX::INFO("Save data: {} running scene(s) saved", count);
		}
	}

	void OnGameLoadedData(const F4SE::SerializationInterface* a_intfc)
	{
		std::vector<SavedScene> scenes;
		std::uint32_t type = 0, version = 0, length = 0;
		auto resolve = [&](std::uint32_t a_id) { return a_intfc->ResolveFormID(a_id).value_or(0); };
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
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
		std::scoped_lock lock(g_savedLock);
		g_loadedScenes.clear();
	}

	// Lets everyone in a saved scene go: unrestrained, interaction back, and
	// what was taken off back on. Main thread.
	void ReleaseSavedScene(const SavedScene& a_saved, const char* a_why)
	{
		REX::INFO("Save data: \"{}\" not started again ({}): letting its actors go", a_saved.sceneID, a_why);
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			const auto id = a_saved.actors[i];
			const auto actor = id ? RE::TESForm::GetFormByID<RE::Actor>(id) : nullptr;
			if (!actor) {
				continue;
			}
			if (a_saved.baseScale[i] > 0.0F) {
				CallActorMethod(actor, "ObjectReference"sv, "SetScale"sv, a_saved.baseScale[i]);  // undo the alignment's size
			}
			if (!a_saved.stripped[i].empty()) {
				Undress::NoteStripped(id, a_saved.stripped[i]);
				Undress::Redress(id, true);
			}
			if (!IsPlayer(actor) && g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				g_vm->DispatchStaticCall("FourStimScene"sv, "ReleaseAfterLoad"sv, callback, static_cast<std::int32_t>(id));
			}
		}
	}

	// Starts a saved scene again (or lets its actors go). Main thread.
	void ResumeSavedScene(const SavedScene& a_saved)
	{
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
			g_pendingResume = std::move(resume);
		}
		if (a_saved.furnitureRef) {
			std::scoped_lock lock(g_pendingFurnitureLock);
			PendingFurniture pending{ a_saved.furnitureRef, a_saved.furnitureType, sorted };
			pending.offset = scene->furnitureOffset;
			g_pendingFurniture = std::move(pending);
		}
		const bool   withPlayer = std::ranges::any_of(a_saved.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		std::vector<std::int32_t> ids(a_saved.actors.begin(), a_saved.actors.end());
		const auto   anchor = withPlayer ? 0 : ids.front();
		REX::INFO("Save data: starting \"{}\" again ({} actor(s))", a_saved.sceneID, ids.size());
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		g_vm->DispatchStaticCall("FourStimMenu"sv, "StartPickedCast"sv, callback, a_saved.sceneID, ids, anchor);
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
		std::thread([scenes = std::move(scenes)]() {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			for (int i = 0; i < 600 && LoadingOrFading(); ++i) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1500));
			for (const auto& saved : scenes) {
				F4SE::GetTaskInterface()->AddTask([saved]() { ResumeSavedScene(saved); });
				std::this_thread::sleep_for(std::chrono::seconds(4));
			}
		}).detach();
	}

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kPostLoad:
			REX::INFO("kPostLoad");
			break;
		case F4SE::MessagingInterface::kPreLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			// Scene-event registrations aren't saved: scripts register again on
			// load. Scenes don't survive loading either, so the HUD goes too.
			SceneEvents::Clear();
			SetPlayerScene({});
			HUD::Reset();
			// Running scenes and their timers end with the old game.
			F4SE::GetTaskInterface()->AddTask([]() {
				g_activeScenes.clear();
				Excitement::Clear();
				Undress::Clear();
			});
			{
				std::scoped_lock lock(g_pendingSequenceLock);
				g_pendingSequence.reset();
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
			ResumeSavedScenes();
			break;
		case F4SE::MessagingInterface::kGameLoaded:
			{
				REX::INFO("kGameLoaded");
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
				HUD::LoadConfig();
				// A loaded save starts with no scene running.
				HUD::Reset();
				g_activeScenes.clear();
				Excitement::Clear();
				Undress::Clear();
				g_focusNextScene = false;
				SetPlayerScene({});
				g_sceneCameraActive = false;
				g_startViewSaved = false;
				RestoreFreeCameraSpeed();
				RestoreFOV();
				EndSlowMotion();
				g_sceneFade = false;
				ReleaseSceneLayer();
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
			g_vm->DispatchStaticCall("FourStimMenu"sv, "EndPlayerScene"sv, callback);
		}
	}

	void OpenSearchForFocused()
	{
		if (GetPlayerScene().Active()) {
			ShowPicker(PickerMode::kSearch);
		}
	}

	std::string FormatLabel(std::string a_label, const FocusedScene& a_scene)
	{
		return FormatNavLabel(std::move(a_label), a_scene);
	}

	bool FocusedCanPlay(const SceneRegistry::Scene& a_scene)
	{
		const auto focused = GetPlayerScene();
		return SexesFit(a_scene, focused.ActorIDs()) && SceneFitsFurniture(a_scene, focused.furniture);
	}

	bool GetFocusedAlignment(std::size_t a_role, Alignment::Offset& a_offset, std::string& a_sceneID)
	{
		const auto focused = GetPlayerScene();
		const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
		if (!active || a_role >= active->actors.size()) {
			return false;
		}
		const auto scene = SceneRegistry::Find(active->sceneID);
		a_sceneID = scene ? SceneRegistry::Settled(scene)->id : active->sceneID;
		a_offset = Alignment::Get(active->alignKey, a_sceneID, a_role);
		return true;
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
		REX::INFO("Alignment: \"{}\" ({}) role {}: x {:.1f} y {:.1f} z {:.1f} rot {:.1f} scale {:.2f}", sceneID, active->alignKey, a_role, a_offset.x, a_offset.y,
			a_offset.z, a_offset.rot, a_offset.scale);
		LockScene(*active);
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

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	LoadSettings();
	HUD::SetOptions(g_settings.hudTheme, g_settings.hudEnabled);
	InputHook::Install();
	F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
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
