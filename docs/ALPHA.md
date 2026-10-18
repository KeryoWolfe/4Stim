# 4Stim 0.3.0 alpha: tester's guide

Thanks for testing. 4Stim is an OStim-style scene framework for Fallout 4: you pick someone (or several people), pick where, and a scene starts; from there you move between positions on a HUD, or let auto mode run it. This is the last alpha before beta: 4Stim now does what OStim NG does, so this round is about finding what breaks. What you report decides what gets fixed first.

**Please don't share these files.** The test animations are only for this test.

## What you need

- Fallout 4 **1.11.240** (the "Anniversary Edition" update), with [F4SE](https://f4se.silverlock.org/) and Address Library for F4SE Plugins for 1.11.240. Other game versions won't load the plugin (1.10.163 is still missing addresses: see "Older game versions").
- A mod manager (Mod Organizer 2 or Vortex).
- [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497), for the settings pages (optional: everything is also in `4Stim.ini`).
- Optional: A-Body (male) with FO4 Faster HDT-SMP. 4Stim swaps to the erect physics during scenes (and the receiving one for a man on the bottom).
- Optional: the High Heels System. Actors in heels are scaled or lowered to fit the animation.
- Optional: [Intiface Central](https://intiface.com/central/) and a supported toy, for sex toys (below).

**Use a separate save.** 4Stim moves NPCs and changes their equipment and AI for the length of a scene.

## Installing

Install both archives with your mod manager, as two mods:

1. **`4Stim-0.3.0-alpha.zip`**: the framework (plugin, settings, MCM page, HUD, icons, sounds, scripts).
2. **`4Stim-TestAnims-0.3.0-alpha.zip`**: the test animations and their scene file. Enable **`4StimTestAnims.esp`** in your load order.

**Updating:** replace both mods (don't merge them), and take the new `4Stim.ini`, copying your changes into it: there are new settings (sex toys, controller, playback speed). If you changed settings in the MCM, those stay.

## Playing

| Key | What it does |
|---|---|
| Up arrow | Out of a scene: start one. Aimed at someone in a scene: that scene's options (watch, auto mode, end). In a scene: give the HUD the keys, or hand them back. |
| Arrow keys / numpad 8 5 4 6 | Move through the picker and the HUD. |
| Enter / numpad 7 | Choose. |
| Numpad 9 | The HUD: between the list and the tab buttons. |
| Esc, Backspace | Back, or hand the keys back to the game. |
| Numpad + / - | Faster / slower. |
| Numpad 0 | Auto mode on or off for your scene. |
| Numpad 1, numpad ., K, L, numpad /, numpad * | Pull out, end the scene, search, Align tab, free camera, hide the HUD (OStim's keys). |
| `Shift` + Up arrow | Start a scene between NPCs, even while you're in one yourself. |

**Controller:** A chooses. With the **4Stim Modern** scheme (default), B goes from the list to the tab buttons (Up / Down pick a tab, A opens it), B again hides the HUD while the scene goes on, and A shows it again. **OStim Classic** (MCM: Controller page) uses OStim's navigation on the d-pad. The Controller page also gives any scene key a button (none by default).

The HUD fades out after 10 seconds without input and hands the keys back; any of its keys brings it back.

Starting a scene: press the Up arrow near someone. Add more people if the scene allows, pick where (right here, or a bed, chair, table or other furniture nearby), and an intro scene starts. Choose **You: not in it** to start a scene between NPCs and watch it; **Running scenes** lists every scene going on.

All settings are on 4Stim's MCM pages, and in `F4SE\Plugins\4Stim.ini` with a comment on each (edit it with the game closed). Settings OStim also has use OStim's names and defaults (`docs/SETTINGS.md`).

## What to test

New since 0.2.x, and the least tested:

- **The HUD's Utility tab → Actors:** each actor's page: undress / dress, mute, and for women their strap-on and tongue (on / off, and which one). This was broken before this build.
- **Strap-ons:** a woman (or a futa set up in the actor properties) in a role that needs a penis gets one, from the optional 4Stim Equip Objects add-on. Men never get the strap-on page.
- **Sex toys with Intiface Central:** start Intiface Central, connect your toy, start its server, then turn on **MCM → Sex toys → Use Intiface Central**. During your own scenes (not NPC-only ones), toys follow sex actions: a steady strength, a pulse on each thrust or stroke, a strong pulse at climax. `4Stim.log` lists the devices found (`Intiface:`) and which action each toy follows (`Sex toys:`). A connected controller counts as a toy too.
- **Undressing:** "Remove her chest armor / outfit" in undressing scenes, and the outfit coming back after.
- **Weapons:** whatever you or an NPC held before a scene is equipped again after it.
- **Physics (A-Body):** erect physics in scenes, the "bottom" physics for a man on the receiving end, and nothing left bent or stretched after a scene ends.
- **Scene sounds** staying where the scene is (walk away from an NPC scene: they should fade).
- **Playback speed:** scenes whose speeds set OStim's `playbackSpeed` should play faster or slower (`bUsePlaybackSpeed`).
- **MCM pages:** every page shows, changes apply at once, and the hotkey pages show the keys.
- **Saving and loading** mid-scene, and the strap-on / tongue you chose for someone staying chosen after a load.

Still worth checking from before: NPCs who were busy (sitting, working) dropping it for a scene; beds against walls; excitement and climax; auto mode; several scenes at once; the Align tab; your body and outfit mods.

## Known problems

- **Freezes with FO4 Faster HDT-SMP:** its Papyrus functions (`DynamicHDT`) can stop the game for seconds at a time, longer with many SMP actors around. A-Body's own NPC script calls them while you walk around; 4Stim's physics swaps call them at scene start and end. If you get long freezes, check `PapyrusMonitor.log` if you use Papyrus Monitor, and try without A-Body's NPC physics; report it to FSMP's author too.
- NPCs in scenes still talk (idle chatter, greetings): 4Stim's own dialogue isn't in yet.
- The test animations have no `4StimEvent` annotations, so there are no spanks in them yet.
- Toys only follow scenes with you in them (as OStim's), and only actions with a toy slot.
- Lights are game lights moved with the actor every frame; if one lags or flickers, send the log (`Lights:` lines).

## Older game versions

1.10.163 (before the next-gen update) **doesn't load** in this build: 4Stim's own address table for it still lacks 34 of the game addresses 4Stim needs, and the Runtime Database mapping we tried doesn't contain them (`docs/OLDGEN.md`). `4Stim.log` lists what's missing. No need to send more mapping files. 1.10.984 (next-gen) is untested.

## Reporting a problem

For each problem, send:

1. **What you did and what happened**, step by step, and what you expected instead.
2. **A screenshot**, if it's something you can see.
3. **Both logs, right after it happens** (they're replaced every time the game starts):
   - `Documents\My Games\Fallout4\F4SE\4Stim.log`
   - `Documents\My Games\Fallout4\Logs\Script\Papyrus.0.log`. If it isn't there, set `bEnableLogging=1` and `bEnableTrace=1` under `[Papyrus]` in `Fallout4Custom.ini`.
4. **Your load order** (or at least the body, physics, animation and AI mods you use).

If the game crashes, also send the newest crash log from your crash logger (Buffout 4, Addictol or similar).
