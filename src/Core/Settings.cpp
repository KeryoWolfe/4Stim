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

	// sPenisBendNode / sPenisBendUp / sPenisBendDown / sPenisFlaccid: comma
	// lists, nodes base first and their shares of a bend up / down.
	void ReadPenisBendList(std::string_view key, const std::string& value)
	{
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
	}

	// Every setting 4Stim.ini (and the MCM, through MCM::ToIni) can set, by
	// its lowercase name: what it sets, and how it's read and clamped. A new
	// setting is a line here and a member where it's kept. OStim's names are
	// turned into these first (OSTIM_SETTING_NAMES). Keys with a number in
	// them (bUndressSlot30...61) and the sex toys' (SexToys::ReadSetting,
	// Intiface::ReadSetting) are read in LoadSettings.
	using Setter = void (*)(const std::string& value);
	const std::unordered_map<std::string_view, Setter> SETTINGS{
		{ "ihotkey", [](const std::string& value) { g_settings.hotkey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "inpcscenekey", [](const std::string& value) { g_settings.npcSceneKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "bresumescenes", [](const std::string& value) { g_settings.resumeScenes = std::stoi(value) != 0; } },
		{ "stargetmode", [](const std::string& value) {
			auto v = value;
			std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			g_settings.targetMode = (v == "proximity") ? 1 : 0;
		} },
		{ "fmaxdistance", [](const std::string& value) { g_settings.maxDistance = std::stof(value); } },
		{ "fcrosshaircone", [](const std::string& value) { g_settings.crosshairCone = std::stof(value); } },
		{ "fproximityradius", [](const std::string& value) { g_settings.proximityRadius = std::stof(value); } },
		{ "ffreecameraspeed", [](const std::string& value) { g_settings.freeCamSpeedMult = std::max(std::stof(value), 0.01F); } },  // older 4Stim setting: a multiplier
		{ "fcameraspeed", [](const std::string& value) {
			g_settings.freeCamSpeed = std::max(std::stof(value), 0.01F);
			g_settings.freeCamSpeedMult = -1.0F;
		} },
		{ "busefreecam", [](const std::string& value) { g_settings.useFreeCam = std::stoi(value) != 0; } },
		{ "ffreecamfov", [](const std::string& value) { g_settings.freeCamFOV = std::clamp(std::stof(value), 0.0F, 150.0F); } },
		{ "bforcefirstperson", [](const std::string& value) { g_settings.forceFirstPerson = std::stoi(value) != 0; } },
		{ "busefades", [](const std::string& value) { g_settings.useFades = std::stoi(value) != 0; } },
		{ "bnofacialexpressions", [](const std::string& value) { Expressions::Settings().disabled = std::stoi(value) != 0; } },
		{ "fexpressiondurationmin", [](const std::string& value) { Expressions::Settings().durationMin = std::max(std::stof(value), 0.1F); } },
		{ "fexpressiondurationmax", [](const std::string& value) { Expressions::Settings().durationMax = std::max(std::stof(value), 0.1F); } },
		{ "fmoanintervalmin", [](const std::string& value) { Sounds::Settings().moanIntervalMin = std::max(std::stof(value), 0.2F); } },
		{ "fmoanintervalmax", [](const std::string& value) { Sounds::Settings().moanIntervalMax = std::max(std::stof(value), 0.2F); } },
		{ "fmoanvolume", [](const std::string& value) { Sounds::Settings().moanVolume = std::clamp(std::stof(value), 0.0F, 1.0F); } },
		{ "bsounds", [](const std::string& value) { Sounds::Settings().disabled = std::stoi(value) == 0; } },
		{ "fsoundvolume", [](const std::string& value) { Sounds::Settings().soundVolume = std::clamp(std::stof(value), 0.0F, 1.0F); } },
		{ "bonlylightindark", [](const std::string& value) { Lights::Settings().onlyInDark = std::stoi(value) != 0; } },
		{ "flightleveldark", [](const std::string& value) { Lights::Settings().darkLevel = std::stof(value); } },
		{ "smalelight", [](const std::string& value) { Lights::Settings().maleLight = value; } },
		{ "sfemalelight", [](const std::string& value) { Lights::Settings().femaleLight = value; } },
		{ "splayerlight", [](const std::string& value) { Lights::Settings().playerLight = value; } },
		{ "isoundflags", [](const std::string& value) { Sounds::Settings().flags = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "isoundpriority", [](const std::string& value) { Sounds::Settings().priority = static_cast<std::uint8_t>(std::clamp(std::stoi(value, nullptr, 0), 0, 255)); } },
		{ "bdisablescaling", [](const std::string& value) { Scaling::Settings().disabled = std::stoi(value) != 0; } },
		{ "buseintroscenes", [](const std::string& value) { g_settings.useIntroScenes = std::stoi(value) != 0; } },
		{ "setcustomtimescale", [](const std::string& value) { g_settings.customTimescale = std::max(std::stof(value), 0.0F); } },
		{ "setusescreenshake", [](const std::string& value) { Excitement::Settings().useScreenShake = std::stoi(value) != 0; } },
		{ "addactorsatstart", [](const std::string& value) { g_settings.addActorsAtStart = std::stoi(value) != 0; } },
		{ "buseautofades", [](const std::string& value) { g_settings.useAutoFades = std::stoi(value) != 0; } },
		{ "fnpcsceneduration", [](const std::string& value) { g_settings.npcSceneDuration = std::max(std::stof(value), 0.0F); } },
		{ "ipulloutkey", [](const std::string& value) { g_settings.pullOutKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "iendkey", [](const std::string& value) { g_settings.endKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "isearchkey", [](const std::string& value) { g_settings.searchKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "ialignmentkey", [](const std::string& value) { g_settings.alignmentKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "ifreecamkey", [](const std::string& value) { g_settings.freeCamKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "ihideuikey", [](const std::string& value) { g_settings.hideUIKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "ispeedupkey", [](const std::string& value) { g_settings.speedUpKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "ispeeddownkey", [](const std::string& value) { g_settings.speedDownKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "stheme", [](const std::string& value) { g_settings.hudTheme = value; } },
		{ "spadscheme", [](const std::string& value) {
			auto v = value;
			std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			g_settings.padClassic = v == "classic";
		} },
		{ "fhudfadetime", [](const std::string& value) { g_settings.hudFadeTime = std::clamp(std::stof(value), 0.0F, 600.0F); } },
		{ "spadhotkey", [](const std::string& value) { g_settings.padHotkey = PadButton(value); } },
		{ "spadnpcscene", [](const std::string& value) { g_settings.padNpcScene = PadButton(value); } },
		{ "spadspeedup", [](const std::string& value) { g_settings.padSpeedUp = PadButton(value); } },
		{ "spadspeeddown", [](const std::string& value) { g_settings.padSpeedDown = PadButton(value); } },
		{ "spadautomode", [](const std::string& value) { g_settings.padAutoMode = PadButton(value); } },
		{ "spadpullout", [](const std::string& value) { g_settings.padPullOut = PadButton(value); } },
		{ "spadend", [](const std::string& value) { g_settings.padEnd = PadButton(value); } },
		{ "spadsearch", [](const std::string& value) { g_settings.padSearch = PadButton(value); } },
		{ "spadalign", [](const std::string& value) { g_settings.padAlign = PadButton(value); } },
		{ "spadfreecam", [](const std::string& value) { g_settings.padFreeCam = PadButton(value); } },
		{ "spadhideui", [](const std::string& value) { g_settings.padHideUI = PadButton(value); } },
		{ "benableplayerbar", [](const std::string& value) { g_settings.playerBar = std::stoi(value) != 0; } },
		{ "benablenpcbar", [](const std::string& value) { g_settings.npcBar = std::stoi(value) != 0; } },
		{ "bautohidebars", [](const std::string& value) { g_settings.autoHideBars = std::stoi(value) != 0; } },
		{ "bmatchcolortogender", [](const std::string& value) { g_settings.matchBarColor = std::stoi(value) != 0; } },
		{ "inavupkey", [](const std::string& value) { g_settings.navUpKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "inavdownkey", [](const std::string& value) { g_settings.navDownKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "inavleftkey", [](const std::string& value) { g_settings.navLeftKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "inavrightkey", [](const std::string& value) { g_settings.navRightKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "inavyeskey", [](const std::string& value) { g_settings.navYesKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "inavtogglekey", [](const std::string& value) { g_settings.navToggleKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
		{ "shudcontrols", [](const std::string& value) {
			auto v = value;
			std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			g_settings.hudColumns = v == "columns";
		} },
		{ "benablehud", [](const std::string& value) { g_settings.hudEnabled = std::stoi(value) != 0; } },
		{ "factorradius", [](const std::string& value) { g_settings.actorRadius = std::stof(value); } },
		{ "ffurnitureradius", [](const std::string& value) { g_settings.furnitureRadius = std::stof(value); } },
		{ "ffurnitureheight", [](const std::string& value) { g_settings.furnitureHeight = std::stof(value); } },
		{ "blogfurniture", [](const std::string& value) { g_settings.logFurniture = std::stoi(value) != 0; } },
		{ "setenablefurniture", [](const std::string& value) { g_settings.useFurniture = std::stoi(value) != 0; } },
		{ "setbedrealignment", [](const std::string& value) { Furniture::Settings().bedRealignment = std::stof(value); } },
		{ "bedoffset", [](const std::string& value) { Furniture::Settings().bedOffset = std::stof(value); } },
		{ "setselectfurniture", [](const std::string& value) { g_settings.selectFurniture = std::stoi(value) != 0; } },
		{ "setresetclutter", [](const std::string& value) { g_settings.resetClutter = std::stoi(value) != 0; } },
		{ "setresetclutterradius", [](const std::string& value) { g_settings.resetClutterRadius = std::stof(value) * 100.0F; } },  // OStim's steps of 100 units
		{ "bmatchsex", [](const std::string& value) { g_settings.matchSex = std::stoi(value) != 0; } },
		{ "playeralwaysdomstraight", [](const std::string& value) { g_settings.playerAlwaysDomStraight = std::stoi(value) != 0; } },
		{ "playeralwayssubstraight", [](const std::string& value) { g_settings.playerAlwaysSubStraight = std::stoi(value) != 0; } },
		{ "playeralwaysdomgay", [](const std::string& value) { g_settings.playerAlwaysDomGay = std::stoi(value) != 0; } },
		{ "playeralwayssubgay", [](const std::string& value) { g_settings.playerAlwaysSubGay = std::stoi(value) != 0; } },
		{ "playerselectrolestraight", [](const std::string& value) { g_settings.playerSelectRoleStraight = std::stoi(value) != 0; } },
		{ "playerselectrolegay", [](const std::string& value) { g_settings.playerSelectRoleGay = std::stoi(value) != 0; } },
		{ "playerselectrolethreesome", [](const std::string& value) { g_settings.playerSelectRoleThreesome = std::stoi(value) != 0; } },
		{ "unrestrictednavigation", [](const std::string& value) { SceneRegistry::Rules().unrestricted = std::stoi(value) != 0; } },
		{ "futausemalerole", [](const std::string& value) { ActorProperties::Settings().futaUseMaleRole = std::stoi(value) != 0; } },
		{ "futausemaleexcitement", [](const std::string& value) { ActorProperties::Settings().futaUseMaleExcitement = std::stoi(value) != 0; } },
		{ "futausemaleclimax", [](const std::string& value) { ActorProperties::Settings().futaUseMaleClimax = std::stoi(value) != 0; } },
		{ "bdisablepenisbend", [](const std::string& value) { PenisBend::Settings().disabled = std::stoi(value) != 0; } },
		{ "fpenisbendstep", [](const std::string& value) { PenisBend::Settings().stepDegrees = std::clamp(std::stof(value), -45.0F, 45.0F); } },
		{ "spenisbendnode", [](const std::string& value) { ReadPenisBendList("spenisbendnode", value); } },
		{ "spenisbendup", [](const std::string& value) { ReadPenisBendList("spenisbendup", value); } },
		{ "spenisbenddown", [](const std::string& value) { ReadPenisBendList("spenisbenddown", value); } },
		{ "spenisflaccid", [](const std::string& value) { ReadPenisBendList("spenisflaccid", value); } },
		{ "ipenisbendaxis", [](const std::string& value) { PenisBend::Settings().axis = std::clamp(std::stoi(value), 0, 2); } },
		{ "berections", [](const std::string& value) { Erection::Settings().enabled = std::stoi(value) != 0; } },
		{ "unequipstraponifnotneeded", [](const std::string& value) { EquipObjects::Settings().unequipStrapOnIfNotNeeded = std::stoi(value) != 0; } },
		{ "sdefaultstrapon", [](const std::string& value) { EquipObjects::Settings().defaults[EquipObjects::FEMALE]["strapon"] = value; } },  // OStim's Default Strap-On (0x1)
		{ "splayerstrapon", [](const std::string& value) { EquipObjects::Settings().defaults[EquipObjects::PLAYER]["strapon"] = value; } },
		{ "sdefaulttonguemale", [](const std::string& value) { EquipObjects::Settings().defaults[EquipObjects::MALE]["tongue"] = value; } },
		{ "sdefaulttonguefemale", [](const std::string& value) { EquipObjects::Settings().defaults[EquipObjects::FEMALE]["tongue"] = value; } },
		{ "splayertongue", [](const std::string& value) { EquipObjects::Settings().defaults[EquipObjects::PLAYER]["tongue"] = value; } },
		{ "maledialoguecountdownmin", [](const std::string& value) { Sounds::Settings().maleDialogueCountdownMin = std::max(std::stoi(value), 0); } },
		{ "maledialoguecountdownmax", [](const std::string& value) { Sounds::Settings().maleDialogueCountdownMax = std::max(std::stoi(value), 0); } },
		{ "femaledialoguecountdownmin", [](const std::string& value) { Sounds::Settings().femaleDialogueCountdownMin = std::max(std::stoi(value), 0); } },
		{ "femaledialoguecountdownmax", [](const std::string& value) { Sounds::Settings().femaleDialogueCountdownMax = std::max(std::stoi(value), 0); } },
		{ "playerdialogue", [](const std::string& value) { Sounds::Settings().playerDialogue = std::stoi(value) != 0; } },
		{ "preventsamevoicecrosstalk", [](const std::string& value) { Sounds::Settings().preventSameVoiceCrossTalk = std::stoi(value) != 0; } },
		{ "splayervoice", [](const std::string& value) { Sounds::Settings().playerVoice = value; } },
		{ "bfurnitureoutoftheway", [](const std::string& value) { g_settings.furnitureOutOfWay = std::stoi(value) != 0; } },
		{ "bresetphysicsafterscene", [](const std::string& value) { Physics::Settings().resetAfterScene = std::stoi(value) != 0; } },
		{ "bpassthroughwalls", [](const std::string& value) { g_settings.passThroughWalls = std::stoi(value) != 0; } },
		{ "bcollisionwatch", [](const std::string& value) { g_settings.collisionWatch = std::stoi(value) != 0; } },
		{ "bplayeraidriven", [](const std::string& value) { g_settings.playerAIDriven = std::stoi(value) != 0; } },
		{ "btranslationlock", [](const std::string& value) { g_settings.translationLock = std::stoi(value) != 0; } },
		{ "bdirectnatives", [](const std::string& value) { g_settings.directNatives = std::stoi(value) != 0; } },
		{ "btimeanimcues", [](const std::string& value) { g_settings.timeAnimCues = std::stoi(value) != 0; } },
		{ "bloganimevents", [](const std::string& value) { g_settings.logAnimEvents = std::stoi(value) != 0; } },
		{ "benableexcitement", [](const std::string& value) { Excitement::Settings().enabled = std::stoi(value) != 0; } },
		{ "fmaleexcitementmult", [](const std::string& value) { Excitement::Settings().maleMult = std::max(std::stof(value), 0.0F); } },
		{ "ffemaleexcitementmult", [](const std::string& value) { Excitement::Settings().femaleMult = std::max(std::stof(value), 0.0F); } },
		{ "fexcitementdecayrate", [](const std::string& value) { Excitement::Settings().decayRate = std::max(std::stof(value), 0.0F); } },
		{ "fexcitementdecaygrace", [](const std::string& value) { Excitement::Settings().decayGrace = std::max(std::stof(value), 0.0F); } },
		{ "fpostclimaxexcitement", [](const std::string& value) { Excitement::Settings().postClimax = std::clamp(std::stof(value), 0.0F, 99.0F); } },
		{ "fpostclimaxexcitementmax", [](const std::string& value) { Excitement::Settings().postClimaxMax = std::clamp(std::stof(value), 0.0F, 99.0F); } },
		{ "bclimaxscenes", [](const std::string& value) { Excitement::Settings().climaxScenes = std::stoi(value) != 0; } },
		{ "bendonplayerclimax", [](const std::string& value) { Excitement::Settings().endOnPlayer = std::stoi(value) != 0; } },
		{ "bendonmaleclimax", [](const std::string& value) { Excitement::Settings().endOnMale = std::stoi(value) != 0; } },
		{ "bendonfemaleclimax", [](const std::string& value) { Excitement::Settings().endOnFemale = std::stoi(value) != 0; } },
		{ "bendonallclimax", [](const std::string& value) { Excitement::Settings().endOnAll = std::stoi(value) != 0; } },
		{ "bendnpcscenesonclimax", [](const std::string& value) { Excitement::Settings().endNPCScenes = std::stoi(value) != 0; } },
		{ "fclimaxenddelay", [](const std::string& value) { Excitement::Settings().endDelay = std::clamp(std::stof(value), 0.0F, 60.0F); } },
		{ "fclimaxshake", [](const std::string& value) { Excitement::Settings().shake = std::clamp(std::stof(value), 0.0F, 5.0F); } },
		{ "fclimaxblur", [](const std::string& value) { Excitement::Settings().blur = std::clamp(std::stof(value), 0.0F, 1.0F); } },
		{ "fclimaxflash", [](const std::string& value) { Excitement::Settings().flash = std::clamp(std::stof(value), 0.0F, 1.0F); } },
		{ "fclimaxglowsize", [](const std::string& value) { Excitement::Settings().glowSize = std::clamp(std::stof(value), 0.01F, 0.5F); } },
		{ "bblurorgasms", [](const std::string& value) { Excitement::Settings().blurOn = std::stoi(value) != 0; } },
		{ "bslowmoorgasms", [](const std::string& value) { Excitement::Settings().slowMo = std::stoi(value) != 0; } },
		{ "bclimaxrumble", [](const std::string& value) { Excitement::Settings().rumble = std::stoi(value) != 0; } },
		{ "balignbysex", [](const std::string& value) { Alignment::Settings().groupBySex = std::stoi(value) != 0; } },
		{ "balignbyheight", [](const std::string& value) { Alignment::Settings().groupByHeight = std::stoi(value) != 0; } },
		{ "balignbyheels", [](const std::string& value) { Alignment::Settings().groupByHeels = std::stoi(value) != 0; } },
		{ "bresetposition", [](const std::string& value) { g_resetPosition = std::stoi(value) != 0; } },
		{ "bendondamage", [](const std::string& value) { g_endOnDamage = std::stoi(value) != 0; } },
		{ "bstaywheninterrupted", [](const std::string& value) { g_stayWhenInterrupted = std::stoi(value) != 0; } },
		{ "blockscenes", [](const std::string& value) { g_lockScenes = std::stoi(value) != 0; } },
		{ "bguardscenes", [](const std::string& value) { g_guardScenes = std::stoi(value) != 0; } },
		{ "bundress", [](const std::string& value) { Undress::Settings().enabled = std::stoi(value) != 0; } },
		{ "bundressatstart", [](const std::string& value) { Undress::Settings().atStart = std::stoi(value) != 0; } },
		{ "bpartialundress", [](const std::string& value) { Undress::Settings().partial = std::stoi(value) != 0; } },
		{ "bfullundressmidscene", [](const std::string& value) { Undress::Settings().fullMidScene = std::stoi(value) != 0; } },
		{ "bundressplayer", [](const std::string& value) { Undress::Settings().player = std::stoi(value) != 0; } },
		{ "bredress", [](const std::string& value) { Undress::Settings().redress = std::stoi(value) != 0; } },
		{ "fundressitemdelay", [](const std::string& value) { Undress::Settings().itemDelay = std::clamp(std::stof(value), 0.0F, 5.0F); } },
		{ "banimateredress", [](const std::string& value) { Undress::Settings().animateRedress = std::stoi(value) != 0; } },
		{ "setremoveweaponsatstart", [](const std::string& value) { Undress::Settings().removeWeaponsAtStart = std::stoi(value) != 0; } },
		{ "setremoveweaponswithslot", [](const std::string& value) { Undress::Settings().removeWeaponsWithSlot = std::stoi(value); } },
		{ "setundressingslotmask", [](const std::string& value) {
			// OStim's name: a mask, bit 0 = slot 30.
			const auto       mask = static_cast<std::uint32_t>(std::stoll(value, nullptr, 0));
			std::vector<int> slots;
			for (int slot = 30; slot <= 61; ++slot) {
				if (mask & (1u << (slot - 30))) {
					slots.push_back(slot);
				}
			}
			Undress::Settings().slots = std::move(slots);
		} },
		{ "sundressslots", [](const std::string& value) {
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
		} },
		{ "bautomodeplayer", [](const std::string& value) { g_autoConfig.player = std::stoi(value) != 0; } },
		{ "bautomodesolo", [](const std::string& value) { g_autoConfig.playerSolo = std::stoi(value) != 0; } },
		{ "bautomodepartner", [](const std::string& value) { g_autoConfig.playerPartner = std::stoi(value) != 0; } },
		{ "bautomodenpc", [](const std::string& value) { g_autoConfig.npc = std::stoi(value) != 0; } },
		{ "fautomodescenemin", [](const std::string& value) { g_autoConfig.sceneMin = std::max(std::stof(value), 2.0F); } },
		{ "fautomodescenemax", [](const std::string& value) { g_autoConfig.sceneMax = std::max(std::stof(value), 2.0F); } },
		{ "iforeplaychance", [](const std::string& value) { g_autoConfig.foreplayChance = std::clamp(std::stoi(value), 0, 100); } },
		{ "fforeplayendmin", [](const std::string& value) { g_autoConfig.foreplayMin = std::clamp(std::stof(value), 0.0F, 100.0F); } },
		{ "fforeplayendmax", [](const std::string& value) { g_autoConfig.foreplayMax = std::clamp(std::stof(value), 0.0F, 100.0F); } },
		{ "ipulloutchance", [](const std::string& value) { g_autoConfig.pulloutChance = std::clamp(std::stoi(value), 0, 100); } },
		{ "fpulloutmin", [](const std::string& value) { g_autoConfig.pulloutMin = std::clamp(std::stof(value), 0.0F, 100.0F); } },
		{ "fpulloutmax", [](const std::string& value) { g_autoConfig.pulloutMax = std::clamp(std::stof(value), 0.0F, 100.0F); } },
		{ "iautomodemaxsteps", [](const std::string& value) { g_autoConfig.maxSteps = std::clamp(std::stoi(value), 1, 50); } },
		{ "bautomodelimittonavigation", [](const std::string& value) { g_autoConfig.limitToNavigation = std::stoi(value) != 0; } },
		{ "bautomodestandingonfloor", [](const std::string& value) { g_autoConfig.standingOnFloor = std::stoi(value) != 0; } },
		{ "bautospeed", [](const std::string& value) { g_autoConfig.autoSpeed = std::stoi(value) != 0; } },
		{ "fautospeedintervalmin", [](const std::string& value) { g_autoConfig.speedIntervalMin = std::max(std::stof(value), 0.5F); } },
		{ "fautospeedintervalmax", [](const std::string& value) { g_autoConfig.speedIntervalMax = std::max(std::stof(value), 0.5F); } },
		{ "fautospeedexcitementmin", [](const std::string& value) { g_autoConfig.speedExcitementMin = std::clamp(std::stof(value), 0.0F, 100.0F); } },
		{ "fautospeedexcitementmax", [](const std::string& value) { g_autoConfig.speedExcitementMax = std::clamp(std::stof(value), 0.0F, 100.0F); } },
		{ "ftransitionlead", [](const std::string& value) { g_settings.transitionLead = std::clamp(std::stof(value), -2.0F, 5.0F); } },
		{ "buseplaybackspeed", [](const std::string& value) { g_settings.usePlaybackSpeed = std::stoi(value) != 0; } },
		{ "bdisableeyelook", [](const std::string& value) { EyeLook::Settings().disabled = std::stoi(value) != 0; } },
		{ "flookmaxside", [](const std::string& value) { EyeLook::Settings().maxSide = std::clamp(std::stof(value), 0.0F, 60.0F); } },
		{ "flookmaxup", [](const std::string& value) { EyeLook::Settings().maxUp = std::clamp(std::stof(value), 0.0F, 60.0F); } },
		{ "slefteyenode", [](const std::string& value) { EyeLook::Settings().leftEye = value; } },
		{ "srighteyenode", [](const std::string& value) { EyeLook::Settings().rightEye = value; } },
		{ "iautomodekey", [](const std::string& value) { g_autoConfig.key = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); } },
	};

	// 4Stim.ini, then the MCM's file over it (docs/MCM.md). Again whenever
	// the MCM changes a setting (FourStim.ReloadSettings).
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
				if (const auto setting = SETTINGS.find(key); setting != SETTINGS.end()) {
					setting->second(value);
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
				} else if (!SexToys::ReadSetting(key, value)) {
					Intiface::ReadSetting(key, value);
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
