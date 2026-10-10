# To do

## V1.0 cleanup (before release)

- **Short hitch when picking a transition.** Choosing a scene that starts a transition freezes the game very briefly. It's likely the game loading the transition's animation file on first play; worth measuring (time from the pick to `PlayIdle` returning in the log) and, if so, preloading the clips of a scene's transitions while it plays.

- **Scene events' actor arrays.** Actors passed from native code to Papyrus arrived as a "type mismatch" in the physics swap (fixed there by passing form IDs). The scene events (`FourStim_OnSceneStart` and the rest) pass `Actor[]` the same way; check them with a listening script, and pass form IDs if they're broken too.

- **Stuck loading screen on scene start (watch for it).** Once, a second scene in a row left the game on a black loading screen with the spinner while the scene itself ran. Likely cause: the pair placement teleported the player with Papyrus `SetPosition` (which can start a load) while the script engine was swamped by another mod, and the scene camera switched to the free camera during that load. Fixed by placing pairs natively and holding the free camera until loading screens and fades clear; confirm it doesn't come back.
- **No idle chatter during scenes.** NPCs in a scene still talk: idle lines, greetings and comments to each other. Silence scene actors (and keep bystanders from greeting them) for the scene's length; `SuppressInteraction` only blocks the player's Activate / Talk.
- **No animation markers mid-scene.** NPCs in a scene can be pulled off by their AI packages to use furniture or idle markers (sandboxing), snapping out of the scene. Keep their AI from picking markers or furniture until the scene ends. *Started: NPCs are taken out of any furniture or idle marker when a scene starts and given a do-nothing package, and the scene guard (bGuardScenes) takes them out again and replays the scene if they get back in or a game idle takes over. Needs testing.*
- **Lock actors in place.** Scene actors get pushed around by other NPCs walking into them and slide on uneven terrain. Hold each actor at its scene position and heading for the whole scene (no collision pushes, no slope sliding, no gravity drift).

- **Penis position adjustments per animation.** With rigid SMP genitals (physics swaps), some animations need the penis angle or offset adjusted per scene or role so it lines up with the partner. Pinned for later.

## OStim parity

What OStim NG has that 4Stim doesn't yet, from a read of its source (October 2026). Roughly in priority order within each group.

### Core
- ~~**Actions.**~~ Done: action types in `Actions\*.json`, `actions` in scene files, requirements in role matching, Papyrus queries (docs/ACTIONS.md). The animation converter (Separate projects) should copy OStim scenes' `actions` (the test pack's were backfilled from OStim's scene files).
- ~~**Excitement and climax.**~~ Done (docs/EXCITEMENT.md). Left for later: climax sounds and moans (with Sound, from the OStim sounds now in 4Stim Core\Sound), partner reactions, climax annotations in animations (OStim triggers the climax from the climax animation; 4Stim climaxes when the animation starts), slow motion / blur options.
- ~~**Auto mode.**~~ Done (docs/AUTOMODE.md): stages, routes through navigations, auto speed, pull-out, toggle key, Papyrus. With it, these scene fields: `defaultSpeed`, `noRandomSelection`, actor `tags`, actor `requirements`, `intendedSex`, `autoTransitions` (any event).
- ~~**Concurrent scenes (threads).**~~ Done: any number of scenes at once, tracked natively; NPC-only scenes from the picker ("You: not in it"); a "Running scenes" list and the hotkey on someone in a scene to watch (free camera + HUD, "Stop watching"), switch auto mode or end it; one scene per actor (`FourStim.IsInScene`, checked by `CanUseActor`). Still to do: starting an NPC scene while you're in your own (a second hotkey), and scenes surviving a save / load (see Save data).
- ~~**Undressing.**~~ Done (docs/UNDRESS.md): at start (optional), partial per action (`undressSlots`, mapped from OStim's), full for `fullStrip` actions, `noStrip` scenes, a slot list setting, redress at the end, Papyrus `UndressActor` / `RedressActor`. Weapons were already put away. Still to do: animated undressing / redressing (OStim's redress animations), an HUD option to undress or dress someone by hand.

### Presentation
- **Alignment menu.** Live per-actor offsets (x, y, z, rotation, scale, penis bend) per scene and role, saved to JSON and keyed by sex / height / heels. Covers the penis-position pin above.
- **Actor scaling and heels.** Scale actors to the animation's intended height (`scale`, `scaleHeight`), and remove or compensate heel offsets.
- **Facial expressions.** Expression sets (default, excited, kissing, moan, climax, open mouth...) with phonemes, eyes and brows, by action and excitement; `look` directions and `expressionOverride` per scene.
- **Sound.** Voice sets per actor / voice type (moans, muffled moans, climax sounds, comments, reactions, post-scene lines), action sounds (slaps on peaks), mute / muffle per actor or action.
- **Peaks and events.** Distance- or loop-based peak signals per action, and annotation events (spank...) with sound, stimulation, camera shake and rumble.
- **Lights.** Optional face / crotch light during scenes, only in the dark.
- **Camera extras.** Freecam speed and FOV, first person after the scene, fades on start and on scene changes, screen shake.

### Actors and roles
- **Equip objects, strap-ons and futa (one system).** Strap-ons equipped when a woman takes a role that needs a penis, plus tongues and other per-scene items, removed at the end. **Futa is merged into strap-ons:** in Fallout 4 futa comes from equippable penises with two states (flaccid / erect), not one built into the body mesh, so they can't be posed or bent like a male body's. They're handled like a strap-on: an actor wearing one has a `penis` for action requirements (`Actions::Provides`), with options for the male role, male excitement and male climax.
- **Role choice.** Intended-sex-only filter (have it: `bMatchSex`), player always dominant or submissive, choose your role when several fit.
- **Actor requirements and properties.** Per-role requirements (race, vampire / ghoul / super mutant, has a penis...), actor tags, perk-conditioned properties. Covers "Race filtering" below.

### Furniture
- Done: edge-anchored furniture (beds, tables, benches) avoids a side against a wall (`checkWalls`, FURNITURE.md).
- Change furniture mid-scene, reset displaced clutter afterwards, auto-use the nearest furniture, a bed confirmation, more types (wall, shelf, chemistry / armor / power armor stations).

### Scene format
- Fields OStim has and 4Stim doesn't yet, each to come with its feature: `scale` / `scaleHeight` and `feetOnGround` (actor scaling), `look*` (expressions), `muted` (sound), and OStim's `sosBend` as **`penisBend`** (alignment). In Fallout 4 the penis is part of the male body mesh (A-Body, BodyTalk, Atomic Muscle...), not a separate Schlongs of Skyrim item, so 4Stim names it `penisBend`; the loader should also read `sosBend` from OStim-converted files as an alias. Done: `actions`, `noStrip`, `defaultSpeed`, `noRandomSelection`, actor `tags`, `requirements`, `intendedSex`, `autoTransitions`.

### For other mods
- **Papyrus API.** A thread builder (actors, furniture, start scene or sequence, flags: no auto mode, no undress...), scene metadata queries, random scene by tags (OLibrary), per-actor data.
- **More events.** Furniture changed, NPC scene start / end, sequence end (below). (Climax is done: `FourStim_OnClimax`.)
- **A C++ plugin interface** for other F4SE plugins (start / stop, listeners, alignment).
- **Save data.** Per-actor choices (voice set, equip objects, alignment) in the F4SE co-save; clean up scenes left running on load.

### Settings and UI
- **MCM page** (Fallout 4's Mod Configuration Menu) for every INI setting plus hotkeys, with export / import. Include a setting for each climax effect (shake, blur, edge glow strength and size, rumble), so players can tune them in game.
- **Hotkeys:** end scene, auto mode, pull out, hide UI, NPC-only scene start.
- **Options in the HUD:** per-actor toggles (undress, strap-on, mute...).
- **Translations** for the menu and HUD text.
- **Sex toys** (device integration). Optional, last.

## Later

- **Automatic mesh detection.** Detect which body mesh each actor uses and its size (the body, its SMP build and BodySlide sliders), and adjust scene positions to match, so bigger or smaller bodies line up with their partners without hand alignment. Bodies to tell apart:
    - Female: TWB, CBBE, JaneBod, Fusion Girl, Atomic Beauty.
    - Male: A-Body, BodyTalk, Super Hero Bodies, Muscular Body, Atomic Muscle.
    - Both: Enhanced Vanilla Bodies.
- **In-game HUD editor.** Lets theme makers rearrange the HUD in game: drag the navigation list, tabs, logo, actor meters and speed meter where they want, resize them, then save the layout to a file they can put in their theme (`Data\Interface\4Stim\Themes\`, see HUD_API.md), so the theme ships with that positioning.
- **YAML scene files.** Let animation authors write their scene files (scenes and sequences, `Scenes\`) as either JSON or YAML (`.yaml` / `.yml`), whichever they prefer, with the same fields; both load side by side from the same folder. YAML is easier to write by hand: comments, no quotes or trailing-comma errors.
- **OG and NG versions.** Builds for Fallout 4 1.10.163 (OG) and 1.10.984 (NG), alongside 1.11.240 (AE). The only 1.10.163 CommonLibF4 is libxse's frozen 2023 branch, an older API (CMake / vcpkg, `stl::` instead of `REX::`), so the plugin's code needs porting to it. Missing calls (`StopInteractingQuick`, `InitiateDoNothingPackage`) need adding, and every struct 4Stim reads directly (process data, camera, inventory) checked against each runtime's layout. Needs testers on each version.
- **Runtime Database version.** One plugin for OG, NG and AE that also survives future game updates, built on [CommonLibF4RD](https://github.com/Zzyxz/CommonLibF4RD) for [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394), which finds the game's functions when the game starts instead of using a fixed address table per version. The same older CommonLibF4 API as the OG build (port once, covers both), still with per-runtime struct layouts to check (its docs say it doesn't make class layouts compatible), and players need Runtime Database installed. Fairly new (released August 26, 2026; CommonLibF4RD still updated in October): check how it's held up before switching. If it works out, it replaces separate OG / NG builds.
- NPC greetings
- Race filtering
- Claim registry
- First-person camera
- A sequence-end event for other mods

## Separate projects

Tools and add-on modules built alongside 4Stim, outside the core plugin.

- **Animation converter, rebuilt from scratch.** One tool that ports animations both ways, Skyrim to Fallout 4 and Fallout 4 to Skyrim, with a proper bone map and retargeting between the two skeletons and the right Havok output for each game (FO4's hk_2014, Skyrim SE's 64-bit hk_2010 packfiles), keeping annotations and root motion. It builds each game's plugin: FO4 IDLE records one-shot (`dyn_Activation`) for transitions and looping (`dyn_ActivationLoop`) otherwise (the current converter makes every idle looping, and regenerating the test pack's esp undoes the hand fix). It holds the last pose of transition clips (what `hold_end.py` does now, as an option instead of a separate step), and writes 4Stim scene files that keep OStim's `actions`, actor `tags`, `autoTransitions` and the rest.
- **Sim Settlements 2 furniture module.** A 4Stim module that finds the furniture inside Sim Settlements 2 plots (beds, chairs, tables... placed by the plots' building plans rather than the workshop), so 4Stim's furniture detection and placement work on it like on ordinary furniture.
- **Placeable 4Stim animation markers module.** Markers the player can place (like the base game's animation markers / idle markers) where NPCs start 4Stim scenes by themselves, for example on their sandbox routine, with settings on each marker for which scenes or tags, who can use it and how often.
