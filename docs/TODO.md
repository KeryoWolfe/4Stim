# To do

## V1.0 cleanup (before release)

- **Short hitch when picking a transition.** Choosing a scene that starts a transition freezes the game very briefly. It's likely the game loading the transition's animation file on first play; worth measuring (time from the pick to `PlayIdle` returning in the log) and, if so, preloading the clips of a scene's transitions while it plays.
- **Converter: transitions should play once.** The converter writes every idle with `dyn_ActivationLoop`; transition idles need `dyn_Activation` (see SCENES.md). Regenerating the test pack's esp currently undoes this.
- **Converter: hold the last pose.** Offer `hold_end.py` (0.5 s held last pose for transition clips) as an option in the converter GUI and pipeline instead of a separate step.

- **Scene events' actor arrays.** Actors passed from native code to Papyrus arrived as a "type mismatch" in the physics swap (fixed there by passing form IDs). The scene events (`FourStim_OnSceneStart` and the rest) pass `Actor[]` the same way; check them with a listening script, and pass form IDs if they're broken too.

- **Penis position adjustments per animation.** With rigid SMP genitals (physics swaps), some animations need the penis angle or offset adjusted per scene or role so it lines up with the partner. Pinned for later.

## Converter rebuild

- **Rebuild the animation converter both ways: Skyrim to Fallout 4 and Fallout 4 to Skyrim.** One tool with a proper bone map and retargeting between the two skeletons, and the right Havok output for each game (FO4's hk_2014 and Skyrim SE's 64-bit hk_2010 packfiles). It should keep annotations and root motion, and build each game's plugin (FO4 IDLE records: one-shot for transitions, looping otherwise). It should also write 4Stim scene files and hold the last pose of transition clips. The two converter items above fold into this.

## OStim parity

What OStim NG has that 4Stim doesn't yet, from a read of its source (October 2026). Roughly in priority order within each group.

### Core
- ~~**Actions.**~~ Done: action types in `Actions\*.json`, `actions` in scene files, requirements in role matching, Papyrus queries (docs/ACTIONS.md). The converter should copy OStim scenes' `actions` (the test pack's were backfilled from OStim's scene files).
- ~~**Excitement and climax.**~~ Done (docs/EXCITEMENT.md). Left for later: climax sounds and moans (with Sound, from the OStim sounds now in 4Stim Core\Sound), partner reactions, climax annotations in animations (OStim triggers the climax from the climax animation; 4Stim climaxes when the animation starts), slow motion / blur options.
- **Auto mode.** Random navigation by stage (foreplay, main, pull-out), auto speed from excitement, min/max time per scene; for the player (toggle hotkey) and for NPC scenes. Needs `noRandomSelection` on scenes.
- **Concurrent scenes (threads).** Several scenes at once, NPC-only scenes started by hotkey or script, all tracked natively (the claim registry). Today the HUD follows one focused scene.
- **Undressing.** Undress at start, partial undress per action (slot lists), weapons removed, redress at the end (optionally animated), a slot mask setting, `noStrip` scenes, a Papyrus override.

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
- Change furniture mid-scene, reset displaced clutter afterwards, auto-use the nearest furniture, a bed confirmation, more types (wall, shelf, chemistry / armor / power armor stations).

### Scene format
- Fields OStim has and 4Stim doesn't: `defaultSpeed`, other `autoTransitions` than climax, `noRandomSelection`, `requirements`, `intendedSex`, actor tags, `scale` / `scaleHeight`, `feetOnGround`, `look*`, `noStrip`, `muted`, `sosBend`. (`actions` and the actor `climax` scene are done.)

### For other mods
- **Papyrus API.** A thread builder (actors, furniture, start scene or sequence, flags: no auto mode, no undress...), scene metadata queries, random scene by tags (OLibrary), per-actor data.
- **More events.** Furniture changed, NPC scene start / end, sequence end (below). (Climax is done: `FourStim_OnClimax`.)
- **A C++ plugin interface** for other F4SE plugins (start / stop, listeners, alignment).
- **Save data.** Per-actor choices (voice set, equip objects, alignment) in the F4SE co-save; clean up scenes left running on load.

### Settings and UI
- **MCM page** (Fallout 4's Mod Configuration Menu) for every INI setting plus hotkeys, with export / import.
- **Hotkeys:** end scene, auto mode, pull out, hide UI, NPC-only scene start.
- **Options in the HUD:** per-actor toggles (undress, strap-on, mute...).
- **Translations** for the menu and HUD text.
- **Sex toys** (device integration). Optional, last.

## Later

- PrismaUI
- NPC greetings
- Race filtering
- Claim registry
- First-person camera
- A sequence-end event for other mods
