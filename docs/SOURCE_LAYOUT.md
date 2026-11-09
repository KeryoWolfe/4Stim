# Source layout

Where 4Stim's plugin code lives. Until November 2026 most of it was one
14,000-line `src/main.cpp`; it was split by area (roadmap 2b.3) with
`tools/refactor/split_main.py`, moving code without changing it.

## `src/main.cpp`

The F4SE entry points (`F4SEPlugin_Load`, the message handler `OnMessage`)
and the functions in `Bridge.h` that the HUD and scene events call.

## `src/Core/`

What used to be main.cpp's anonymous namespace, now `namespace Core`.

- **`Core.h`**: what more than one file uses, in its old order: the
  settings struct, the running scene (`ActiveScene`), thread records, shared
  state (as `inline` globals), and a declaration of every function another
  file calls. A type, global or function that only one file uses lives in
  that file instead.
- One file per area, after main.cpp's old `// ----` sections:

| File | What |
|---|---|
| `Settings.cpp` | 4Stim.ini and OStim's setting names (`LoadSettings`) |
| `Scenes.cpp` | The focused scene, roles, furniture for a scene, running scenes, no gravity, scene start / stop / change tracking |
| `Threads.cpp` | Thread records, and starting threads (OStim's thread API: builders, flows, `StartThread`) |
| `Collision.cpp` | Scene furniture out of the way, the physics world's lock, the collision watch |
| `AutoMode.cpp` | Autoplay (transitions, sequences) and auto mode |
| `SceneLock.cpp` | The scene lock, scales, changing furniture mid-scene, moving apart |
| `Visibility.cpp` | The visibility check, fades on scene actors |
| `SceneGuard.cpp` | The scene guard and the shaking watch |
| `Climax.cpp` | Excitement and climax |
| `Playback.cpp` | The autoplay tick, ending broken scenes, playing / stopping poses, locking actors, pair placement and collision |
| `GroupScenes.cpp` | Scenes with any number of actors |
| `Camera.cpp` | Player controls, the scene camera, HUD, FOV, timescale |
| `Navigation.cpp` | Speed and navigation in the player's scene |
| `Picker.cpp` | The scene picker menu, watching running scenes, choosing who and where |
| `Hotkey.cpp` | The hotkey and other input |
| `AnimEvents.cpp` | Animation events of scene actors |
| `PluginAPI.cpp` | The C++ plugin interface (`4Stim/PluginInterface.h`) |
| `NGAPI.cpp` | OStim's second C++ API (`4Stim/FourStimNG-API.h`) |
| `SaveData.cpp` | The F4SE co-save |

## `src/Natives/`

The Papyrus natives, one file per script (`namespace Core::Natives::<Script>`),
and `Register.cpp`, which binds them all.

`FourStim.cpp` (the FourStim script), `FourStimMCM.cpp` (the MCM page's
natives), `FourThread.cpp`, `FourThreadBuilder.cpp`, `FourActor.cpp`,
`FourActorUtil.cpp`, `FourLibrary.cpp`, `FourMetadata.cpp`,
`FourFurniture.cpp`, `FourSequence.cpp`, `FourModEvents.cpp`, `FourJSON.cpp`.

## Everything else in `src/`

Self-contained systems with their own headers, as before: `Actions`,
`Alignment`, `Excitement`, `Undress`, `Furniture`, `SceneRegistry`, `HUD`,
`Physics`, `Scheduler` and the rest.
