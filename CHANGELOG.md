# Changelog

## Unreleased

- **OStim's settings:** `4Stim.ini` now uses OStim NG's setting names and defaults (docs/SETTINGS.md); the older names still work. Changed defaults, to OStim's: post-climax excitement 10 / 30, auto mode 7.5-15 s per scene, pull-out up to 90, 5 navigation steps, auto speed every 2.5-7.5 s between 15 and 85 excitement, furniture search 1600 units, no redress animations, clothes off all at once, and scenes in a save end on load (`bResumeScenes=1` starts them again).
- **New, from OStim:** slow motion at a climax, fades to black as scenes with you start and end, the free camera's FOV during scenes, first person after scenes, a free camera switch, NPC scenes ending after 5 minutes, and keys for pull out, end scene, search, the Align tab, the free camera and hiding the HUD (unbound by default).

## 0.1.0 alpha (October 2026)

First test build, for private testers (see `docs/ALPHA.md`).

- **Scenes** from JSON files: solo, pair and group scenes, speeds, navigation between scenes, transitions and sequences.
- **Starting scenes like OStim's:** the hotkey lists who's near you, then where (here, or nearby furniture: beds, chairs, tables, mattresses...), then an idle scene. Scenes between NPCs only, and several scenes at once, each watchable from the picker.
- **Furniture:** scenes placed on beds, chairs, tables and mattresses, avoiding a bed's side against a wall.
- **In-scene HUD:** navigation, scene icons, actor and speed meters, themes, keyboard and gamepad.
- **Actions:** who does what to whom in each scene (OStim's action types); roles go to actors who fit them.
- **Excitement and climax:** meters driven by each scene's actions, climax at 100 with climax animations, screen effects and rumble, and scene-end rules.
- **Auto mode:** scenes move on by themselves (foreplay, intercourse, pull-out), faster with excitement.
- **Undressing:** what each scene needs comes off, one piece at a time, everything optionally at the start, and it all goes back on at the end. Undress / Dress anyone in the scene by hand from the HUD's Utility tab. Animated undressing scenes, and redress animations after a scene (OStim's, in the test pack).
- **Scene lock and alignment:** everyone in a scene is locked on its spot (nothing can push them off); the HUD's Align tab moves, turns and scales each actor per scene and role, saved to `Alignment.json`.
- **Saving mid-scene:** running scenes are kept in the save and start again when it's loaded.
- **NPC scene key** (Shift + `N` by default): start a scene between NPCs even while you're in one.
- **Scene guard:** NPCs are taken out of furniture and work spots when a scene starts, and kept on the scene's animations.
- **Physics swap** for A-Body's SMP during scenes.
- **Papyrus API and events** for other mods: start / change / speed / end / climax events, actions, excitement, auto mode and undressing calls.
