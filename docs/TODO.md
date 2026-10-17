# To do

## V1.0 cleanup (before release)

- **Short hitch when picking a transition.** Choosing a scene that starts a transition freezes the game very briefly. It's likely the game loading the transition's animation file on first play; worth measuring (time from the pick to `PlayIdle` returning in the log) and, if so, preloading the clips of a scene's transitions while it plays.

- **Scene events' actor arrays.** Actors passed from native code to Papyrus arrived as a "type mismatch" in the physics swap (fixed there by passing form IDs). The scene events (`FourStim_OnSceneStart` and the rest) pass `Actor[]` the same way; check them with a listening script, and pass form IDs if they're broken too.

- **Stuck loading screen on scene start (watch for it).** Once, a second scene in a row left the game on a black loading screen with the spinner while the scene itself ran. Likely cause: the pair placement teleported the player with Papyrus `SetPosition` (which can start a load) while the script engine was swamped by another mod, and the scene camera switched to the free camera during that load. Fixed by placing pairs natively and holding the free camera until loading screens and fades clear; confirm it doesn't come back.
- ~~**NPCs standing still after a scene.**~~ Fixed (needs testing): leaving a scene ends the do-nothing package it gave them and the scene guard's 30 s hold on their idles, so their AI picks up again at once.
- **No idle chatter during scenes.** NPCs in a scene still talk: idle lines, greetings and comments to each other. Silence scene actors (and keep bystanders from greeting them) for the scene's length; `SuppressInteraction` only blocks the player's Activate / Talk.
- **No animation markers mid-scene.** NPCs in a scene can be pulled off by their AI packages to use furniture or idle markers (sandboxing), snapping out of the scene. Keep their AI from picking markers or furniture until the scene ends. *Started: NPCs are taken out of any furniture or idle marker when a scene starts and given a do-nothing package, and the scene guard (bGuardScenes) takes them out again and replays the scene if they get back in or a game idle takes over. Needs testing.*
- ~~**Lock actors in place.**~~ Done: the scene lock (`bLockScenes`, docs/ALIGNMENT.md) holds every actor on the scene's spot (plus their alignment) with a keyframed translation, as OStim does, and the scene guard puts anyone who drifts back.

- ~~**Penis position adjustments per animation.**~~ Started (docs/ALIGNMENT.md, Penis bend): OStim's sosBend per scene and role, from scene files, the Align tab and `4StimBendSoS` annotations, turning ZeX's `Penis_01_Offset`. Still to do: moving it (an offset, not only a bend), and confirming the direction and step size in game.

## OStim parity

What OStim NG has that 4Stim doesn't yet, from a read of its source (October 2026). Roughly in priority order within each group.

### Parity audit (approved October 2026)
The audit compared every finished feature with OStim's source; it's being carried out in three builds.
- ~~**1. Settings and defaults.**~~ Done (docs/SETTINGS.md).
- ~~**2. File formats.**~~ Done: OStim's one-scene-per-file layout (SCENES.md, "OStim's layout") with `speeds[].animation` resolved by Idle editor ID, its navigation fields and `origin` back-links sorted by `priority`, `offset`, `modpack`, scene-wide `autoTransitions`, a `Sequences` folder; one action per file with Skyrim `strippingSlots` mapped (ACTIONS.md); one furniture type per file in `Furniture Types` with OStim's fields and `listIndividually` (FURNITURE.md); alignment in OStim's layout and file location, grouped by sex (ALIGNMENT.md). Kept for their features: `playbackSpeed`, `displaySpeed`, `border`, `noWarnings`, `sosBend`, `multiplyScale`.
- ~~**3. Behavior.**~~ Done: starting scenes tagged `intro` (`SetUseIntroScenes`) with OStim's standing / bed rule, any scene for NPC-only scenes; climax waiting for the climax animation's `4StimClimax` annotation (or its end); auto mode fades (`SetUseAutoFades`, `fadeOnEntry`). The converter should turn OStim's `OStimClimax` annotations into `4StimClimax` events (Separate projects).

### Core
- ~~**Actions.**~~ Done: action types in `Actions\*.json`, `actions` in scene files, requirements in role matching, Papyrus queries (docs/ACTIONS.md). The animation converter (Separate projects) should copy OStim scenes' `actions` (the test pack's were backfilled from OStim's scene files).
- ~~**Excitement and climax.**~~ Done (docs/EXCITEMENT.md). Left for later: partner reactions, climax annotations in animations (OStim triggers the climax from the climax animation; 4Stim climaxes when the animation starts), slow motion / blur options.
- ~~**Auto mode.**~~ Done (docs/AUTOMODE.md): stages, routes through navigations, auto speed, pull-out, toggle key, Papyrus. With it, these scene fields: `defaultSpeed`, `noRandomSelection`, actor `tags`, actor `requirements`, `intendedSex`, `autoTransitions` (any event).
- ~~**Concurrent scenes (threads).**~~ Done: any number of scenes at once, tracked natively; NPC-only scenes from the picker ("You: not in it"); a "Running scenes" list and the hotkey on someone in a scene to watch (free camera + HUD, "Stop watching"), switch auto mode or end it; one scene per actor (`FourStim.IsInScene`, checked by `CanUseActor`). Also done: the NPC scene key (`keyNpcSceneStart`, default Shift + the hotkey) starts an NPC scene even while you're in your own, and running scenes are kept in the save: on load everyone in them is let go and dressed again, as OStim ends its scenes, or with `bResumeScenes=1` they start again.
- ~~**Undressing.**~~ Done (docs/UNDRESS.md): at start (optional), partial per action (`undressSlots`, mapped from OStim's), full for `fullStrip` actions, `noStrip` scenes, a slot list setting, redress at the end, Papyrus `UndressActor` / `RedressActor`. Weapons were already put away. Also done: Undress / Dress for each actor by hand in the HUD's Utility tab; clothes come off and go on one piece at a time (`fUndressItemDelay`); animated undressing scenes (an `undress` list in scene files, with OStim's converted ones wired up in the test pack from Standing apart) and OStim's redress animations after a scene, a body part at a time (`SetAnimateRedress`, scenes tagged `redress`). The animation converter (Separate projects) should carry over OStim's `OStimUndressPartial` annotations as `undress` steps: the converted clips lost them (the test pack's timings were read from OStim's originals).

### Presentation
- ~~**Alignment menu.**~~ Done (docs/ALIGNMENT.md): the HUD's Align tab moves, turns and scales each actor per scene and role, live, saved to `Alignment.json`, on top of the scene lock. Alignments are now keyed by sex as OStim's (pass 2). Penis bend done (Penis bend, ALIGNMENT.md). Still to do: alignments keyed by body / height (with Automatic Mesh Detection), heel compensation.
- ~~**Actor scaling and heels.**~~ Done (docs/SCALING.md): OStim's scaling (`SetScaling`, role `scale`, NPC height evened out), heels from the High Heels System's `.json` and `.txt` files, scaled with `scaleHeight` when feet are on the ground, lowered when not, and in the alignment key. Mesh extra-data heights (`HHS` NiFloatExtraData) done too. HHS MCM heights are its `.txt` files, already read. Still to do: other heel mods, furniture `multiplyScale`, LooksMenu height.
- ~~**Facial expressions.**~~ Done (docs/EXPRESSIONS.md): OStim's expression files and layers (underlying by action or `default`, `moan` / `climax` events, `expressionOverride`), Skyrim types mapped onto Fallout 4's face morphs (`Expression Morphs.json`), set smoothly with `mfg morphs`; `noFacialExpressions`, `SetExpressionDurationMin` / `Max`. Still to do: `look` directions, expression `delay`.
- ~~**Sound.**~~ Done (docs/SOUND.md): OStim's voice sets per NPC / voice type / sex, moans (`moan` / `muffled` from actions), climax sounds, with event expressions; sound files or folders as well as descriptors; `moanIntervalMin` / `Max`, `moanVolume`. Action sounds and event reactions came with Peaks and events. Still to do: dialogue (comments, climax comments, post-scene lines), `talk`, muting actors.
- ~~**Peaks and events.**~~ Done (docs/EVENTS.md): OStim's action `peak` (bonedistance on Fallout 4's vanilla bones, loop) and `4StimPeak` annotations (`doPeaks`, `peaksAnnotated`); action `sounds` (peak, bonedistance, loop; `muted`, `soundVolume`); OStim's event files (spank, wash) with stimulation, sound, camera shake, rumble, supertypes and voice set reactions, fired by `4StimEvent` annotations or `FourStim.CallEvent`, heard through `FourStim_OnEvent`; `4StimPlayExpression`. Still to do: muting actors (OStim's actor `muted` and Papyrus mute), sex toys on peaks.
- ~~**Lights.**~~ Done (docs/LIGHTS.md): OStim's face / crotch / ass lights, bright or dim, per sex (`sMaleLight` / `sFemaleLight`), `SetOnlyLightInDark`; game lights placed and moved with a bone (no plugin), removed before saves. Still to do: lights per NPC, the check for actors with a light from another mod, choosing from the HUD; with a 4Stim plugin, OStim-style light armor (Equip objects).
- ~~**Camera extras.**~~ Done (docs/SETTINGS.md): `SetUseFreeCam`, `SetCameraSpeed`, `SetFreeCamFOV`, `SetForceFirstPerson`, fades as scenes with the player start and end (`SetUseFades`), slow motion at a climax (`SetSlowMoOrgasms`). Auto mode fades came with pass 3. Still to do: the custom time scale (`SetCustomTimescale`).

### Actors and roles
- ~~**Equip objects, strap-ons and futa (one system).**~~ Done (docs/ACTORS.md): OStim's equip objects (`Equip Objects\`, type / name / default / variants) from actions' `equipObject`, actor properties, `4StimEquipObject` annotations and Papyrus, taken away at the end; strap-ons for roles that need a penis; futa as an actor property giving a penis (`sex`), with OStim's `futaUseMaleRole` / `Excitement` / `Climax`. Still to do: choosing an object per actor, race or sex (OStim's MCM; with the Papyrus API and save data), and strap-on / tongue armors to point at (4Stim has none).
- ~~**Role choice.**~~ Done (docs/ACTORS.md): OStim's order (penis first), `PlayerAlwaysDom` / `Sub` `Straight` / `Gay`, the picker's "Your role?" (`playerSelectRole...`), `unrestrictedNavigation`.
- ~~**Actor requirements and properties.**~~ Done (docs/ACTORS.md): OStim's actor properties (`Actor Properties\`: type, sex, requirements, equip objects, expression, mute, muffle) with conditions in the file (sex, races, keywords, factions, worn items, player, child, penis) or a perk; scene roles' `type`; defaults for people, women, ghouls and super mutants.

### Furniture
- Done: edge-anchored furniture (beds, tables, benches) avoids a side against a wall (`checkWalls`, FURNITURE.md).
- Change furniture mid-scene, reset displaced clutter afterwards, auto-use the nearest furniture, a bed confirmation, more types (wall, shelf, chemistry / armor / power armor stations).

### Scene format
- Fields OStim has and 4Stim doesn't yet, each to come with its feature: `scale` / `scaleHeight` and `feetOnGround` (actor scaling), `look*` (expressions), the actor `muted` (sound; `moan`, `muffled`, `expressionOverride` and the action `muted`, `doPeaks` and `peaksAnnotated` are done), and OStim's `sosBend` as **`penisBend`** (alignment). In Fallout 4 the penis is part of the male body mesh (A-Body, BodyTalk, Atomic Muscle...), not a separate Schlongs of Skyrim item, so 4Stim names it `penisBend`; the loader should also read `sosBend` from OStim-converted files as an alias. Done: `actions`, `noStrip`, `defaultSpeed`, `noRandomSelection`, actor `tags`, `requirements`, `intendedSex`, `autoTransitions`.

### For other mods
- **Papyrus API.** A thread builder (actors, furniture, start scene or sequence, flags: no auto mode, no undress...), scene metadata queries, random scene by tags (OLibrary), per-actor data.
- **More events.** Furniture changed, NPC scene start / end, sequence end (below). (Climax is done: `FourStim_OnClimax`.)
- **A C++ plugin interface** for other F4SE plugins (start / stop, listeners, alignment).
- **Save data.** Running scenes are saved and start again on load (done, with Concurrent scenes). Still to do: per-actor choices (voice set, equip objects, alignment) in the F4SE co-save, as those features come.

### Settings and UI
- **MCM page** (Fallout 4's Mod Configuration Menu) for every INI setting plus hotkeys, with export / import. Include a setting for each climax effect (shake, blur, edge glow strength and size, rumble), so players can tune them in game.
- ~~**OStim's setting names, defaults and keys.**~~ Done (docs/SETTINGS.md): 4Stim.ini uses OStim's names and defaults (older names still read), with OStim's pull-out, end, search, alignment, free camera and hide-UI keys (unbound by default), and NPC scenes ending after `NPCSceneDuration`.
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
- **OG and NG versions.** Started (docs/OLDGEN.md): 1.10.163 (OG) runs from 4Stim's own address table, made from the Runtime Database's known addresses (65 of the 96 4Stim needs); the other 31 need a CommonLibF4RD `.mapping` file from a 1.10.163 game. Then a tester checks that scenes work (the struct offsets 4Stim reads match AE's by the Runtime Database's layout notes). 1.10.984 (NG) needs a tester.
- **Runtime Database version.** One plugin for OG, NG and AE that also survives future game updates, built on [CommonLibF4RD](https://github.com/Zzyxz/CommonLibF4RD) for [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394), which finds the game's functions when the game starts instead of using a fixed address table per version. The same older CommonLibF4 API as the OG build (port once, covers both), still with per-runtime struct layouts to check (its docs say it doesn't make class layouts compatible), and players need Runtime Database installed. Fairly new (released August 26, 2026; CommonLibF4RD still updated in October): check how it's held up before switching. If it works out, it replaces separate OG / NG builds.
- NPC greetings
- Race filtering
- Claim registry
- First-person camera
- A sequence-end event for other mods

## Separate projects

Tools and add-on modules built alongside 4Stim, outside the core plugin.

- **Animation converter, rebuilt from scratch.** One tool that ports animations both ways, Skyrim to Fallout 4 and Fallout 4 to Skyrim, with a proper bone map and retargeting between the two skeletons and the right Havok output for each game (FO4's hk_2014, Skyrim SE's 64-bit hk_2010 packfiles), keeping annotations and root motion. It builds each game's plugin: FO4 IDLE records one-shot (`dyn_Activation`) for transitions and looping (`dyn_ActivationLoop`) otherwise (the current converter makes every idle looping, and regenerating the test pack's esp undoes the hand fix). It holds the last pose of transition clips (what `hold_end.py` does now, as an option instead of a separate step), and writes 4Stim scene files that keep OStim's `actions`, actor `tags`, `autoTransitions` and the rest. It must carry the genital bones over: the current converter keys only the first 95 bones of ZeX's skeleton, which stops just before `Penis_00`, so converted animations don't move the penis at all (only the scene's penis bend aims it).
- **Sim Settlements 2 furniture module.** A 4Stim module that finds the furniture inside Sim Settlements 2 plots (beds, chairs, tables... placed by the plots' building plans rather than the workshop), so 4Stim's furniture detection and placement work on it like on ordinary furniture.
- **Placeable 4Stim animation markers module.** Markers the player can place (like the base game's animation markers / idle markers) where NPCs start 4Stim scenes by themselves, for example on their sandbox routine, with settings on each marker for which scenes or tags, who can use it and how often.
