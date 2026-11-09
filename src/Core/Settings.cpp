#include "Core/Core.h"

namespace Core
{
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
}
