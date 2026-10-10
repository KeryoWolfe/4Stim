# 4Stim 0.1.0 alpha: tester's guide

Thanks for testing. 4Stim is an OStim-style scene framework for Fallout 4: you pick someone (or several people), pick where, and a scene starts; from there you move between positions on a HUD, or let auto mode run it. This is an **alpha**: things will break, and what you report decides what gets fixed first.

**Please don't share these files.** The test animations are only for this test.

## What you need

- Fallout 4 **1.11.240** (the "Anniversary Edition" update). Older or newer game versions won't load the plugin.
- [F4SE](https://f4se.silverlock.org/) for 1.11.240.
- Address Library for F4SE Plugins, for 1.11.240.
- A mod manager (Mod Organizer 2 or Vortex).
- Optional: A-Body (male) with its SMP physics. 4Stim swaps to the erect physics during scenes when it's there.

**Use a separate save.** 4Stim keeps the scenes running when you save in that save, and moves NPCs and changes their equipment and AI for the length of a scene.

## Installing

Install both archives with your mod manager, as two mods:

1. **`4Stim-0.1.0-alpha.zip`**: the framework (plugin, settings, HUD, icons, scripts).
2. **`4Stim-TestAnims-0.1.0-alpha.zip`**: the test animations and their scene file. Enable **`4StimTestAnims.esp`** in your load order.

If you're updating from an earlier test build, replace both mods (don't merge them), and remove any older `4StimTestAnims.json` that's left in another mod. The newest one has to win.

## Playing

| Key | What it does |
|---|---|
| `N` | Out of a scene: start one. Aimed at someone in a scene: that scene's options (watch, auto mode, end). In a scene: give the HUD the arrow keys, or hand them back. |
| Arrow keys / d-pad | Move through the picker and the HUD. |
| Enter / A | Choose. |
| Esc, Backspace / B | Back, or hand the keys back to the game. |
| `=` / `-` (gamepad Y / X) | Faster / slower. |
| `U` | Auto mode on or off for your scene. |
| `Shift` + `N` | Start a scene between NPCs, even while you're in one yourself. |

Starting a scene: press `N` near someone. Add more people if the scene allows, pick where (right here, or a bed, chair, table or mattress nearby), and an idle scene starts. Choose **You: not in it** to start a scene between NPCs and watch it; **Running scenes** lists every scene going on, where you can watch one, switch its auto mode or end it.

Every key and option is in `F4SE\Plugins\4Stim.ini`, with a comment on each. Edit it with the game closed. Settings OStim also has use OStim's names and defaults (`docs/SETTINGS.md`), and OStim's other scene keys (pull out, end scene, search, align, free camera, hide HUD) are there, unbound.

## What to test

Anything goes, but these are new and the least tested:

- **NPCs who were busy.** Settlers working a plot, sitting, leaning, using a workbench: do they drop what they're doing and stay in the scene, without props in their hands?
- **Beds against a wall.** Does the scene use the open side of the bed?
- **Undressing.** Do clothes come off as the scene needs them, and go back on when it ends?
- **Excitement and climax.** Do the meters rise, does climax happen, and does the scene end the way the ini says it should?
- **Auto mode.** Does it move through positions sensibly and end on its own?
- **Several scenes at once,** with and without you in them, including starting one with `Shift` + `N` while you're in your own.
- **Saving and loading mid-scene.** As in OStim, scenes running when you save end when the save loads, and everyone in them is let go and dressed again. With `bResumeScenes=1` they start again a few seconds after the save loads instead.
- **Undressing by hand:** Undress / Dress for each actor in the HUD's Utility tab.
- **The Align tab** in the HUD: move, turn and resize each actor in a scene (docs/ALIGNMENT.md). Does it stay fixed the next time that scene plays? Does anyone get pushed off their spot during a scene?
- **Animated undressing:** from Standing apart (MF), the "take off her top / bottoms / gloves / hat / boots" options. And after a scene, NPCs dressing a body part at a time with redress animations.
- **OStim's scene feel:** the fade to black as a scene with you starts and ends, slow motion at a climax, the camera's field of view in scenes, and NPC scenes ending on their own after 5 minutes.
- **Your body and outfit mods.** Anything that doesn't line up, clips badly or doesn't undress.

## Known problems

- NPCs in scenes still talk (idle chatter, greetings).
- Picking a transition can hitch for a moment the first time.
- No sounds or facial expressions yet.
- Strap-ons aren't in yet, so roles that need a penis only go to men.

## Reporting a problem

For each problem, send:

1. **What you did and what happened**, step by step, and what you expected instead.
2. **A screenshot**, if it's something you can see.
3. **Both logs, right after it happens** (they're replaced every time the game starts):
   - `Documents\My Games\Fallout4\F4SE\4Stim.log`
   - `Documents\My Games\Fallout4\Logs\Script\Papyrus.0.log`. If it isn't there, set `bEnableLogging=1` and `bEnableTrace=1` under `[Papyrus]` in `Fallout4Custom.ini`.
4. **Your load order** (or at least the body, physics, animation and AI mods you use).

If the game crashes, also send the newest crash log from your crash logger, if you use one (Buffout 4 or similar).
