# Settings

All of 4Stim's settings are in `Data\F4SE\Plugins\4Stim.ini`, with a comment on each. Edit it with the game closed.

## OStim's names

Where OStim NG has the same setting, 4Stim uses OStim's name for it (the names in OStim's MCM settings export, `MCMTable.h`), with the same default and the same meaning, so anyone coming from Skyrim can look a setting up in OStim's docs. The names 4Stim used before still work. Two things differ from OStim:

- **Keys** are Windows virtual-key codes (N = `0x4E`), not Skyrim's DirectX scan codes. `SetKeyUp=78` in OStim is numpad +; in 4Stim, 78 is N.
- **The defaults of the keys** are 4Stim's: N for scenes, Shift + N for NPC scenes, `=` / `-` for speed, U for auto mode. OStim's other keys are there, unbound.

Times given with OStim's names are in milliseconds, as in OStim. `SetFurnitureSearchDistance` is in OStim's steps: (value + 1) x 100 units.

| OStim name | Default | Older 4Stim name | What it does |
|---|---|---|---|
| `SetKeyMap` | `0x4E` (N) | `iHotkey` | Start a scene; in one, give the HUD the keys |
| `keyNpcSceneStart` | 0 (Shift + `SetKeyMap`) | `iNPCSceneKey` | Start a scene between NPCs |
| `SetKeyUp` / `SetKeyDown` | `0xBB` / `0xBD` | `iSpeedUpKey` / `iSpeedDownKey` | Faster / slower |
| `SetControlToggle` | `0x55` (U) | `iAutoModeKey` | Auto mode on / off |
| `SetPullOut` | 0 | | Pull out |
| `SetOsaEndKey` | 0 | | End the scene |
| `keySearch` | 0 | | Search every scene for this many actors |
| `keyAlignment` | 0 | | The HUD's Align tab |
| `SetFreeCamToggleKey` | 0 | | Free camera on / off |
| `keyHideUI` | 0 | | Hide / show the HUD |
| `SetUseFreeCam` | 1 | | Free camera in scenes with you |
| `SetCameraSpeed` | 3 | `fFreeCameraSpeed` (a multiplier) | Free camera speed (the game's `fFreeCameraTranslationSpeed`) |
| `SetFreeCamFOV` | 45 | | Field of view during the scene |
| `SetForceFirstPerson` | 0 | | First person after the scene |
| `SetUseFades` | 1 | | Fade to black as scenes with you start and end |
| `NPCSceneDuration` | 300000 ms | | Scenes without you end after this long |
| `SetResetPosition` | 1 | `bResetPosition` | NPCs go back to where they stood |
| `SetOnlyGayAnimsInGayScenes` | 1 | `bMatchSex` | Only scenes whose roles the actors fit |
| `SetFurnitureSearchDistance` | 15 (1600 units) | `fFurnitureRadius` (units) | How far to look for furniture |
| `SetsexExcitementMult` / `SetFemaleSexExcitementMult` | 1 / 1 | `fMaleExcitementMult` / `fFemaleExcitementMult` | Excitement multipliers |
| `excitementDecayRate` | 0.5 | `fExcitementDecayRate` | Excitement lost per second above the ceiling |
| `excitementDecayGracePeriod` | 5000 ms | `fExcitementDecayGrace` (s) | Wait before it falls |
| `postOrgasmExcitement` / `Max` | 10 / 30 | `fPostClimaxExcitement` / `Max` | A woman's excitement after a climax |
| `SetAutoClimaxAnims` | 1 | `bClimaxScenes` | Play climax animations |
| `endOnPlayerOrgasm` | 0 | `bEndOnPlayerClimax` | End when the player climaxes |
| `SetEndOnOrgasm` | 1 | `bEndOnMaleClimax` | End when a man climaxes |
| `SetEndOnSubOrgasm` | 0 | `bEndOnFemaleClimax` | End when a woman climaxes |
| `SetEndOnBothOrgasm` | 0 | `bEndOnAllClimax` | End once everyone has |
| `endNPCSceneOnOrgasm` | 1 | `bEndNPCScenesOnClimax` | NPC scenes end at the first climax |
| `SetSlowMoOrgasms` | 1 | | Slow motion at a climax |
| `SetBlurOrgasms` | 1 | | Blur at a climax |
| `SetUseRumble` | 1 | `bClimaxRumble` | Controller rumble |
| `SetAIControl` | 0 | `bAutoModePlayer` | Auto mode in scenes with you |
| `autoModeAnimDurationMin` / `Max` | 7500 / 15000 ms | `fAutoModeSceneMin` / `Max` (s) | Time in each scene |
| `autoModeForeplayChance` | 35 | `iForeplayChance` | Percent starting with foreplay |
| `autoModeForeplayThresholdMin` / `Max` | 15 / 35 | `fForeplayEndMin` / `Max` | Excitement that ends foreplay |
| `autoModePulloutChance` | 75 | `iPulloutChance` | Percent pulling out |
| `autoModePulloutThresholdMin` / `Max` | 80 / 90 | `fPulloutMin` / `Max` | Excitement to pull out at |
| `navigationDistanceMax` | 5 | `iAutoModeMaxSteps` | Navigations auto mode walks |
| `autoModeLimitToNavigationDistance` | 1 | `bAutoModeLimitToNavigation` | During sex, only scenes it can walk to |
| `SetActorSpeedControl` | 1 | `bAutoSpeed` | Auto mode speeds up |
| `autoSpeedControlIntervalMin` / `Max` | 2500 / 7500 ms | `fAutoSpeedIntervalMin` / `Max` (s) | How often |
| `autoSpeedControlExcitementMin` / `Max` | 15 / 85 | `fAutoSpeedExcitementMin` / `Max` | Chance from 0 to certain |
| `SetAlwaysUndressAtStart` | 0 | `bUndressAtStart` | Everything off at the start |
| `SetPartialUndressing` | 1 | `bPartialUndress` | What each action needs |
| `SetUndressIfNeed` | 1 | `bFullUndressMidScene` | Everything off for actions that need it |
| `SetAnimateRedress` | 0 | `bAnimateRedress` | Redress animations after a scene |

Older 4Stim names in seconds (`fAutoModeSceneMin`, `fExcitementDecayGrace`...) are still read in seconds.

## 4Stim's own settings

These have no OStim counterpart, and keep their names: `bResumeScenes` (0, as OStim, which ends scenes on load), `sTargetMode`, `fMaxDistance`, `fCrosshairCone`, `fProximityRadius`, `fActorRadius`, `fFurnitureHeight`, `fTransitionLead`, `bLockScenes`, `bGuardScenes`, the logging switches, `[HUD]`, `bEnableExcitement`, `fClimaxEndDelay`, `fClimaxShake` (OStim's shake is a fixed on / off), `fClimaxBlur` (the blur's strength), `fClimaxFlash`, `bAutoModeNPC` (OStim's NPC scenes always use auto mode), `bAutoModeStandingOnFloor`, `bUndress`, `bUndressPlayer`, `bRedress`, `sUndressSlots` (Fallout 4's slots) and `fUndressItemDelay` (0, all at once as OStim).

## Not in yet

OStim settings that come with features 4Stim doesn't have yet: intro scenes (`SetUseIntroScenes`), auto mode fades (`SetUseAutoFades`), clutter reset (`SetResetClutter`), the custom time scale (`SetCustomTimescale`), alignment grouping, sounds, expressions, strap-ons, and the rest of OStim's MCM. They'll take OStim's names when they come.
