# Changelog

## Next

- **For other mods** (docs/API.md): OStim NG's Papyrus API scripts as `FourThread`, `FourThreadBuilder`, `FourLibrary`, `FourMetadata`, `FourActor` and `FourFurniture` (OStim's function names: start scenes with options, follow them by thread ID, random scenes by tags and actions, scene data, per-actor and per-thread metadata, find furniture); new scene events `FourStim_OnNPCSceneStart` / `End` and `FourStim_OnSequenceEnd`.
- **C++ plugin interface** (`include/4Stim/PluginInterface.h`): OStim's interface map for F4SE plugins, with threads (listeners, builder), alignment and sex toys.
- **Sex toys** (OStim's): device plugins register toys through the plugin interface; 4Stim drives them from the player's scene, on actions' `toySlot`s, with OStim's settings (`[SexToys]`, MCM Sex toys page). **Intiface Central** built in (`bIntiface`): 4Stim connects to it and uses its vibrate-type devices with no other mod.
- **`playbackSpeed`** (OStim's): a scene speed can play its animation faster or slower (`bUsePlaybackSpeed`).
- **Physics swaps, suffix syntax:** `toSuffix` / `toReceivingSuffix` ("Hard", "HardBottom") swap whatever physics file the actor's body names for its suffixed version, so one entry covers a body's variants and other bodies named the same way (docs/PHYSICS.md).
- **Saved:** the equip objects chosen for actors (strap-on, tongue...) are kept in the save.
- **Fixed:** scene sounds (moans, climaxes, slaps) followed the player instead of staying where the scene is: sound files now play through one of the game's positional sound output models.
- **Fixed:** a penis could be left stretched and stuck at an angle after a scene: the physics swap back now waits until the actor is out of the scene's pose, and everyone's SMP physics is rebuilt from the reference pose after a scene (FSMP's `ResetPhysics`).
- **Controller scheme** (MCM Controller page, `sPadScheme`): 4Stim Modern (B to the tab buttons, then hide) or OStim Classic (OStim's navigation on the d-pad and A: Left to the tab buttons, Right back).
- **The HUD, closer to OStim's menu:** the tab buttons are OStim's menu selector (Up / Down move a ring, Yes / A opens the tab and goes back to the list); "Faster" / "Slower" rows under the Navigation list, as OStim's speed buttons; and OStim's fade: after `fHUDFadeTime` seconds (10) without input the HUD fades out and hands the keys back, and any of its keys brings it back.
- **Weapons back on after a scene:** the weapon taken off as a scene starts goes back on when it ends (the player and NPCs), also after loading a save mid-scene.
- **OStim's keys by default:** Up arrow starts a scene, numpad + / - speed, numpad 1 pull out, numpad 0 auto mode, numpad / free camera, numpad * hide the HUD, numpad . end, K search, L Align; and OStim's navigation keys for the HUD (numpad 8 / 5 / 4 / 6, 7 to choose, 9 between the list and the tab buttons; `SetOsaUpKey`...), besides the arrows and Enter.
- **Controller:** B goes from the HUD's list to its tab buttons (Up / Down pick a tab), B again hides the HUD while the scene goes on, and A shows it again. The old LB / RB (tabs) and Y / X (speed) are gone, and the d-pad's Left / Right only change an Align value (no more moving between the list and the tabs); the MCM's new Controller page (`[Controller]`) gives any scene key a button, none by default.
- **MCM Hotkeys page:** now sets `4Stim.ini`'s keys, showing them (OStim's defaults) in OStim's layout with its navigation keys; the MCM's own hotkeys are gone. Millisecond settings show in seconds in the MCM.
- **Physics:** a man on the receiving end (anal sex, pegging, fingering...) gets `MaleBodyHardBottom.xml`, decided by the scene's actions instead of the role's place, and switches as the scene changes.
- **Undressing scenes:** "take off her top / bottoms" are now "chest armor" (slot 41) and "outfit" (the whole outfit and its armor), as Fallout 4's clothes are one piece.
- **Fixed:** the MCM's theme (and HUD on / off, control scheme) didn't apply until the game restarted.
- **Actors and roles** (OStim's, docs/ACTORS.md): actor properties decide who can be in scenes and take which roles (an actor type, what they have for roles' requirements), with conditions written in the file instead of OStim's perks. Defaults for people, women, ghouls and super mutants (who only take roles made for them).
- **Equip objects and strap-ons** (OStim's): items put on during scenes by actions, actor properties, `4StimEquipObject` annotations or Papyrus, and taken away at the end; a strap-on for a woman in a role that needs a penis (with a strap-on mod's armor in `Equip Objects\`).
- **OStim's strap-on for Fallout 4** (optional add-on, 4Stim Equip Objects): Calyps' strap-on refit to True Wasteland Body as a BodySlide project, its dildo bending with the penis bend.
- **OStim's tongues** in the same add-on (normal, long, snake; pierced; cum variants), shown by tongue expressions (OStim's `phonemeObjects`).
- **"Who leads?"** for scenes without you: the picker asks which NPC takes the first role.
- **Futa:** an actor property gives those wearing an equippable penis one, with OStim's `futaUseMaleRole`, `futaUseMaleExcitement` and `futaUseMaleClimax`.
- **Your role:** OStim's order (those with a penis first), `PlayerAlwaysDom` / `Sub` settings, and the picker's "Your role?" step (`playerSelectRoleGay` on by default, as OStim).
- **Penis bend** (OStim's sosBend): per scene and role, from the scene file, the Align tab's new "Penis bend" row and `4StimBendSoS` annotations, by turning ZeX's `Penis_01_Offset` (docs/ALIGNMENT.md).
- **Test pack:** OStim's penis bends (`sosBend`) copied into the test scenes' roles as `penisBend` (149 scenes).
- **Fixed:** an actor whose high heels came off mid-scene floated at the heel height for the rest of the scene.
- **Heels off with HHS's own API:** in roles with the feet off the ground, the High Heels System's height is turned off for the scene (and back on after), instead of lowering the actor.
- **Furniture** (OStim's, docs/FURNITURE.md): change furniture mid-scene (the HUD's Utility tab, "Change furniture..."); `SetSelectFurniture=0` puts a new scene on the nearest furniture without asking, `SetEnableFurniture=0` leaves furniture out; furniture in use can't be used by anyone else; `SetResetClutter` puts back the loose items a furniture scene knocked about (only what it moved). Furniture another scene is on isn't offered.
- **Walls and shelves:** the test pack's wall and shelf scenes now play: walls are found by ray casts (Fallout 4 has hardly any wall-lean markers), shelves by their models. Spots are first guesses to tune.
- **Walls and edges, refined:** a wall only counts when nothing stands in front of it (an open shelf no longer lets a wall scene through), and furniture scenes take the side away from a wall, checked at several heights.
- **OStim's scene format, complete:** the actor `noStrip`, `singleSpeed`, OStim's eye directions (`lookUp` / `lookLeft`..., by turning the eye bones), `scaleOffsetWithFurniture` and furniture `multiplyScale`. The test pack now has OStim's actor scales, eye directions and the rest from its scene files.
- **MCM page** (docs/MCM.md): every setting a control can show, the climax effects (with the new `fClimaxGlowSize`), the MCM's own hotkeys, and export / import (OStim's mcmsettings.json); changes apply at once. The page has the 4Stim logo, and its labels are translated (they showed as `$4Stim_MCM_...` keys).
- **Options in the HUD** (docs/OPTIONS.md): OStim's options files in the Utility tab; each actor's page has undress, mute, and their strap-on and tongue (on / off, and which one). Papyrus `Mute` / `Unmute` / `IsMuted`, `SetEquipObjectID` / `GetEquipObjectID`.
- **Translations** (docs/TRANSLATIONS.md): the picker, HUD, notifications, MCM and options read `Interface\Translations\4Stim_<language>.txt`.
- **Navigation `border` and `noWarnings`** (OStim's): a colored ring around a navigation's icon, and no warning for links to scenes that aren't installed.
- **Papyrus:** `GetFurnitureType`, `IsFurnitureChildOf`, `GetSceneFurniture`, `GetSceneFurnitureType`, `ChangeFurniture`, `OpenFurnitureMenu`, and the `FourStim_OnFurnitureChange` scene event.
- **Changed:** `SetOnlyGayAnimsInGayScenes=0` no longer offers every scene: roles still need an actor of their type with what they require (OStim's; `unrestrictedNavigation=1` drops that too).

## 0.2.1 alpha (October 2026)

The 0.2.0 build, plus experimental support for the pre-next-gen game.

- **Fallout 4 1.10.163 (experimental):** 4Stim can load on the pre-next-gen version from an address table in `Addresses\` (docs/OLDGEN.md). The table is still missing 31 addresses, which a tester on 1.10.163 can supply.

## 0.2.0 alpha (October 2026)

Second test build: OStim's settings, file formats, faces, voices, peaks, events and lights (see `docs/ALPHA.md`).

- **OStim's settings:** `4Stim.ini` now uses OStim NG's setting names and defaults (docs/SETTINGS.md); the older names still work. Changed defaults, to OStim's: post-climax excitement 10 / 30, auto mode 7.5-15 s per scene, pull-out up to 90, 5 navigation steps, auto speed every 2.5-7.5 s between 15 and 85 excitement, furniture search 1600 units, no redress animations, clothes off all at once, and scenes in a save end on load (`bResumeScenes=1` starts them again).
- **OStim's file formats:** OStim-style scene files (one per file, id = file name, `speeds[].animation` found by Idle editor ID, `origin` / `priority` navigations, `offset`, `modpack`), sequence files, action files (Skyrim `strippingSlots` mapped to Fallout 4) and furniture type files are read alongside 4Stim's own.
- **Alignment like OStim's:** kept per set of actors (by sex, as OStim's defaults) in `Documents\My Games\Fallout4\4Stim\alignment.json`, in OStim's layout. The old `Alignment.json` is converted once.
- **Starting like OStim:** a new scene with you starts with a scene tagged `intro` (OStim's intro scenes are tagged in the test pack), else `idle`; off furniture someone stands, on a bed no one does. NPC-only scenes start with any fitting scene.
- **Climax timed by the animation:** with a climax animation, the climax happens at its `4StimClimax` annotation (or when it ends), not as it starts.
- **Auto mode fades** (`SetUseAutoFades`, and scenes marked `fadeOnEntry`).
- **Actor scaling and heels** (OStim's): actors are scaled to the animation's size, and heels from the High Heels System are scaled out or taken off (docs/SCALING.md).
- **Peaks and action sounds** (OStim's): actions find their thrusts and strokes from the actors' bones (or a timer, or the animation's `4StimPeak` annotations) and play their slaps and strokes on them (docs/EVENTS.md).
- **Events** (OStim's): spank and wash events with their sound, stimulation, camera shake, rumble and voice reactions, from the animation's `4StimEvent` annotations or Papyrus `FourStim.CallEvent`; scripts hear them through `FourStim_OnEvent` (docs/EVENTS.md). Also the `4StimPlayExpression` annotation.
- **Lights** (OStim's): a face, crotch or ass light on each actor during scenes, per sex, optionally only in the dark (docs/LIGHTS.md). Off by default, as OStim.
- **Heels from the mesh:** the High Heels System's `HHS` extra data in a shoe's mesh is read too (docs/SCALING.md).
- **Fixed:** 4Stim annotations (such as `4StimClimax`) were ignored more than a minute into a scene.
- **Facial expressions** (OStim's): faces by action, moan and climax faces, open mouths for oral, from OStim's own expression files mapped onto Fallout 4's face morphs (docs/EXPRESSIONS.md).
- **Sound** (OStim's voice sets): moans, muffled moans during oral, and climax sounds, per NPC, voice type or sex (docs/SOUND.md).
- **OStim's HUD controls** as an option (`sHUDControls=Columns`): Left / Right move between the list and the tab buttons.
- **Fixes:** floor scenes played on a bed now lie along the middle of it, not off its edge; a label typo ("sit up (...)gs", from OStim's own translation).
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
