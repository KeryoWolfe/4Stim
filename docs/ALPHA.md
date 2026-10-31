# 4Stim 0.4.1 alpha: tester's guide

Thanks for testing. 4Stim is an OStim-style scene framework for Fallout 4: you pick someone (or several people), pick where, and a scene starts; from there you move between positions on a HUD, or let auto mode run it. This alpha fixes what the 0.3.0 testers found (below). The next build is the beta, so this round is about confirming those fixes and finding what still breaks. What you report decides what gets fixed first.

**Please don't share these files.** The test animations are only for this test.

## What you need

- Fallout 4 **1.11.240** (the "Anniversary Edition" update), with [F4SE](https://f4se.silverlock.org/) and Address Library for F4SE Plugins for 1.11.240.
- Or, experimental: **1.10.163** (before the next-gen update, F4SE 0.6.23), **1.10.984** (next-gen) or another 1.11 version, with [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394) installed instead of the Address Library. See "Other game versions".
- A mod manager (Mod Organizer 2 or Vortex).
- [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497), for the settings pages (optional: everything is also in `4Stim.ini`).
- Optional: A-Body (male) with FO4 Faster HDT-SMP. 4Stim swaps to the erect physics during scenes (and the receiving one for a man on the bottom).
- Optional: [LooksMenu](https://www.nexusmods.com/fallout4/mods/12631), for erections on BodyTalk and on Atomic Muscle's `.tri` install (morphs). Atomic Muscle's bodysuit install and A-Body need nothing more.
- Optional: the High Heels System. Actors in heels are scaled or lowered to fit the animation.
- Optional: [Intiface Central](https://intiface.com/central/) and a supported toy, for sex toys (below).

**Use a separate save.** 4Stim moves NPCs and changes their equipment and AI for the length of a scene.

## Installing

**Start clean.** Remove every older 4Stim first: disable and delete the old 4Stim mods in your mod manager, and if you ever copied 4Stim into the game's `Data` folder by hand, delete `Data\F4SE\Plugins\4Stim.dll`, the `Data\F4SE\Plugins\4Stim` folder, `Data\Scripts\FourStim*.pex` and `Data\Scripts\Four*.pex`. An old `4Stim.dll` left behind is the most common reason the scene key does nothing (see "Is it the new build?").

Then install the archives with your mod manager, one mod each:

1. **`4Stim-0.4.1-alpha.zip`**: the framework (plugin, settings, MCM page, HUD, icons, sounds, compiled scripts).
2. **`4Stim-TestAnims-0.4.1-alpha.zip`**: the test animations and their scene file. Enable **`4StimTestAnims.esp`**.
3. Optional, **`4Stim-EquipObjects-0.4.1-alpha.zip`**: strap-ons and tongues. Enable **`4Stim - Equip Objects.esp`**, then build **4Stim OStim Strap-On (TWB)** in BodySlide (the tongues need no building).

Take the new `4Stim.ini` (don't keep your old one): the undressing slots moved to their own section. Settings you changed in the MCM stay.

### Is it the new build?

The top of `Documents\My Games\Fallout4\F4SE\4Stim.log` says:

- `4Stim v0.4.1.0`. If it says `v0.4.0.0` or `v0.3.0.0`, an old `4Stim.dll` is still being loaded.
- On 1.10.163 and the other versions: the places it looked for the Runtime Database (`Addresses: Runtime Database at ...`), then `Addresses: ... found, the other ... done without`. A list of red `[E] ... not found` lines ending in `of 107 missing` is the old build.
- `4Stim loaded`. If the log stops before it, send the log.

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

Fixed in this build, each from a tester's report. Please check whether it's gone:

- **An actor missing from a scene** (worst in wall scenes, and in first person): they should stay visible. If it happens, send the log (`Scene guard: ... keeps being faded out`).
- **An NPC barely following scene changes, or turned sideways in a chair scene**: this came from NPCs who had drawn a weapon before the scene. Draw your weapon, or start a scene with a companion who just fought, and check everyone follows (`Weapons: ... puts their weapon away`).
- **Actors shaking in chair scenes**, especially near a desk or a wall. If it still happens, send the log: the `Collision watch:` lines say what they're touching.
- **Kisses lining up**: head tracking is now off in scenes, so heads follow the animation instead of turning to look at someone.
- **The penis left at an odd angle after a scene**, or its physics flopping about after: it's put back after every scene now.
- **NPCs walking off mid-scene** (settlers going to their work spot, people sandboxing): they should stay until the scene ends, then go back to what they were doing (`Scene package:`).
- **Clothes coming back on mid-scene**: whatever came off should stay off until the scene ends. The undressing slots are now OStim's, on the MCM's Undressing page.
- **Animation packs in the scene list**: another pack's scenes (Atomic Lust, for example) now show as one row named after the pack, which opens just those.
- **A-Body erect physics**: if a man's physics doesn't change in a scene, send the log (`Physics:` lines).
- **Erections (new in 0.4.1)**: men should be erect during scenes and flaccid after, on BodyTalk (with LooksMenu), Atomic Muscle (either install) and A-Body. The log says what each man's body got (`Erection: ... body:`); send it if one stays flaccid or stays erect after the scene. Setting the Align tab's penis bend to -10 should make him flaccid mid-scene (A-Body: bent all the way down instead, since its penis is rigid while the scene's physics are off).
- **Clothes not coming off**: 4Stim undresses the way OStim does by default: only what each position needs, as the scene gets there (gloves for groping, everything for sex). To have everyone undress when the scene starts, turn on **Undress at the start** on the MCM's Undressing page. If something stays on that should come off, say which position and which clothes (`Undress:` lines in the log).

Still worth checking from 0.3.0: sex toys with Intiface Central, the Utility tab's actor pages (undress, mute, strap-on, tongue), weapons coming back after a scene, saving and loading mid-scene, several scenes at once, the Align tab, and your own body and outfit mods.

## Known problems

- **Freezes with FO4 Faster HDT-SMP:** its Papyrus functions (`DynamicHDT`) can stop the game for seconds at a time, longer with many SMP actors around. A-Body's own NPC script calls them while you walk around; 4Stim's physics swaps call them at scene start and end. If you get long freezes, check `PapyrusMonitor.log` if you use Papyrus Monitor, and try without A-Body's NPC physics; report it to FSMP's author too.
- NPCs in scenes still talk (idle chatter, greetings): 4Stim's own dialogue isn't in yet.
- The test animations have no `4StimEvent` annotations, so there are no spanks in them yet.
- Toys only follow scenes with you in them (as OStim's), and only actions with a toy slot.
- Lights are game lights moved with the actor every frame; if one lags or flickers, send the log (`Lights:` lines).
- Clipping: a penis through bodies or faces, hands through bodies. Most of it is in the animations themselves (the converted test animations don't move the penis bones at all); use the Align tab to fix a scene for your characters, and tell us which scenes are worst.
- First person in scenes isn't supported yet: scenes switch to third person.

## Other game versions

The same 4Stim works on 1.10.163, 1.10.984 and other 1.11 versions through [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394) (install it like any mod; it puts `f4rd-runtime.bin` in `F4SE\Plugins`). 4Stim finds the game's code with it when the game starts; that first start can take a little longer. This is **experimental**: on 1.10.163 a tester has started scenes with the 0.3.0 hotfixes, but it's far less tested than 1.11.240, so we'd like to know:

- Does 4Stim load? `4Stim.log` starts with `Addresses: CommonLibF4RD's Runtime Database`, then `Addresses: all ... found`, or lists what it couldn't find (send that). On 1.10.163 it's normal to see up to 12 warnings ending in what 4Stim does instead, then `Addresses: ... found, the other ... done without`: those are handled (docs/OLDGEN.md).
- On 1.10.163: do the MCM's texts show (not `$4Stim_...`), and do scene sounds (moans, slaps) play?
- Do scenes start, with actors in place and animating, and does the HUD show?
- Anything that works on 1.11.240 but not there.

## Reporting a problem

For each problem, send:

1. **What you did and what happened**, step by step, and what you expected instead.
2. **A screenshot**, if it's something you can see.
3. **Both logs, right after it happens** (they're replaced every time the game starts):
   - `Documents\My Games\Fallout4\F4SE\4Stim.log`
   - `Documents\My Games\Fallout4\Logs\Script\Papyrus.0.log`. If it isn't there, set `bEnableLogging=1` and `bEnableTrace=1` under `[Papyrus]` in `Fallout4Custom.ini`.
4. **Your load order** (or at least the body, physics, animation and AI mods you use).

If the game crashes, also send the newest crash log from your crash logger (Buffout 4, Addictol or similar).
