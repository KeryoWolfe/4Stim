# OStim parity audit (1 November 2026)

A line-by-line comparison of OStim NG (github.com/VersuchDrei/OStimNG, local copy at `2c98cb2`) with 4Stim at `479fbc5`, made to find what the port is missing, what it does differently without saying so, and what it never considered. Six reviewers each took one part of OStim and followed its code into 4Stim's; every finding cites file and line in both codebases. Their full reports are in `docs/audit/` (1 to 6). The most serious findings were checked again by hand (marked **checked** below).

Nothing was built or run in game: these are differences in code, not test results.

## What it found

114 findings, about 85 once the overlaps between areas are merged. By area:

| Area | Report | Findings | High |
|---|---|---|---|
| Scene lifecycle: start, roles, furniture, camera, auto mode, end, save/load | audit/1-thread.md | 20 | 5 |
| Per-actor: undress, excitement, climax, expressions, eyes, bend, scaling, equip objects | audit/2-actors.md | 21 | 1 |
| Scene graph, actions, furniture, data formats | audit/3-graph.md | 17 | 2 |
| Sound, voice sets, toys, lights | audit/4-sound.md | 14 | 2 |
| Settings, MCM, menus | audit/5-settings.md | 21 | 0 |
| Scripting API, mod events, plugin interface | audit/6-api.md | 21 | 6 |

The short answer to "are we missing anything": yes, and it falls into five patterns rather than random gaps.

1. **Failure and save/load paths were never ported.** OStim ends scenes on death, combat and cell change, and resets everything on load; 4Stim was built and tested on the happy path.
2. **OStim content doesn't load as written.** The base scenes have other ids, two of OStim's furniture types don't exist, action aliases and unknown action types are handled differently. Each one alone is small; together they mean an unmodified OStim pack doesn't work.
3. **The API is asynchronous where OStim's is immediate.** A scene doesn't exist until it's placed, setters apply a frame later, and the end event comes after the data is gone. Mods written for OStim assume the opposite.
4. **No plugin file.** OStim's API factions, stat lists and keywords need forms; 4Stim has none, so condition-based and dialogue add-ons have nothing to read.
5. **"As OStim" in the docs is sometimes wrong.** About a dozen doc lines describe 4Stim's own behaviour as OStim's (speed, auto mode, sound timing, furniture, events). These are the ones that led to redoing work.

## Tier 1: breaks play or OStim content (fix before beta)

| # | Finding | OStim | 4Stim | Report |
|---|---|---|---|---|
| 1 | **Done in 0.5.0 (built, untested).** Was: **Scenes never end when an actor dies or enters combat, or when an NPC scene is left behind in another cell.** **Checked.** | `Core/Thread.cpp:413-423`, every update | Nothing: `IsDead` / `IsInCombat` are only checked when a scene starts (`main.cpp:6087`, `7464`). ACTORS.md:135 says it's "left to the game". | 1 §1 |
| 2 | **Done in 0.5.0 (built, untested).** Was: **Loading a save from inside a player scene leaves the player locked**: the input layer, the scene FOV, free-camera speed, slow motion and camera flags are only reset at `kGameLoaded`, which F4SE sends once per launch (**checked**: F4SE PluginAPI.h, "only sent once"). | Reset on every load | `main.cpp:9416-9468`; `kPreLoadGame` (`main.cpp:9364`) doesn't do it | 1 §2 |
| 3 | **Done in 0.5.0 (built, untested).** Was: **A failed start leaves actors half-locked** (collision off, scene package on, interaction blocked). | Thread never created | No release path when placement fails | 1 §14 |
| 4 | **Done in 0.5.0 (not yet built or tested).** Was: **The base scenes ship under different ids** (`4StimTest_OStim2PStandingApartMF`), so OStim packs' `origin` / `destination` / `autoTransitions` into base scenes are dropped, silently with `noWarnings`. The hub example in SCENES.md depends on this. **Checked.** Your local reconverted pack (`4Stim Test Anims\...\OStim2PStandingApartMF.json`) does use OStim's ids, but the packaged `4StimTestAnims.json` doesn't. | File name = id, `OStim` prefix reserved | `Data/.../Scenes/4StimTestAnims.json`, packaged by `package-alpha.ps1:151` | 3 §1 |
| 5 | **Done in 0.5.0 (not yet built or tested).** Was: **OStim's `singlebed` and `doublebed` furniture types are missing** (40 base scenes use them), and a scene naming an unknown type is never offered, where OStim plays it as a floor scene. **Checked.** | `furniture types/singlebed.json`, `doublebed.json` | `Furniture/Fallout4.json` has bed, mattress, chair, bench, table, cookingpot, shelf, wall | 3 §2 |
| 6 | **Done in 0.5.0 (not yet built or tested).** Was: **An unknown action type is dropped, shifting every later action's index**, so `4StimPeak.N`, `expressionAction` and index-based API calls point at the wrong action. OStim keeps it as its `default` action. | `default` action | Dropped with a warning (SCENES.md) | 3 §3 |
| 7 | **Done in 0.5.0 (not yet built or tested).** Was: **OStim's own expression files name actions by alias** (`cunnilingus`, `lickingpenis`, `ticklingfeet`...), so those faces never show. **Checked.** | Aliases resolved at load (`Trait/TraitTable.cpp:52,58`) | Raw name lookup (`Expressions.cpp:297-302`) | 2 §1 |
| 8 | **Done in 0.5.0 (not yet built or tested).** Was: **Voice-set entries: 4Stim picks a fitting entry at random; OStim takes the first that fits** (packs are ordered specific-first, catch-all last). OStim's perk `condition` is ignored without a log line, so conditional sounds always play. **Checked.** | `Sound/SoundUtil.cpp:4-12` | `Sounds.cpp` | 4 §2 |
| 9 | **Done in 0.5.0 (not yet built or tested).** Was: **Voice-set choice**: no player / NPC choice (OStim has it in the MCM and the save), no race step or race default voice, creatures get human sounds. | `SoundTable.cpp:104-150` | `Sounds.cpp:258-279` | 4 §1 |
| 10 | **Done in 0.5.0 (not yet built or tested).** Was: **Muting an actor, or an event with nothing to play, leaves the moan face stuck** for the rest of the scene. A bug, not a design choice. | n/a | `Sounds.cpp` stop paths | 4 §8 |

## Tier 2: what add-ons rely on (fix before add-on authors start porting)

| # | Finding | Report |
|---|---|---|
| 11 | **Done in 0.5.0 (not yet built or tested).** Was: **Two starts before the first is placed overwrite each other**: one global "pending" slot holds the thread ID, metadata, options and furniture. Found independently by two reviewers. | 1 §3, 6 F1 |
| 12 | **Done in 0.5.0 (not yet built or tested).** Was: **Thread calls right after `Start` are dropped, and setters apply a frame later** (`AddMetadata` then `HasMetadata` is false). OStim's NPC threads exist when `Start` returns and setters apply in the call. | 6 F2, F3 |
| 13 | **Done in 0.5.0 (not yet built or tested):** `4stim_thread_end` carries the thread ID and OStim's JSON (scene, actors, metadata), and it's sent after Papyrus has released the actors, whose release skips anyone already in a new scene. Was: **The end event fires after the scene and its metadata are erased, and before the actors are released**: no thread ID, metadata or reason, and a scene chained on it can be undone by the release. | 1 §10, 6 F4 |
| 14 | **Done in 0.5.0 (built, untested):** 4Stim now has its own mod events with OStim's names, arguments, thread IDs and order (API.md, "Mod events"). Was: **Mod events**: API.md says Fallout 4's Papyrus has none. F4SE's `RegisterForExternalEvent` exists (**checked**), but F4SE only fires those from Scaleform (`f4se.SendExternalEvent`), so 4Stim would send them from its HUD movie. Event names differ, there are no thread IDs, 8 of OStim's events are missing, and API.md lists two "OStim" events OStim doesn't have. | 6 F5, F17 |
| 15 | **Done in 0.5.0 (not yet built or tested):** `4Stim.esl` has OStim's factions, form lists and keywords (OStim's form IDs), and the plugin fills them in during scenes (API.md). Not set: in-water, time until climax. Was: **No API factions, stat form lists or action factions** (`OStimExcitementFaction`, times climaxed, actor count, in water...), because 4Stim has no plugin file. Dialogue, condition and voice-set add-ons read these. Needs a decision on a small `4Stim.esl`. | 1 §18, 2 §10, 4 §14, 6 F7 |
| 16 | **Done in 0.5.0 (not yet built or tested).** Was: **Starting scenes**: given a starting scene, 4Stim still reorders the actors and fails instead of falling back to a random start; `AddStartingAnimation` without a duration skips the scene at once (OStim uses its length); `navigateTo` is ignored. | 1 §4-5, 6 F11 |
| 17 | **Done in 0.5.0 (built; FourLibrary and FourMetadata checked in game with FourStimTest):** FourActor, FourFurniture, FourActorUtil (new), FourSequence (new), FourLibrary (all 97 of OLibrary's), FourMetadata (all 282 of OMetadata's) and FourJSON have every function of their OStim scripts under its signature; FourThread's `PlaySequence` returns whether it started (OStim's returns nothing, so OStim-style calls still compile), and OThreadBuilder's `Example` (sample code) isn't copied. Was: **Coverage**: `FourMetadata` has 22 of OMetadata's 282 functions, `FourLibrary` 37 of 97; `OActorUtil`, `OSequence`, `OJSON` have no counterpart; `PlayExpression` returns bool not float and `IsClimaxStalled` lacks `CheckThread`, so renamed calls don't compile; `VerifyActors` accepts children. | 6 F14-F16 |
| 18 | **Done in 0.5.0 (built; `FourStimTest.PluginAPI` checks it in game):** OStim's second C++ API is `include/4Stim/FourStimNG-API.h`, with its Scene and Thread interfaces (exports `RequestPluginAPI_Scene` / `RequestPluginAPI_Thread`), events, control callbacks and external-UI switch; the interface map is also exported as `requestInterfaceMap` (OStim's `getInferfaceMap`), and the header no longer says OStim has no alignment API. Departures (API.md): CommonLibF4 types, a three-number plugin version, `MigrateThread` keeps excitement but not undressing, no option descriptions. Was: **OStim's second C++ API (`OstimNG-API`)** (OStim's 22 October update added an interface map to it, `requestInterfaceMap`, through which plugins find its interfaces), used by external UIs (OStim Prism) and companion plugins, is missing; the header wrongly says OStim has no alignment API. | 5 §8, 6 F13 |
| 19 | **Done in 0.5.0 (not yet built or tested).** Was: **Climax stall**: thread and actor stalls are merged, `Climax(act, true)` permanently unstalls, a stalled climax is dropped instead of queued. | 2 §6, 6 F8-F9 |

## Tier 3: behaviour that differs from OStim (Medium)

**Speed and auto mode**
- **Done in 0.5.0 (built, untested):** scenes start at `defaultSpeed` and a scene moved to maps the speed proportionally, as OStim's ChangeNode (its formula as it is). Was: `defaultSpeed` and speed carry-over worked the other way round (1 §9, 3 §6).
- **Done in 0.5.0 (built, untested):** speed control runs in every NPC scene and in the player's with `SetActorSpeedControl`, auto mode or not; `FourThread.SetSpeed` sends the speed event. Was: only in auto mode (1 §7), no event from SetSpeed (6 F6). **Done in 0.5.0 (built, untested):** the speed meter shows the scene's `displaySpeed` (5 §21), else speed / speeds.
- **Done in 0.5.0 (built, untested):** `NoAutoMode` keeps auto mode off whoever asks; `NoPlayerControl` no longer turns it on; `SetForceAIForMasturbation` and `SetForceAIInConsensualScenes` are in (ini and MCM). Was: (1 §6, 1 §8, 5 §5).

**Scenes and transitions**
- **Kept as 4Stim's (tested):** a transition uses its destination's per-role values and alignment, not its own as OStim's: using its own brought back the bounce at each end of every transition in game (2 §2).
- **Done in 0.5.0 (not yet built or tested):** OStim's nine missing annotations (Undress, Redress, UndressPartial, RedressPartial, RemoveWeapons, AddWeapons, SetLooking, UnsetLooking, ResetLooking) as `4Stim...`; the partial masks take Fallout 4's slots (EVENTS.md). Was: missing (2 §3, 3 §12).
- **Done in 0.5.0 (not yet built or tested):** no navigation during a sequence (the HUD and picker), `fadeOnEntry` fades a picked scene too, a sequence's roles ask for what any of its scenes asks for and its actor types must match, and an interrupted sequence ends an `EndAfterSequence` scene. Still 4Stim's: furniture must be the same type in every scene (OStim takes sub- and supertypes). Was: (3 §7-8, 1 §17).
- **Done in 0.5.0 (not yet built or tested):** navigation building as OStim's addNavigations (duplicate links dropped, furniture types checked, the default label and icon from the scene the link ends at), every scene on a link's way checked, and `FourThread.NavigateTo` going along the graph within the navigation distance. Was: (3 §15, 6 F10).
- **Done in 0.5.0 (not yet built or tested):** actor conditions are read again at every scene change, so a penis put on or taken off counts from the next scene on. Still cached between scene changes (the picker's lists are built off the main thread). Was: refreshed only by the picker (2 §5).

**Actions**
- Action tags: done in 0.5.0 for what OStim's carry (their API forms, inverted `-tag` included); action custom data done in 0.5.0 (read from action files, FourMetadata's custom functions). Was: action tags aren't implemented (no `action tags` files, no inverted `-tag`), action custom data (`ints`, `floats`, `strings`, used by OCum and OEndurance) isn't read, and `Default.json` drifted from OStim's data (male masturbation peaks inverted, bathing lost its water requirement...) (3 §4-5, §13).
- **Done in 0.5.0 (not yet built or tested):** per-actor action and event stimulation overrides, OStim's OData functions on `FourStim`, saved with the game. Still open: OStim's MCM page for them (Papyrus only). Was: missing (2 §9).

**Climax**
- **Done in 0.5.0 (not yet built or tested):** a climax and event reactions wait for the current moan to end, as OStim's. Was: they cut it off (2 §8, 4 §6).
- **Done in 0.5.0 (not yet built or tested):** OStim's end rules in every scene (with `endNPCSceneOnOrgasm` on top for NPC scenes) and the end delay restarted by each climax, auto mode's main stage after any climax, the blur only for the player's climax, and 250 Action Points taken per climax (OStim's Stamina). Was: (2 §7, §12, §13, 1 §15, 5 §15).
- **Done in 0.5.0 (not yet built or tested):** climax comments (`climaxCommentSelf` / `climaxCommentOther`), their sound part. Still open: dialogue / `talk` (4 §5, documented), so their dialogue part too.

**Furniture**
- OStim's furniture `rotation` is radians; 4Stim reads degrees, so 180° becomes about 3° (3 §10).
- `listIndividually` means something else, and furniture isn't grouped by list type (3 §9). Conditions are partly skipped, multiple `anykeyword` are ORed (3 §11). Occupied furniture isn't excluded (3 §14); the `NoStim` keyword is, since 0.5.0.
- The player isn't put back after a furniture scene, though ALIGNMENT.md says the player is never moved (1 §11).

**Sound**
- OStim's old voice-set syntax (used by its own `defaultmale.json`) and single-object entries load as empty sets with nothing logged (4 §3).
- Sound forms play through Papyrus `Sound.Play`: no volume, can't be stopped, 2-second guess; CommonLibF4 can play them natively (4 §7).

**Start flow and eligibility**
- Script-started player scenes skip OStim's start flow: waiting for dialogue to close, adding actors, role choice, `PlayerAlways`, furniture choice (1 §12, 6 F18). Builder eligibility is weaker than OStim's `isEligible` (1 §13).

**Settings and menus**
- **Done in 0.5.0 (built, untested):** the search picker takes typing and filters by name, as OStim's search menu (5 §1). Still open: The scene key does something else in and out of a scene, undocumented, without OStim's hold-for-solo (5 §2).
- `SetUseScreenShake` isn't read and shake is on by default; OStim's is off (5 §3).
- **Done in 0.5.0 (built, untested):** OStim's four excitement bar settings (`enablePlayerBar`, `enableNpcBar`, `SetAutoHideBar`, `SetMatchColorToGender`, colored by role by default), and the bars stay up when the HUD fades (5 §4). Was: missing, and the meters faded with the HUD.
- OStim's per-actor and per-sex defaults (Actors page) are mostly missing (5 §6). OStim's add-on `settings` folder isn't read (5 §7).
- Documented as not in yet, so not findings in the report: the custom timescale (`SetCustomTimescale`), the dialogue settings (countdowns, player dialogue, cross talk), `SetBedRealignment` / `bedOffset`, and per-toy / per-slot toy settings (5, "Documented gaps"; SETTINGS.md "Not in yet"). Listed here so this list has every OStim MCM setting 4Stim lacks.
- Undressing: no `SetRemoveWeaponsAtStart`, no wig handling, no Papyrus override hook; `SetUndressingSlotMask` takes Fallout 4 slot bits (5 §14, 2 §15).

## Tier 4: small differences (Low)

The Low findings, each a line or two, are in the reports: the Align tab's steps and keys (5 §9, **done**: OStim's steps, readout, Toggle and the pull-out key as its No), export / import layout (5 §10), MCM volume sliders going to 2 while the plugin clamps to 1 (5 §11), the Toggle key (5 §12), the NPC scene key's 0 (5 §13), settings per install rather than per save (5 §16), the HUD not waking on a scene change (5 §17, **done**), `addActorsAtStart` (5 §18), options' `asState` (5 §20), the camera after a scene and other small lifecycle items (1 §20), equip object choices per reference (2 §16), actor property `sex` (2 §17), expression fall-through and parsing (2 §18-19), post-climax excitement rule (2 §14), toy scaling formulas (4 §11), lights and the equip-object API (4 §12), default voice set contents (4 §9), climax expression without a sound (4 §10), CallEvent defaults and return values (6 F19-F21), graph lookups for the API (3 §16), smaller OStim file fields (3 §17), released actors after a load (1 §19), NPC height (2 §21, unverified).

## Docs that say the wrong thing

**Fixed in 0.5.0:** each line below now says what 4Stim does and, where it differs, what OStim does (the behaviour itself is unchanged; those are tier 3 items). These lines described 4Stim as OStim, or were out of date. Each was a one-line fix and they're where the "redo" cycles came from:

- SOUND.md:67 "cutting in over a moan... as OStim's" (OStim waits).
- AUTOMODE.md: speed control, the standing/bed rule, pull-out "as OStim" (1 §7, §16).
- SCENES.md: `fadeOnEntry` described as OStim's behaviour; the Hubs example needs OStim's ids (3 §1, §7).
- FURNITURE.md: `listIndividually`'s meaning (3 §9).
- API.md: "Fallout 4's Papyrus has no mod events" (6 F5); two events attributed to OStim (6 F17); sex toy formulas "as OStim's" (4 §11); `FindRandomScene` "covers" the missing lookups (3 §16).
- UNDRESS.md: OStim leaves weapons alone (it doesn't; 5 §14).
- ACTORS.md:129, OPTIONS.md:53, TODO.md:45: equip-object choices aren't saved (they are). EXCITEMENT.md: excitement isn't saved (it is) (2 §20, 5 §19).
- SETTINGS.md:10 says OStim's other keys ship unbound (they ship bound); MCM.md and SETTINGS.md miss several ini-only settings (5 §19).
- ALIGNMENT.md: the player is never moved (1 §11); the plugin API header says OStim has no alignment API (6 F13).

## What the port never considered

Beyond single findings, five things were never treated as requirements:

1. **Running OStim content unmodified as a test.** A load of OStim NG's own `data` folder (scenes, actions, furniture types, expressions, voice sets) with a log line for every field 4Stim skips would have caught tier 1 items 4 to 9 at once. This is the cheapest guard against more "redo" cycles.
2. **Failure paths.** Death, combat, cell change, failed starts, loading a save mid-scene, actors released after a load. OStim handles each; 4Stim handles almost none.
3. **The API's timing model.** OStim's API is synchronous; 4Stim's is not, and nothing tells script authors.
4. **A plugin file.** Factions, form lists and keywords are part of OStim's API, not extras.
5. **Other UIs and companion plugins.** OStim's external-UI interface (Prism) and second C++ API.

## Suggested order

1. Tier 1 items 1 to 3 (failure and load paths): small, contained, and the most likely to hit testers.
2. Tier 1 items 4 to 7 (content loading), with the "OStim data unmodified" load check from the section above.
3. Tier 1 items 8 to 10 (sound) and the doc fixes.
4. Tier 2 as one piece of work: a synchronous thread record created at `Start` fixes 11, 12, 13 and most of 16 together; then decide on `4Stim.esl` (15) and external events (14).
5. Tier 3 by area, speed first (it's in every scene).

## What matched

Each report ends with a "Checked and matching" list: what was compared and found equivalent (scene loading and most scene fields, navigation priority and origin links, start-scene selection, excitement rates and decay, alignment keying, expressions' structure, undress slot logic, the options menu format, most settings names and defaults). Read those before assuming an area is untouched.
