# 4Stim

An OStim-style scene framework for **Fallout 4** (1.11.240, "Anniversary Edition"), built as an F4SE plugin. Two actors (or one) share a spot, play paired animations, and move between scenes through an in-scene HUD. Animation authors add content with an Idle-record plugin and a JSON file, with no scripting.

> **Status:** alpha (0.1.0), in private testing. Expect breaking changes. Testers: see [docs/ALPHA.md](docs/ALPHA.md); changes are in [CHANGELOG.md](CHANGELOG.md).

## Features

- **Scenes from JSON:** solo and two-actor scenes, multiple speeds, and navigation links between scenes ([docs/SCENES.md](docs/SCENES.md)).
- **Actions:** scenes say who does what to whom (OStim's action types), and roles only go to actors who fit them ([docs/ACTIONS.md](docs/ACTIONS.md)).
- **Excitement and climax:** actors get excited by what the scene's actions do to them, shown on the HUD's meters, and climax at 100, with climax animations and scene-end rules like OStim's ([docs/EXCITEMENT.md](docs/EXCITEMENT.md)).
- **Auto mode:** scenes move on by themselves, OStim-style: foreplay, intercourse, pull-out, speeding up with excitement ([docs/AUTOMODE.md](docs/AUTOMODE.md)).
- **Undressing:** actors take off what each scene's actions need, one piece at a time, and dress again afterwards; undress or dress anyone in the scene from the HUD ([docs/UNDRESS.md](docs/UNDRESS.md)).
- **Transitions:** short "in-between" animations that play once and move on to their destination by themselves.
- **Sequences:** fixed runs of scenes, each played for a set time, startable from the picker, the HUD or Papyrus.
- **In-scene HUD:** a Scaleform HUD with a navigation list, scene icons (`.dds`), actor and speed meters, and keyboard and gamepad control.
- **Customizable HUD:** themes, a replaceable logo animation, Utility entries, or a complete replacement HUD ([docs/HUD_API.md](docs/HUD_API.md)).
- **Starting a scene, like OStim:** the hotkey lists the NPCs near you (the one you're looking at already picked); add as many as a scene allows, then pick where (right here, or a bed, chair, table... near you), and a fitting idle scene starts. Or browse every scene that fits.
- **Group scenes:** three or more actors, as the scenes allow.
- **Several scenes at once:** start scenes with or without yourself in them ("You: not in it" in the picker), and any number run side by side, each with its own excitement and auto mode. "Running scenes" in the picker, or the hotkey aimed at someone in a scene, lets you watch one (free camera and HUD; "Stop watching" leaves it running), switch its auto mode or end it. An actor is never in two scenes at once. Shift + the hotkey (or `iNPCSceneKey`) starts an NPC scene even while you're in one.
- **Saving mid-scene:** scenes running when you save start again when that save is loaded (`bResumeScenes`).
- **Free camera** during scenes, with the player's controls locked to what a scene allows.
- **Scene events for other mods:** start, change, speed change and end, delivered to Papyrus.

Children are never eligible for scenes.

## Requirements (players)

- Fallout 4 **1.11.240**
- [F4SE](https://f4se.silverlock.org/) for that runtime
- Address Library for F4SE Plugins, for that runtime
- An animation pack with a 4Stim scene file (the test pack in `Data\F4SE\Plugins\4Stim\Scenes\` needs its matching `4StimTestAnims.esp` and animations, which aren't in this repository)

## Usage

| Key | Action |
|---|---|
| `N` (hotkey) | Out of a scene: start one (who, where, then an idle). In a scene: give the HUD the arrow keys / d-pad, or hand them back. |
| Arrow keys / d-pad | Move through the HUD (tabs, navigation list) |
| Enter / A | Choose |
| Esc or Backspace / B | Back, or hand the keys back to the game |
| `=` / `-` (gamepad Y / X) | Scene speed up / down |

The hotkey and the speed keys are set in `Data\F4SE\Plugins\4Stim.ini`.

## Repository layout

```
src/                   F4SE plugin (C++23, CommonLibF4)
Data/                  Files that ship with the mod
  F4SE/Plugins/        4Stim.ini, scene, action, furniture and physics files
  Interface/4Stim/     HUD themes
  Scripts/Source/User/ Papyrus sources (FourStim, FourStimScene, FourStimMenu, FourStimTest)
Interface-src/         ActionScript 3 sources for the picker and HUD movies
tools/                 package-alpha.ps1 (test build archives)
docs/                  Scene format (SCENES.md) and HUD API (HUD_API.md)
lib/commonlibf4/       CommonLibF4 (git submodule)
```

## Building

### Plugin (DLL)

Requirements: [XMake](https://xmake.io) 3.0+ and a C++23 compiler (MSVC or Clang-CL).

```bat
git clone --recurse-submodules https://github.com/KeryoWolfe/4Stim
cd 4Stim
xmake build
```

The DLL lands in `build\windows\`. To have it copied straight into a mod manager or game folder, set `XSE_FO4_MODS_PATH` or `XSE_FO4_GAME_PATH` first. For Visual Studio, run `xmake project -k vsxmake`; for clangd, run `xmake project -k compile_commands`.

### Interface movies (SWF)

Requirements: the [Apache Flex SDK](https://flex.apache.org/) (playerglobal 11.0) and Java.

Set `FLEX_HOME` at the top of `Interface-src\build.bat`, then run it. The movies are written to `Interface-src\out\Interface\`; copy that folder into `Data\`.

### Papyrus

Compile the scripts in `Data\Scripts\Source\User\` with the Creation Kit's Papyrus compiler.

### Test builds

`tools\package-alpha.ps1` packages a test build: `4Stim-<version>-alpha.zip` (the framework) and `4Stim-TestAnims-<version>-alpha.zip` (the test pack), with the testers' read-me, in `release\`. Build the plugin, movies and scripts first. The top of the script says where it takes each file from; it stops if one is missing, and warns about anything older than its source:

```bat
powershell -ExecutionPolicy Bypass -File tools\package-alpha.ps1
```

## Credits

- [OStim NG](https://github.com/VersuchDrei/OStimNG) for the design this framework follows, the action types (`Actions\Default.json`) and the scene data used for testing
- [CommonLibF4](https://github.com/libxse/commonlibf4) and the [F4SE](https://f4se.silverlock.org/) team

## License

GPL-3.0 with the modding exception in [EXCEPTIONS](EXCEPTIONS). See [LICENSE](LICENSE).
