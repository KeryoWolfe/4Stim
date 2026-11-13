# Test plan

The tests a release has to pass (roadmap 1.3), from the instructions'
matrix. Results go in TEST_RESULTS.md, one entry per run.

## How to run them

- **Game:** Fallout 4 1.11.240 (Steam), F4SE 0.7.x, Address Library.
- **Profile:** a separate Mod Organizer 2 profile, saves backed up first.
  At least one run in a small load order: 4Stim, its requirements and the
  test animation pack only (roadmap 3.2).
- **Logs** (each test names the lines to look for):
  - 4Stim: `Documents\My Games\Fallout4\F4SE\4Stim.log`
  - F4SE: `Documents\My Games\Fallout4\F4SE\f4se.log`
  - Papyrus: `Documents\My Games\Fallout4\Logs\Script\Papyrus.0.log`
    (`bEnableLogging=1` under `[Papyrus]` in `Fallout4Custom.ini`)
  - Crashes: the crash logger's folder (Addictol)
- **A test passes** only when what's expected is seen in game *and* in the
  log. No error on screen isn't a pass. Each result is Pass, Fail (with what
  happened and the log lines), Blocked (what stopped it) or Not tested (why).
- `4Stim.log` lines below drop the time and thread (`[12:00:00.000] [1234]`);
  `X` is a form ID, `"…"` a name.

## The matrix

### 1. Installation

| # | Steps | Expected |
|---|---|---|
| 1.1 | Install `4Stim-<version>.zip` with the mod manager; enable `4Stim.esl`. | Files land under `Data\` (`F4SE\Plugins\4Stim.dll`, `4Stim.ini`, `4Stim\…`, `Interface\`, `Scripts\`, `Sound\fx\4Stim\`, `MCM\Config\4Stim\`). |
| 1.2 | Install the test pack and, optionally, the Equip Objects add-on (enable their plugins; build the strap-on in BodySlide). | `4StimTestAnims.esp` and `4Stim - Equip Objects.esp` enabled. |
| 1.3 | Uninstall: end any scene, save, remove the mods, load that save. | The save loads; no 4Stim errors in the Papyrus log beyond "unable to bind script" for removed scripts. |

### 2. Dependency checks

| # | Steps | Expected |
|---|---|---|
| 2.1 | Start the game. | `4Stim v…`, `Game version 1.11.240.0, F4SE 0.7.…`, `4Stim loaded`. |
| 2.2 | Without the Address Library (and without Runtime Database). | 4Stim doesn't load and its log or `f4se.log` says why; the game doesn't crash. |
| 2.3 | Without MCM, LooksMenu, FSMP, HHS (one at a time). | The game runs and scenes work; that feature is off (no `Physics: FSMP's DynamicHDT script found`, no morph erections...). |
| 2.4 | Without `4Stim.esl`. | No `API forms: 4Stim.esl found` line; scenes still run, without OStim's factions. |

### 3. Plugin loading

| # | Steps | Expected |
|---|---|---|
| 3.1 | Reach the main menu. | `kPostLoad`, `Papyrus functions bound`, `kGameLoaded`, `HUD menu registered`, `Picker menu registered`; nothing from 4Stim in `f4se.log` but a load. |
| 3.2 | Load a save. | `Preload: scenes, actor properties, expressions and lights read in N ms`; `Scenes: loaded N scene(s) … from N file(s)`. Warnings about single scenes (`no Idle with the editor ID…`) are the pack's, not failures. |

### 4. Papyrus

| # | Steps | Expected |
|---|---|---|
| 4.1 | Compile `4Stim Core\Scripts\Source\User` with the Creation Kit's compiler. | Every script compiles. |
| 4.2 | Load a save and run a scene. | No `FourStim…`/`Four…` errors in `Papyrus.0.log`. |
| 4.3 | An add-on registered for scene events (`FourStim.RegisterForSceneEvents`). | `Scene events: script X registered`; its `FourStim_OnSceneStart` / `End` run. |

### 5. Actor validation

| # | Steps | Expected |
|---|---|---|
| 5.1 | Press the hotkey looking at nobody, then at a child, a dead NPC, a creature with no scenes. | The picker offers no scene with them; no scene starts. |
| 5.2 | Open the picker near men, women and a futa. | `Actor properties: X has a penis` / `has no penis` match each; clothed men have one. |
| 5.3 | An NPC already in a scene. | They aren't offered for another. |

### 6. Scene startup

| # | Steps | Expected |
|---|---|---|
| 6.1 | Hotkey → pick a partner → Right here → a scene. | `Picker: picked "…"`, `PlaySceneIdles: PlayIdle returned true / true`; both actors in place, facing each other, animating. |
| 6.2 | The same on furniture (a bed, a chair). | `Furniture: X -> (…)`; actors on the furniture. |
| 6.3 | A solo scene, a three-actor scene. | Each actor plays their role. |
| 6.4 | An NPC-only scene (NPC scene hotkey). | It starts and runs in auto mode. |

### 7. Animation

| # | Steps | Expected |
|---|---|---|
| 7.1 | Watch a two-actor scene for a minute. | The actors stay together (no drift): no `Scene guard: … put back` again and again; `Shake watch` shows small numbers. |
| 7.2 | Change speed (`=` / `-`), navigate to other scenes, a transition. | `Player scene: "…" speed N: PlayIdle X ok, X ok`; both change on the same frame. A brief hitch the first time a scene plays is known (DECISIONS.md). |
| 7.3 | Undressing, erection, penis bend, faces, sounds, excitement. | `Undress: X, slots …`, `Erection:`, `Penis bend:`, moans heard, `Excitement:` rises; climax at 100 (`Climax: X (role N)`). |
| 7.4 | A woman in a role that needs a penis (`Intended gender only` off). | `Equip objects: X puts on "ostimstrapon"`; she still reads `has no penis`. |

### 8. Scene completion

| # | Steps | Expected |
|---|---|---|
| 8.1 | Let a scene end on climax (`SetEndOnOrgasm`) and by the HUD's End. | `Scene end: X back to where they stood before the scene`; everyone redressed, normal scale, walking off normally; equip objects off. |
| 8.2 | Talk to the NPC, give them orders, fight. | They respond normally (their AI and packages back). |

### 9. Cancellation

| # | Steps | Expected |
|---|---|---|
| 9.1 | End a scene in its first second. | It ends cleanly, as in 8.1. |
| 9.2 | Hurt a scene actor (`bEndOnDamage`). | The scene ends, as in 8.1. |
| 9.3 | Kill or disable a scene actor (console `disable`). | The scene ends; the others are released. |
| 9.4 | Start a scene, end it, start again ten times. | No actor left frozen, small, undressed or unable to move. |

### 10. Save and load

| # | Steps | Expected |
|---|---|---|
| 10.1 | Save during a scene, load that save (`bResumeScenes=0`). | `Save data: N scene(s) were running in this save`; the actors are released, dressed and free. |
| 10.2 | The same with `bResumeScenes=1`. | `Save data: starting "…"`: the scene starts again. |
| 10.3 | Load another save during a scene; start a new game during one. | Nothing from the old scene carries over. |

### 11. Cell transitions

| # | Steps | Expected |
|---|---|---|
| 11.1 | Start an NPC scene, walk out of the cell or fast-travel. | `Actor 3D: X's 3D was unloaded`; the scene ends or is released; coming back, the NPCs are normal. |
| 11.2 | A scene with a follower, then a load door. | The follower follows normally afterwards. |

### 12. UI and controls

| # | Steps | Expected |
|---|---|---|
| 12.1 | The HUD's tabs: Navigation, Search, Align, Utility (options). | Each opens and acts; `Esc` / `Backspace` hands the keys back. |
| 12.2 | The same with a gamepad. | The d-pad and buttons work as the keys do. |
| 12.3 | Every MCM page; change a setting, export, import. | `MCM: …`; the change takes effect; export / import round-trips. |
| 12.4 | A non-English game language (if a translation is installed). | Texts in that language, English where a text is missing. |

### 13. Compatibility

| # | Steps | Expected |
|---|---|---|
| 13.1 | With AAF installed, an AAF scene, then a 4Stim scene. | Both work; neither ends the other. |
| 13.2 | In the owner's full load order. | No new crashes; problems compared with the small load order (3.2). |

### 14. Performance

| # | Steps | Expected |
|---|---|---|
| 14.1 | Five minutes of scenes. | No lasting frame drop; `4Stim.log` doesn't grow by more than a few lines a second. |
| 14.2 | Ten minutes after scenes end. | No 4Stim lines still being written; Papyrus log quiet. |

### 15. Packaging

| # | Steps | Expected |
|---|---|---|
| 15.1 | Build the zips (`tools/package.ps1`). | Three zips; every file in them is in SOURCE_AUDIT.md's provenance list. |
| 15.2 | Look in the zips. | No Skyrim files (no Skyrim `.pex`, behaviors or `OStim.esp`), no other packs' animations, no Futanari FEV patch. |
| 15.3 | Install from the zips into a clean profile and run 6.1. | Works. |
