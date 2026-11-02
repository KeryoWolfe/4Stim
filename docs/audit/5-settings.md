# Area 5: Settings, MCM, UI and options: OStim NG vs 4Stim parity audit

Sources read. OStim: `skse/src/MCM/MCMTable.{h,cpp}`, `skse/src/Settings/*` (SettingTable, AddonPage, ExportImport), `skse/src/UI/*` (UIState, Settings, Scene/SceneMenu + SceneOptions, Align/AlignMenu + IncrementValue, Search/SearchMenu), `skse/src/Events/EventListener.cpp` (keys), `skse/src/ModAPI/OstimNG-API-Thread.*`, `skse/src/Serial/Manager.cpp` (export of actor data), `data/Scripts/Source/OSexIntegrationMCM.psc`, `OBarsScript.psc`, `OSettings.psc`, `data/SKSE/Plugins/OStim/{options, options README.txt, settings README.txt}`, `OStim_ENGLISH.txt`, `flash/Option_Box_MC.as`.
4Stim: `Data/F4SE/Plugins/4Stim.ini`, `Data/MCM/Config/4Stim/{config.json,settings.ini,keybinds.json}`, `src/main.cpp` (Settings struct ~134, alias table ~302, LoadSettings 420-880, keys ~7040-7160, climax effects ~3510-3650), `src/MCM.cpp`, `src/Options.cpp`, `src/HUD.cpp`, `Interface-src/hud/HUDPanel.as`, `Data/F4SE/Plugins/4Stim/Options/4Stim.json`, `Data/Scripts/Source/User/FourStimMCM.psc`, docs SETTINGS / MCM / OPTIONS / HUD_API / ALIGNMENT / UNDRESS / ACTORS / EXCITEMENT / TODO.

---

## Settings table: OStim's MCMTable to 4Stim

(✓ = same name and default; ✗ = missing; ≠ = differs.) Units: 4Stim.ini keeps OStim's ms; the MCM shows seconds (as OStim's MCM does).

| OStim name (MCMTable.h) | OStim default | 4Stim | Status |
|---|---|---|---|
| SetResetPosition | 1 | SetResetPosition=1 | ✓ |
| SetCustomTimescale | 0 | none | ✗ (documented "Not in yet", SETTINGS.md:102) |
| SetUseFades | 1 | 1 | ✓ |
| SetUseIntroScenes | 1 | 1 | ✓ |
| addActorsAtStart | 1 | none (the picker always lists NPCs, fActorRadius 1500; OStim uses 2000) | ✗ undocumented (Finding 18) |
| SetOnlyLightInDark | 0 | 0 | ✓ |
| autoExportSettings / autoImportSettings | 0 / 0 | none | ✗ undocumented (Finding 16) |
| SetOsaUp/Down/Left/Right/Yes/End/TogKey | 72/76/75/77/71/83/73 (DX) | numpad 8/5/4/6/7/./9 (VK) | ✓ same keys; Tog's meaning ≠ (Finding 12) |
| SetKeyMap | 200 (Up) | 0x26 (Up) | ✓ key; behaviour ≠ (Finding 2) |
| keyNpcSceneStart | 0 (= unbound) | 0 (= Shift + hotkey) | ≠ (Finding 13) |
| SetKeyUp / SetKeyDown | 78 / 74 (numpad + / -) | 0x6B / 0x6D | ✓ |
| SetPullOut | 79 (numpad 1) | 0x61 | ✓ key; Align-menu meaning missing (Finding 9) |
| SetControlToggle | 82 (numpad 0) | 0x60 | ✓ |
| keySearch / keyAlignment | 37 (K) / 38 (L) | 0x4B / 0x4C | ✓ |
| SetFreeCamToggleKey | 181 (numpad /) | 0x6F | ✓ |
| keyHideUI | 55 (numpad *) | 0x6A | ✓ |
| SetUseRumble | 1 | 1 | ✓ |
| SetActorSpeedControl, autoSpeedControlInterval Min/Max, Excitement Min/Max | 1, 2500/7500, 15/85 | same | ✓ |
| NPCSceneDuration | 300000 | 300000 | ✓ |
| endNPCSceneOnOrgasm | 1 | 1 | ✓ |
| navigationDistanceMax | 5 | 5 | ✓ |
| SetAIControl | 0 | 0 | ✓ |
| SetForceAIForMasturbation | 0 | none | ✗ (Finding 5) |
| SetForceAIIfAttacking / IfAttacked | 0 / 0 | none (unused in OStim's code too) | ✗ (Finding 5) |
| SetForceAIInConsensualScenes | 0 | none | ✗ (Finding 5) |
| autoModeLimitToNavigationDistance | 1 | 1 | ✓ |
| SetUseAutoFades | 0 | 0 | ✓ |
| autoModeAnimDuration Min/Max, ForeplayChance/Threshold, PulloutChance/Threshold | 7500/15000, 35, 15/35, 75, 80/90 | same | ✓ |
| SetEnableFurniture / SetSelectFurniture | 1 / 1 | 1 / 1 | ✓ |
| SetFurnitureSearchDistance | 15 | 15 (MCM 0-50; OStim 1-30) | ✓ |
| SetResetClutter / SetResetClutterRadius | 1 / 5 | 1 / 5 | ✓ |
| SetBedRealignment / bedOffset | 0 / 3 | none | ✗ (documented SETTINGS.md:102, TODO.md:51) |
| SetUseFreeCam / SetCameraSpeed / SetFreeCamFOV | 1 / 3 / 45 | same | ✓ |
| SetUseScreenShake | **0** | not read; fClimaxShake=**1.0** | ≠ (Finding 3) |
| SetForceFirstPerson | 0 | 0 | ✓ |
| SetsexExcitementMult / SetFemaleSexExcitementMult | 1 / 1 | same | ✓ |
| excitementDecayRate / GracePeriod | 0.5 / 5000 | same | ✓ |
| postOrgasmExcitement / Max | 10 / 30 | same | ✓ |
| enablePlayerBar / enableNpcBar | 1 / 1 | none | ✗ (Finding 4) |
| SetAutoHideBar | 0 | none (meters always fade with the HUD) | ✗≠ (Finding 4) |
| SetMatchColorToGender | 0 | none (always by sex) | ✗≠ (Finding 4) |
| SetAutoClimaxAnims, endOnPlayerOrgasm, SetEndOnOrgasm, SetEndOnSubOrgasm, SetEndOnBothOrgasm | 1,0,1,0,0 | same | ✓ |
| SetSlowMoOrgasms | 1 | 1 | ✓ |
| SetBlurOrgasms | 1 | 1 | ✓ name/default; trigger ≠ (Finding 15) |
| SetAlwaysUndressAtStart | 0 | 0 | ✓ |
| SetRemoveWeaponsAtStart | 1 | always on, no setting | ✗ (Finding 14) |
| SetUndressIfNeed / SetPartialUndressing | 1 / 1 | same | ✓ |
| SetRemoveWeaponsWithSlot | 32 | none | ✗ (Finding 14) |
| SetAnimateRedress | 0 | 0 | ✓ |
| undressWigs | 0 | none | ✗ (Finding 14) |
| SetUndressingSlotMask | 0x3D8BC39D (Skyrim slots) | read, FO4 bits | ≠ (Finding 14) |
| SetExpressionDurationMin / Max | 1000 / 3000 | same | ✓ |
| moanIntervalMin / Max, moanVolume | 2500 / 4000, 1 | same (MCM range bug, Finding 11) | ✓ |
| maleDialogueCountdown*, femaleDialogueCountdown*, playerDialogue, preventSameVoiceCrossTalk | 3/6, 1/3, 1, 0 | none | ✗ (documented "dialogue" not in, SETTINGS.md:102) |
| soundVolume | 1 | 1 | ✓ |
| SetOnlyGayAnimsInGayScenes, PlayerAlways{Dom,Sub}{Straight,Gay}, playerSelectRole{Straight,Gay,Threesome} | 1, 0×4, 0/1/0 | same | ✓ |
| unequipStrapOnIfNotNeeded | 0 | 0 | ✓ |
| useSoSSex / useTNGSex / futaUseMaleLight / fixDarkFace | 1/1/–/0 | none | n/a in Fallout 4 (no SoS/TNG; dark-face is a Skyrim bug). Not stated in SETTINGS.md (Finding 19) |
| futaUseMaleRole / Excitement / Climax | 1 / 0 / 1 | same | ✓ |
| SetScaling / SetSchlongBending | 0 / 0 | same | ✓ |
| alignmentGroupBySex / Height / Heels | 1 / 0 / 1 | same | ✓ |
| unrestrictedNavigation / noFacialExpressions | 0 / 0 | same | ✓ |
| MCM menus outside MCMTable: male/female/**player** light, default/**player** strap-on, default tongue male/female/**player**, **player voice**, **Actors page** (per-actor voice, equip objects, action/event stimulation overrides) | – | sMaleLight/sFemaleLight only | Partial (Finding 6) |
| Toys page (per toy, per slot) | – | global [SexToys] only | documented (TODO.md:68) |
| Addons page (JSON `settings` folder, SettingInterface) | – | none | ✗ (Finding 7) |
| UI `ui_settings.json`: positions/scales, fadeTime 10 | – | theme `layout.*`, fHUDFadeTime 10 | ✓ equivalent |

---

## Findings (most important first)

### 1. The search menu has no text search
- **Class**: Missing
- **Severity**: Medium
- **OStim**: `keySearch` opens a modal, game-pausing text-entry menu (`UI/Search/SearchMenu.cpp:8-28`, `AllowTextInput(true)` at :77). Typing searches every scene by name (`Search()`, :147 → `GraphTable::searchNodesByName`), keeping those with the thread's actor count, the furniture type, and no transitions (:152-156). Results come back sorted by label (`AssignData`, :125, sorted `a.label > b.label`, so descending). Up/Down/Yes/No drive the list (:55-69). Picking one calls `ChangeNode`.
- **4Stim**: the search key opens the picker in `PickerMode::kSearch` (`src/main.cpp:7127-7128`). It lists every scene and sequence that fits these actors and this furniture (`main.cpp:6261-6295`). There is no text input; `Interface-src/PickerPanel.as` has no keyboard text field. With a large pack (OStim's base pack alone has hundreds of scenes) you have to scroll the whole list. The picker also filters by role sex (`SexFilter`, main.cpp:6267), which OStim's search doesn't, and it adds sequences. Neither difference is documented.
- **Fallout 4 constraint?**: No. Fallout 4 Scaleform has text input (`BSScaleformManager` / `ControlMap::AllowTextInput` equivalents exist; the console and naming menus use it).
- **Suggested fix**: add a filter field to the picker in search mode: type to filter by name, Enter to pick. Document the sex filter and sequences as departures.

### 2. The scene key does something different in and out of a scene (undocumented)
- **Class**: Different
- **Severity**: Medium
- **OStim** (`Events/EventListener.cpp:80-101`, tooltip `$ostim_tooltip_main_key`):
  - Out of a scene, pressing the key while looking at an actor starts a duo scene with player + crosshair actor at once, with no picker.
  - Holding the key for 2 s or more with no actor under the crosshair, then releasing it, starts a **solo** player scene.
  - In a player scene, the key opens **furniture selection**, to move the scene to other furniture (`Furniture::selectFurniture(... changeFurniture)`).
- **4Stim**:
  - Out of a scene, the key calls `FourStimMenu.OnHotkey` → the picker (`main.cpp:7144-7155`).
  - In a scene with the HUD open, it switches the HUD's keyboard focus.
  - No hold detection: grepped `HeldDuration` / held / hold in src. There are no hits for the hotkey.
  - SETTINGS.md:16 ("Start a scene; in one, give the HUD the keys") doesn't say OStim's key works differently. Solo scenes and furniture changes are reachable, but only through the picker or HUD Utility.
- **Fallout 4 constraint?**: No. `ButtonEvent::heldDownSecs` exists in CommonLibF4, and the furniture change already exists (FURNITURE.md:71).
- **Suggested fix**: keep the picker if the owner wants it, but document it as a departure naming OStim's behaviour. Add OStim's hold-for-solo, and make the in-scene key open "Change furniture..." (as OStim) or document why not.

### 3. `SetUseScreenShake` is not read, and shake is on by default (OStim: off)
- **Class**: Different
- **Severity**: Medium
- **OStim**:
  - `SetUseScreenShake` (0xE10) defaults to **0** (`MCMTable.h:230`).
  - It gates **every** camera shake (`Util/CameraUtil.cpp:8`): the climax shake, which is first person only (`ThreadActorClimax.cpp:100`, `shakeCamera(1.0, 2.0, true)`), and event shakes (`Core/Thread.cpp:573-577`).
  - So by default an OStim user sees no shake at all.
- **4Stim**: `fClimaxShake=1.0` (4Stim.ini:358) shakes the free camera at every climax in a scene you're in or watching (`main.cpp:3624-3633`). Event shakes are gated on `fClimaxShake > 0` (EVENTS.md:48). `setusescreenshake` is not in `OSTIM_SETTING_NAMES` (`main.cpp:302-394`), so an OStim user's `SetUseScreenShake=0` is ignored. SETTINGS.md:98 says only "OStim's shake is a fixed on / off". It doesn't say that it's off by default, or that it's first person only.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: default `fClimaxShake=0`, or read `SetUseScreenShake` as a master switch (0 = no climax or event shakes) defaulting to 0. Document that OStim's climax shake is first person only.

### 4. Excitement bars: OStim's four bar settings are missing, and the meters fade with the HUD
- **Class**: Partial
- **Severity**: Medium
- **OStim**:
  - Bars are a separate widget (`OBarsScript.psc`), configured by `enablePlayerBar`=1, `enableNpcBar`=1, `SetAutoHideBar`=0 and `SetMatchColorToGender`=0 (`MCMTable.h:240-243`; MCM `OSexIntegrationMCM.psc:1441-1447`).
  - With AutoHide off (the default), bars stay on screen the **whole scene** (`OBarsScript.psc:149-160`). With it on, they hide after 15 s without player input, and any key in a player scene shows them again (`EventListener.cpp:122-128`, `OSKSE.ShowBars`).
  - Colors are positional by default: dom blue, sub pink, third purple. They follow sex only when MatchColorToGender is on (`OBarsScript.psc:119-126`).
  - The player's bar and NPC bars can be turned off separately (`IsBarEnabled`, :212-222).
- **4Stim**:
  - The meters are part of the HUD root (`HUDPanel.as:87`). The whole `_root.alpha` fades after `fHUDFadeTime` 10 s (`HUDPanel.as:480-499`), so the meters disappear after 10 s by default, where OStim's stay.
  - The only switch is the theme's `show.actorMeters` (HUD_API.md:69), with no player / NPC split.
  - Colors always follow sex (`meterMale` / `meterFemale`).
  - None of the four names is read (not in `main.cpp` LoadSettings), and none is listed in SETTINGS.md.
- **Fallout 4 constraint?**: No.
- **Suggested fix**:
  - Add `enablePlayerBar`, `enableNpcBar` and `SetAutoHideBar` (default 0). Keep the meters visible when the rest of the HUD fades unless `SetAutoHideBar=1`, then hide them after 15 s idle.
  - Add `SetMatchColorToGender` (0 = positional colors), or document the sex coloring as a departure.

### 5. Auto-mode start conditions are missing: solo and "vanilla" scenes
- **Class**: Missing
- **Severity**: Medium
- **OStim**: `Thread::evaluateAutoMode` (`Core/Thread/ThreadAutoControl.cpp:95-117`).
  - NPC threads: auto mode always on.
  - Player threads: on if `SetAIControl`; else, for a one-actor scene, on if `SetForceAIForMasturbation` ("Always starts solo scenes in automatic mode"); else on if `SetForceAIInConsensualScenes` ("no dominant/submissive roles").
  - `SetForceAIIfAttacking` / `IfAttacked` are in the MCM (`OSexIntegrationMCM.psc:898-904`, greyed out when SetAIControl is on) but marked TODO in the code.
- **4Stim**: `[AutoMode]` has `SetAIControl` and `bAutoModeNPC` only (4Stim.ini). Grepped for ForceAI / masturbation / consensual in src, docs and the ini: none. Not in SETTINGS.md "Not in yet".
- **Fallout 4 constraint?**: No.
- **Suggested fix**: add `SetForceAIForMasturbation` (solo player scenes) and `SetForceAIInConsensualScenes` (player scenes without dom/sub; 4Stim has no aggressive scenes yet, so effectively every non-solo player scene). Read but document the two attacker settings as unused, as in OStim.

### 6. Per-actor and per-sex defaults (OStim's Actors page and menu options) are mostly missing
- **Class**: Partial
- **Severity**: Medium
- **OStim** MCM:
  - Player light mode next to male/female (`OSexIntegrationMCM.psc:358-362`).
  - Default and **player** strap-on (:1762-1764).
  - Default tongue male / female / **player** (:2393-2397).
  - **Player voice** (:2515).
  - An **Actors** page (:2855-2892): pick any actor and set their voice set, equip object per type, and **per-actor action and event stimulation / max-stimulation overrides** per role.
  - All are stored per form ID: base form, else race, else pseudo IDs 0x0 male / 0x1 female; the player's base form is 0x7 (`Trait/TraitTable.cpp:257-290`). They are exported as `actorData` (`Serial/Manager.cpp:301-345`).
- **4Stim**:
  - `sMaleLight` / `sFemaleLight` only; no player light (LIGHTS.md).
  - Equip objects chosen per actor in the HUD options only. ACTORS.md:129 documents "no MCM setting per race or sex".
  - No player voice (TODO.md:60 says "no per-actor voice set choice yet").
  - No per-actor stimulation overrides: grepped ActionStimulation / EventStimulation in src and docs, nothing. That part is undocumented.
- **Fallout 4 constraint?**: No.
- **Suggested fix**:
  - Add `sPlayerLight`.
  - Add default strap-on / tongue per sex and for the player (MCM dropdowns written to the ini, applied as OStim's lookup chain).
  - Add a player voice-set choice.
  - List the per-actor stimulation overrides in SETTINGS.md "Not in yet".

### 7. OStim's add-on settings (`settings` folder, Addons page) are not read
- **Class**: Missing
- **Severity**: Medium (add-on compatibility)
- **OStim**: `Data/SKSE/Plugins/OStim/settings/*.json` (`settings README.txt`). An add-on declares toggles, sliders, dropdowns and keymaps backed by **global variables**. OStim shows them on its MCM Addons page and writes the globals, and the values join export / import (`Settings/AddonPage.cpp:14-80`, `SettingTable.cpp:8-11`). C++ add-ons use `SettingInterface` (`PluginInterface/Settings/SettingInterface.h`).
- **4Stim**: no settings folder is read (grepped src for "settings" folder paths; only `Data/MCM/Config/4Stim/settings.ini`, `MCM.cpp:23`). The header comment `include/4Stim/PluginInterface.h:24` and TODO.md:59 say SettingInterface isn't there. The JSON settings folder is mentioned nowhere.
- **Fallout 4 constraint?**: Partly. The FO4 MCM is declarative (`config.json`), so a dynamic page can't be generated at runtime the way SkyUI's can. The globals part works as is: read `Data/F4SE/Plugins/4Stim/Settings/*.json`, apply defaults to the globals, and let add-ons ship their own MCM pages, or have `tools/make-mcm.py` merge them.
- **Suggested fix**: read the folder (set each global's default when missing; include them in export / import). Document that the MCM page itself has to come from the add-on or a regenerated config.

### 8. External-UI API is missing: OStim's `OstimNG-API` Thread interface
- **Class**: Not considered
- **Severity**: Medium
- **OStim**: `skse/src/ModAPI/OstimNG-API-Thread.h:180-200` is a DLL-export API (`RequestPluginAPI_Thread`, for UIs like OStim Prism). It offers:
  - `SetExternalUIEnabled(bool)`: OStim's Flash menus stop showing and stop taking controls (`UIState.cpp:20-23, 47-49`; `SceneMenu.cpp:33`, `AlignMenu.cpp:19`, `SearchMenu.cpp:73`).
  - `RegisterControlCallback`, where every navigation key is forwarded (`NotifyControlInput`, `UIState.cpp:14-17`).
  - `GetKeyData` (the configured keys).
  - Event callbacks, alignment get/set, and scene and navigation queries.
  - There is also a Scene API (`OstimNG-API-Scene.h`).
- **4Stim**: grepped ExternalUI / ControlInput / OstimNG_API / ModAPI in src, include and docs: nothing. 4Stim has a replaceable HUD movie (HUD_API.md) and the `FourStimAPI` interface map (listeners, alignment), but no way for another plugin to suppress 4Stim's HUD and receive its key presses, and no key-data query.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: add to `ThreadInterface` (or a new "UI" interface) `setExternalUIEnabled`, `registerControlListener(control, threadID)` and `getKeyData`, matching OStim's `Controls` enum. Document it in API.md.

### 9. Align tab: OStim's increments, controls and save timing differ (undocumented)
- **Class**: Different
- **Severity**: Low-Medium
- **OStim** (`UI/Align/AlignMenu.cpp`, `IncrementValue.cpp`):
  - Fields: X, Y, Z, scale, rotation, sosBend, wrapping with Up/Down (:200-209). Left/Right add or subtract the increment (:221-247).
  - Increments are **10 / 1 / 0.1 / 0.01 / 0.001**, default 1, cycled by the **No** control, which is the **pull-out key** while the Align menu is open (`EventListener.cpp:158-163`; tooltip `$ostim_tooltip_pullout_key`).
  - Scale changes by the same increment (1 → 2 at the default step) with no clamp (`Thread.cpp:401`). sosBend changes by 1.
  - **Toggle** = next actor (:211-219).
  - A scene-info panel shows the pack, scene id and **alignment key** (`UpdateSceneInfo`, :80-97).
  - Saved to `alignment.json` when the menu **closes** (`Hide`, :62-70).
- **4Stim** (`src/HUD.cpp:106-109, 739-761`; ALIGNMENT.md:21-33):
  - Steps 0.5 / 1 / 2 / 5 / 10, cycled on the Step row. Values snap to 0.1. Scale changes by step / 100, clamped to 0.5-2.
  - Pull-out in the Align tab still pulls out (`main.cpp:7118-7121`). Toggle goes to the tab buttons (Finding 12).
  - No scene id / pack / alignment-key display (`SendAlign`, HUD.cpp:701-737).
  - Saved on every change (an improvement).
  - ALIGNMENT.md doesn't name any of these as departures.
- **Fallout 4 constraint?**: No.
- **Suggested fix**:
  - Offer OStim's 0.001-10 ladder (keep 4Stim's scale / 100 but say so).
  - Make the pull-out key cycle the step while the Align tab has focus, and Toggle switch actors there.
  - Show the scene id and alignment key in the tab.
  - Document the remaining differences.

### 10. Export / import file differs from OStim's, and an OStim-style file imports nothing silently
- **Class**: Different
- **Severity**: Low-Medium
- **OStim**: `mcmsettings.json` is **flat**: `{ "SetKeyMap": 200, ..., "SetUndressingSlotMask": n, "actorData": {...}, toy settings, add-on settings }` (`MCMTable.cpp:63-81`, `MCMSetting::exportSetting`, `Serial/Manager.cpp:301`, `SettingTable::writeJson`). Import falls back to the legacy `JCUser/OstimMCMSettings.json` (`MCMTable.cpp:86-92`).
- **4Stim**: `MCM::Export` writes `{ "<ini section>": { "<key>": "<string>" } }` (`src/MCM.cpp:403-433`). `Import` skips any top-level value that isn't an object (`MCM.cpp:456-459`), so a flat OStim-layout file imports 0 settings and still "succeeds". Not exported: the chosen equip objects (saved in the co-save only), toy settings beyond the ini. MCM.md "Export and import" says "As OStim's" but the layout differs.
- **Fallout 4 constraint?**: No. (Key codes are not portable between the games, but most other values are.)
- **Suggested fix**: write OStim's flat layout (OStim names at the top level, 4Stim-only names alongside) and accept both on import. Warn when nothing was imported. Document what isn't carried.

### 11. MCM volume sliders go to 2, but the plugin clamps to 1
- **Class**: Different (bug)
- **Severity**: Low
- **OStim**: moan and sound volume sliders 0.1-1 (`OSexIntegrationMCM.psc` OID_MoanVolume / OID_SoundVolume ranges).
- **4Stim**: `config.json` `f4S_moanVolume` / `f4S_soundVolume` sliders run 0-2 (config.json ~2322, ~2340). `LoadSettings` clamps both to 0-1 (`main.cpp:512-517`), so the upper half of the slider does nothing.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: set the slider max to 1 in `tools/make-mcm.py`, or allow >1 in the plugin and say so.

### 12. The navigation "Toggle" key means something else
- **Class**: Different
- **Severity**: Low
- **OStim**: Toggle (numpad 9) is used only in the Align menu, to change actor (`AlignMenu.cpp:211-219`; tooltip `$ostim_tooltip_osa_tog`: "Currently only used in the alignment menu").
- **4Stim**: Toggle moves between the list and the tab buttons (`HUDPanel.as:314-315`; SETTINGS.md row `SetOsaYesKey / SetOsaTogKey`). That is a different meaning under OStim's name, not flagged as a departure.
- **Suggested fix**: in the Align tab make Toggle switch actors, as OStim. Document the extra list/tab use.

### 13. NPC scene key: OStim's 0 means unbound; 4Stim's 0 means Shift + hotkey, and the behaviour differs
- **Class**: Different
- **Severity**: Low
- **OStim**: `keyNpcSceneStart` defaults to 0 (`MCMTable.h:178`) and keyCode 0 is rejected as invalid (`EventListener.cpp:75`), so OStim ships with **no** NPC-scene key. When set, it does nothing during a player scene (:103-106). It starts a scene with the crosshair actor and asks (message boxes) whom to add (`DebugNPCThreadStarter.cpp:12-40`).
- **4Stim**: 0 = Shift + `SetKeyMap` (4Stim.ini `keyNpcSceneStart=0`). It works during your own scene and opens the picker (TODO.md:32; `main.cpp:7135-7142`). SETTINGS.md doesn't mention that OStim's 0 is "none".
- **Suggested fix**: document it in SETTINGS.md (OStim name, different meaning of 0), or use a separate 4Stim value for "Shift + hotkey".

### 14. Undressing: weapon and wig settings are missing, the docs mis-state OStim, and `SetUndressingSlotMask` has different bit semantics
- **Class**: Missing / Documented departure (wrong reason)
- **Severity**: Low
- **OStim**:
  - `SetRemoveWeaponsAtStart`=1 unequips weapons for every scene actor at start (`ThreadActor.cpp:57-59`, `removeWeapons` :593-601), whatever the undress settings. They are re-equipped at the end.
  - `SetRemoveWeaponsWithSlot`=32: partial undressing of that slot also removes weapons (`Thread.cpp:274-276`).
  - `undressWigs`=0 (`ThreadActor.cpp:122, 575`).
- **4Stim**: UNDRESS.md:98 says "**OStim leaves weapons alone**, Skyrim's animations hide them". That is false: 4Stim's always-remove behaviour actually **matches** OStim's default. There's no toggle for `SetRemoveWeaponsAtStart`, and `SetRemoveWeaponsWithSlot` and `undressWigs` aren't read.
  - `SetUndressingSlotMask` is read with Fallout 4 bit meanings (`main.cpp:769-778`). OStim's value is a Skyrim mask; its default 0x3D8BC39D read as FO4 bits would take off Hair Top (30) and leave 35/36 on. So a value carried over from OStim does the wrong thing, though the name suggests it means the same.
- **Fallout 4 constraint?**: Wigs: FO4 has no hair-tint wig convention; n/a, say so. Weapons: no constraint.
- **Suggested fix**: fix the UNDRESS.md sentence, and add `SetRemoveWeaponsAtStart` (default 1). In SETTINGS.md / UNDRESS.md, say `SetUndressingSlotMask` uses FO4 slot bits (or map a Skyrim mask through the slot table).

### 15. Climax blur fires on any climax; OStim's only on the player's
- **Class**: Different
- **Severity**: Low
- **OStim**: in a player thread, the blur (`OStimNutEffect`) applies only `if (isPlayer)`. Slow motion, shake and rumble apply on any climax in the player thread (`ThreadActorClimax.cpp:83-104`).
- **4Stim**: `PlayClimaxEffects` blurs on any climax in a scene you're in **or watching** (`main.cpp:3614-3640`). EXCITEMENT.md:23 doesn't mention OStim's player-only blur.
- **Suggested fix**: blur only when the player climaxes, or document the change.

### 16. Settings are per install, not per save; OStim's auto export / import is missing
- **Class**: Different (undocumented)
- **Severity**: Low
- **OStim**: settings live in global variables in the save (`MCMSetting::setup(formID)`), so each save has its own. `autoExportSettings` exports on MCM close (`OSexIntegrationMCM.psc:99-101`). `autoImportSettings` imports on every load, and a new game always imports (`$ostim_tooltip_auto_import`). There's also "reset to defaults" (`OID_ResetSettings`, :377 → `MCMTable::restoreDefaults`).
- **4Stim**: ini plus MCM file, global for every save (MCM.md "Where the settings go"). That probably makes the two auto settings unnecessary, but neither MCM.md nor SETTINGS.md says that this is a departure, or why the two OStim settings are absent.
- **Suggested fix**: one sentence in MCM.md naming the difference and the two settings.

### 17. A navigation or auto-mode scene change doesn't bring back a faded HUD (OStim's does)
- **Class**: Different
- **Severity**: Low
- **OStim**: `SceneMenu::UpdateMenuData` runs on every node change (`UIState.cpp:91-102`). `Option_Box_MC.AssignData` calls `FadeTrigger()` whenever there are options (`flash/Option_Box_MC.as:327-331`), so the menu fades back in for `fadeTime` on each new scene (auto mode, Papyrus navigation).
- **4Stim**: `HUDPanel.SetScene` wakes the HUD only when the previous scene id was empty, i.e. the first scene (`HUDPanel.as:131-135`). Later scene changes leave it faded.
- **Suggested fix**: `wake()` on any scene id change, without taking focus.

### 18. `addActorsAtStart` has no switch
- **Class**: Missing
- **Severity**: Low
- **OStim**: `addActorsAtStart`=1 asks with message boxes whether to add nearby eligible actors within 2000 units, unless the scene has a starting animation (`PlayerThreadStarter.cpp:84-105`). 0 skips the question.
- **4Stim**: the picker always offers NPCs within `fActorRadius`=1500 (4Stim.ini). There's no way to skip the step, and it's not mapped or documented.
- **Suggested fix**: read `addActorsAtStart` (0 = skip the "add actors" step). Note the radius difference, or default `fActorRadius` to 2000.

### 19. Stale or contradictory docs in this area
- **Class**: Different (documentation)
- **Severity**: Low
- SETTINGS.md:10 says "The defaults of the keys are 4Stim's: N for scenes, Shift + N..., `=` / `-` for speed, U for auto mode. OStim's other keys are there, unbound." That's wrong: the table below it and `4Stim.ini` ship OStim's keys (Up arrow, numpad + / -, numpad 0, all bound).
- ACTORS.md:129 and OPTIONS.md:53 say equip-object choices last "until a game is loaded", and TODO.md:45 says "Still to do: ... kept in the save". But they **are** saved in the co-save (`src/EquipObjects.cpp:66-69`, `main.cpp:9037-9045`; TODO.md:60 agrees).
- `main.cpp:173` and 4Stim.ini `[Controller]` say OStim "takes its keys from the keyboard map". OStim's key map takes **gamepad and mouse buttons** too (keyCode 256+ for mouse, `gamepadMaskToKeyCode` for pads, `EventListener.cpp:67-73`). 4Stim can't bind mouse buttons at all.
- SETTINGS.md doesn't say that `useSoSSex`, `useTNGSex`, `futaUseMaleLight` and `fixDarkFace` are deliberately absent (no SoS / TNG; a Skyrim-only bug).
- MCM.md's "Only in 4Stim.ini" list omits `bCollisionWatch`, `bFurnitureOutOfTheWay`, `sPenisBendUp` / `Down` / `sPenisFlaccid` and `sIntifaceAddress`, which also have no MCM control (config.json dump). SETTINGS.md's own-settings list omits `bFurnitureOutOfTheWay`, `bCollisionWatch` and the penis-bend keys.
- **Suggested fix**: correct these lines.

### 20. Options menu: plain options get the state's name as `asState`; OStim passes ""
- **Class**: Different
- **Severity**: Low
- **OStim**: `MenuOption.currentState` stays "" for options that aren't equip objects (`UI/Scene/SceneOptions.h:13`). `ExecuteOption` passes `currentState`, so `asState == ""`, even though the description shown is the first state's (`SceneOptions.cpp:42-60`).
- **4Stim**: passes the shown state's name, e.g. "0" for OStim's own debug option (`src/Options.cpp:403, 480-481`). An OStim add-on that tests `asState == ""` behaves differently. Also note OStim's `stateFaction` is parsed but unused, and `states` is an unordered_map, so OStim's "first" state is unspecified. 4Stim's file-order choice is fine.
- **Suggested fix**: pass "" for options without a 4Stim `"state"` and outside `equipObject` repeats, as OStim. Document it.

### 21. Speed display doesn't show the scene's `displaySpeed`
- **Class**: Different
- **Severity**: Low
- **OStim**: `SceneMenu::UpdateSpeed` shows `speeds[speed].displaySpeed` as text, with up / down arrows enabled per end, and hides the speed for single-speed scenes (`SceneMenu.cpp:144-166`).
- **4Stim**: HUD `SetSpeed(level, count)` sends only the index (HUD_API.md:246), so an OStim pack's display speeds (e.g. 0.5 / 1 / 1.5) aren't shown.
- **Suggested fix**: add the optional `displaySpeed` to `SetSpeed`.

---

## Checked and matching
- All OStim key defaults, translated DirectX→VK; the speed, pull-out, end, auto-mode, free-cam, search, align and hide keys act on the focused scene (OStim: player thread only; 4Stim extends to watched scenes, documented in TODO.md:32).
- Every MCMTable setting marked ✓ above: name, default and unit (ms in the ini, seconds in the MCM as OStim's MCM). Furniture-distance steps (value + 1) x 100 and clutter radius x 100 match `MCMTable.h:73`.
- MCM page coverage of the implemented settings: General, Camera, Furniture, Roles, Alignment, HUD, Excitement, Climax effects, Auto mode, Undressing plus Undressing slots (OStim's per-slot checkboxes), Faces and sound, Lights, Sex toys, Hotkeys (OStim's Controls layout: scene keys plus navigation keys), Export / Import / back to ini (≈ OStim's reset).
- The UI fade (`fadeTime` 10 s, `ui_settings.json`) → `fHUDFadeTime` 10. OStim's UI positions and scales → theme `layout.anchor/offset/scale/opacity`.
- The hide-UI key toggles the HUD, as OStim's `KEY_HIDE`. The align key toggles Align/Navigation, as `ToggleActiveMenu(kAlignMenu)`.
- The options file format: pages, parent, repeat actor, equipObject toggle and object states, `{actor}` / `{type}`, function signatures (state), (state, actor), (state, actor, object), a duplicate page skipped, `OStim/` icon paths mapped, and a Back entry (OStim's "back" / "exit").
- Search excludes transitions and filters by actor count and furniture, as OStim's `SearchMenu::Search`. The navigation list hides for NoPlayerControl (`main.cpp:5676-5710`).
- Excitement, auto mode, furniture, roles, futa and alignment-group settings: names and defaults identical, all read by LoadSettings (`main.cpp:470-880`).
- Documented gaps not repeated as findings: `SetCustomTimescale`, dialogue settings, `SetBedRealignment` / `bedOffset`, per-toy and per-slot toy settings, the SettingInterface (header), and lights not being equip objects.
