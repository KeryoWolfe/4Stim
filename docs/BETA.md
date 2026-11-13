# 4Stim 0.5.0 beta: tester's guide

Thanks for testing. 4Stim is an OStim-style scene framework for Fallout 4:
you pick someone (or several people), pick where, and a scene starts; from
there you move between positions on a HUD, or let auto mode run it. The beta
is everything OStim NG does that Fallout 4 allows (DECISIONS.md lists where
4Stim differs and why), with the alpha testers' problems fixed. This round is
about finding what still breaks before the first public release.

**Please don't share these files.**

## What you need

| | |
|---|---|
| **Required** | Fallout 4 **1.11.240** (Steam), [F4SE](https://f4se.silverlock.org/) for it, Address Library for F4SE Plugins for 1.11.240, a mod manager (Mod Organizer 2 or Vortex) |
| **Experimental** | 1.10.163, 1.10.984 or another 1.11 version, with [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394) instead of the Address Library ("Other game versions") |
| **Recommended** | [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497) (the settings pages; everything is also in `4Stim.ini`); a crash logger (Addictol) |
| **Optional** | A-Body (male) with FO4 Faster HDT-SMP (rigid physics in scenes); [LooksMenu](https://www.nexusmods.com/fallout4/mods/12631) (erections on BodyTalk and Atomic Muscle's `.tri` install); the High Heels System; a skeleton with ZeX's penis bones (penis bend and aim); True Wasteland Body (for the strap-on); [Intiface Central](https://intiface.com/central/) and a toy |

The full list, with what each is for: DEPENDENCY_MATRIX.md. No versions are
required: use the latest of each.

**Use a separate save** (better: a separate mod manager profile). 4Stim
moves NPCs and changes their equipment and AI for the length of a scene.

## Installing

**Remove every older 4Stim first**, and anything of it copied into `Data`
by hand (`F4SE\Plugins\4Stim.dll`, the `F4SE\Plugins\4Stim` folder,
`Scripts\Four*.pex`). An old `4Stim.dll` left behind is the most common
reason the scene key does nothing.

Then install with your mod manager, one mod each:

1. **`4Stim-0.5.0-beta.zip`**: the framework. Enable **`4Stim.esl`**.
2. **`4Stim-TestAnims-0.5.0-beta.zip`**: OStim's base animations, converted. Enable **`4StimTestAnims.esp`**.
3. Optional, **`4Stim-EquipObjects-0.5.0-beta.zip`**: strap-ons and tongues. Enable **`4Stim - Equip Objects.esp`** and build **4Stim OStim Strap-On (TWB)** in BodySlide.

Take the new `4Stim.ini`. Settings you changed in the MCM stay.

**Is it the new build?** `Documents\My Games\Fallout4\F4SE\4Stim.log`
starts with `4Stim v0.5.0.0`, `Game version …, F4SE …` and, a few lines on,
`4Stim loaded`.

## Uninstalling

End any scene, save, disable and remove the three mods, and load that save.
4Stim keeps nothing in the save that the game needs; the Papyrus log may
mention 4Stim's scripts being missing, which is harmless.

## Playing

| Key | What it does |
|---|---|
| Up arrow | Out of a scene: start one (hold 2 seconds: alone). Aimed at someone in a scene: that scene's options. In a scene: give the HUD the keys, or hand them back. |
| `Shift` + Up arrow | Start a scene between NPCs, even while you're in one. |
| Arrow keys / numpad 8 5 4 6 | Move through the picker and the HUD. |
| Enter / numpad 7 | Choose. |
| Numpad 9 | The HUD: between the list and the tab buttons. |
| Esc, Backspace | Back, or hand the keys back to the game. |
| Numpad + / - | Faster / slower. |
| Numpad 0 | Auto mode on or off. |
| Numpad 1, numpad ., K, L, numpad /, numpad * | Pull out, end the scene, search, Align tab, free camera, hide the HUD (OStim's keys). |

**Controller:** A chooses; with the **4Stim Modern** scheme (default) B goes
to the tab buttons, then hides the HUD. **OStim Classic** (MCM, Controller
page) uses OStim's navigation on the d-pad.

Every setting is on 4Stim's MCM pages and in `F4SE\Plugins\4Stim.ini`.
Settings OStim also has use OStim's names and defaults (docs/SETTINGS.md).

## What to test

TEST_PLAN.md is the full list. Most wanted:

- **Starting and ending, again and again**: ten scenes in a row with the same NPC. Nobody should be left frozen, small, undressed, unable to move or unwilling to talk.
- **Save and load**: save during a scene and load it. Load another save during a scene. Everyone should be released and dressed.
- **Leaving**: start a scene between NPCs and walk or fast-travel away, then come back.
- **Ending by itself**: hurt someone in a scene, or let a fight start near one.
- **Your bodies and outfits**: erections, undressing and redressing with your body, outfit and physics mods.
- **Who gets which role**: men, women, and futa from a wearable penis. Each should get the scenes they fit; with **Intended gender only** off (MCM, Gender Roles), two women get strap-on scenes.
- **The HUD and MCM**: every tab, every page, with keyboard and controller.
- **Several scenes at once**, NPC-only scenes, auto mode.

## Known problems

- **A short hitch the first time a scene plays** (up to about a tenth of a second): the game loading the animation. Scenes played before don't hitch.
- **Clipping**: a penis through bodies or faces, hands through bodies, mostly in the animations themselves. Use the Align tab to fix a scene for your characters, and tell us which scenes are worst.
- **First person** in scenes isn't supported yet: scenes switch to third person.
- **Freezes with FO4 Faster HDT-SMP**: its Papyrus functions can stop the game for seconds, more with many SMP actors around. 4Stim's physics swaps call them at scene start and end; A-Body's NPC script calls them all the time.
- **Toys** only follow scenes you're in (as OStim's), and only actions with a toy slot.
- A wearable penis that isn't on ZeX's penis bones (older mods) can't be aimed by the scene; at large sizes it clips.

## Other game versions

The same 4Stim runs on 1.10.163, 1.10.984 and other 1.11 versions through
[Runtime Database](https://www.nexusmods.com/fallout4/mods/108394). This is
**experimental** (docs/OLDGEN.md). Please tell us whether 4Stim loads (the
log lists what it couldn't find, if anything), whether the MCM's texts and
the scene sounds work, and whether scenes start with everyone in place.

## Reporting a problem

For each problem, send:

1. **What you did and what happened**, step by step, and what you expected.
2. **A screenshot**, if it's something you can see.
3. **The logs, right after it happens** (they're replaced when the game starts):
   - `Documents\My Games\Fallout4\F4SE\4Stim.log`
   - `Documents\My Games\Fallout4\Logs\Script\Papyrus.0.log` (if it isn't there: `bEnableLogging=1` and `bEnableTrace=1` under `[Papyrus]` in `Fallout4Custom.ini`)
   - after a crash, the newest crash log (Addictol, or your crash logger)
4. **Your load order**, or at least your body, physics, animation and AI mods.
