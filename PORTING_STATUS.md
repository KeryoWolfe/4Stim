# Porting status

Every feature docs/TODO.md marks done, and whether it has been seen working
(roadmap 1.2). **Tested**: seen in game and in the owner's logs (the line
named); **Built, untested**: built, never seen running; **Open**: not done.
Evidence is from the owner's `4Stim.log` of 12 November 2026 unless a date
says otherwise. Test runs: TEST_RESULTS.md.

## Scenes and playback

| Feature | Status | Evidence |
|---|---|---|
| Scene files: OStim's format and layout, every field | Tested | `Scenes: loaded 724 scene(s) and 4 sequence(s) from 635 file(s)` |
| YAML scene files | Built, untested | No YAML pack has been loaded yet |
| Picker: who, where, which scene | Tested | `Picker: picked "…"` |
| Scene lock, scene guard, collision | Tested | `Scene lock:`, `Scene guard:`, `Collision watch:`, `IgnorePairCollision:` (alpha fixes 2.1, 2.3, 7 November) |
| Navigation and speed | Tested | `Player scene: "…" speed N: PlayIdle X ok, X ok` |
| Sequences | Tested | `Sequence` lines (28) |
| Transitions played once | Tested | `Scenes: 403 transition Idle(s) set to play once` |
| Concurrent scenes, NPC-only scenes | Tested | Cait and FF01E414's scene without the player (`Scene end: 00079305 back to where they stood`) |
| Auto mode | Tested | `Auto mode: on for "…" (intercourse)` |
| `playbackSpeed` | Built, untested | `playbackSpeed hook installed`; no pack with a `playbackSpeed` played yet |
| Furniture: finding, placing, edge anchoring | Tested | `Furniture: nearest doublebed…`, `Furniture use:`, `Scene end: the player back to where they stood before the furniture scene` |
| Changing furniture mid-scene | Built, untested | No `ChangeFurniture` in the logs |
| Scene end and clean-up | Tested | `Scene end: X back to where they stood before the scene` |
| Scene package (NPCs do nothing but the scene) | Tested | `Scene package: X does nothing but the scene until it ends` (7 November) |
| NPCs back on their own packages after | Tested | `EvaluatePackage (scene end)` (12 November) |
| NPCs standing still after a scene | Built, untested | The fix (TODO "NPCs standing still") hasn't been watched for |
| Weapons sheathed (2.2) | Built, untested | Needs a scene started with a weapon drawn |

## Actors and bodies

| Feature | Status | Evidence |
|---|---|---|
| Actor properties, roles, requirements | Tested | `Actor properties: X has a penis, …` |
| Who has a penis from the body (2b.7) | Tested | `Penis from body: X has one: "BaseMaleBody:0Full" (skinned to Penis_Offset_SMP)` |
| Futa (wearable penis, `carried`) | Tested | `has a penis, "sex" from Futanari FEV - worn.json` (test patch) |
| Equip objects: strap-ons, tongues | Tested | `Equip objects: X puts on "ostimstrapon"` / `"normaltongue"` |
| Gear the game takes off put back | Tested | `Equip objects: X lost 1 item(s) … putting back` |
| Undressing and redressing | Tested | `Undress: X, slots …: N item(s) to take off`, redress lines |
| Scaling and heels | Tested | `Scaling: X (role 0) scaled to 1.020` |
| Alignment (Align tab, saved) | Tested | `Alignment: 43 offset(s) for 1 actor set(s)` |
| Erections | Tested | `Erection:`; 2.4, 2.15 (7 November) |
| Penis bend and aim | Tested | `Penis bend: X bent …` |
| Physics swaps (A-Body) | Tested | 2.4 (7 November): swaps to `MaleBodyHard.xml` |

## Scene life

| Feature | Status | Evidence |
|---|---|---|
| Actions and action tags | Tested | `Actions: loaded 87 action type(s)` |
| Excitement and climax | Tested | `Excitement: X in "…"` (136), `Climax: X (role N)` |
| Facial expressions | Tested | `Expressions: 80 facial expression(s)`; faces seen |
| Eye directions | Tested | Eye lines (28) |
| Sound (voice sets, moans, climaxes) | Tested | `Sound: files play through sound output model…`; moans heard (10 November) |
| Peaks and events | Tested | Peak lines (16) |
| Lights | Tested | `Lights: light level 75 at X where the scene starts` |
| Camera (free camera, fades) | Tested | `Scene camera:` lines |
| First person | Open | Not tested in 0.4 (owner, roadmap 2.11) |

## Interface and settings

| Feature | Status | Evidence |
|---|---|---|
| HUD (tabs, meters, navigation) | Tested | Used in every run |
| Options in the HUD | Tested | Used (Utility tab) |
| MCM page | Tested | `MCM: its settings are newer than 4Stim.ini`; "Intended gender only" changed (12 November) |
| OStim's setting names, defaults and keys | Tested | `Settings:` lines at load |
| Translations | Tested | `Translations: 602 text(s) in 4Stim_en.txt`; only English tried |

## APIs and saving

| Feature | Status | Evidence |
|---|---|---|
| Papyrus API (`FourThread`…) | Tested in part | The scene events (`Scene events: script X registered`, 10 November); the rest used by 4Stim's own scripts |
| Scene events' actor arrays (2.9) | Tested | `Scene events: actors reach Papyrus as Actor[] (2 actor(s)): fine` |
| Mod events | Tested in part | Through the scene events; `RegisterForModEvent` from an add-on not yet |
| C++ plugin interface, OStim's NG API | Built, untested | No plugin using them yet |
| Sex toys (Intiface) | Built, untested | No toy connected |
| Save data | Tested in part | `Save data: equip objects chosen for 3 actor(s)` at load; saving during a scene not tested in this format (TEST_PLAN.md 10) |
| Other game versions (Runtime Database) | Built, untested on the current build | A 1.10.163 tester ran an earlier build (roadmap 2.12) |

## Open before beta

None in code (docs/TODO.md, "Before Beta Release"). Untested items above
are what TEST_PLAN.md's run before beta (roadmap 3.1) should cover first.
