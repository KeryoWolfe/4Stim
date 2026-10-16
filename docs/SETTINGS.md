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
| `SetUseAutoFades` | 0 | | Also fade when auto mode jumps to another scene |
| `SetUseIntroScenes` | 1 | | Scenes with you start with an `intro` scene |
| `NPCSceneDuration` | 300000 ms | | Scenes without you end after this long |
| `SetResetPosition` | 1 | `bResetPosition` | NPCs go back to where they stood |
| `SetScaling` | 0 | | 1 = don't scale actors to the animation (`SCALING.md`) |
| `SetOnlyGayAnimsInGayScenes` | 1 | `bMatchSex` | A role goes only to the sex it's meant for (type and requirements always count; `ACTORS.md`) |
| `unrestrictedNavigation` | 0 | | Only the actor type counts for roles |
| `PlayerAlwaysDomStraight` / `SubStraight` | 0 / 0 | | You take the first / second role with a man and a woman |
| `PlayerAlwaysDomGay` / `SubGay` | 0 / 0 | | The same, with two of the same sex |
| `playerSelectRoleStraight` / `Gay` / `Threesome` | 0 / 1 / 0 | | The picker asks which role you take |
| `futaUseMaleRole` / `Excitement` / `Climax` | 1 / 0 / 1 | | Women with a penis take men's roles, the male excitement rate, a man's climax |
| `unequipStrapOnIfNotNeeded` | 0 | | Strap-ons come off in any role that doesn't need one |
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
| `alignmentGroupBySex` | 1 | | Alignments kept per sex of the actors |
| `alignmentGroupByHeight` | 0 | | And per height (scale) |
| `alignmentGroupByHeels` | 1 | | And per heel height (High Heels System) |
| `noFacialExpressions` | 0 | | No facial expressions (`EXPRESSIONS.md`) |
| `SetExpressionDurationMin` / `Max` | 1000 / 3000 ms | | How long a picked expression stays |
| `moanIntervalMin` / `Max` | 2500 / 4000 ms | | Time between moans (`SOUND.md`) |
| `moanVolume` | 1 | | Moan and climax volume, 0-1 |
| `soundVolume` | 1 | | Action and event sound volume, 0-1 (`SOUND.md`) |
| `SetOnlyLightInDark` | 0 | | Lights only in the dark (`LIGHTS.md`) |

Older 4Stim names in seconds (`fAutoModeSceneMin`, `fExcitementDecayGrace`...) are still read in seconds.

## 4Stim's own settings

These have no OStim counterpart, and keep their names: `bResumeScenes` (0, as OStim, which ends scenes on load), `sTargetMode`, `fMaxDistance`, `fCrosshairCone`, `fProximityRadius`, `fActorRadius`, `fFurnitureHeight`, `fTransitionLead`, `bLockScenes`, `bGuardScenes`, the logging switches, `[HUD]` (including `sHUDControls`, which offers OStim's control scheme as `Columns`), `bEnableExcitement`, `fClimaxEndDelay`, `fClimaxShake` (OStim's shake is a fixed on / off), `fClimaxBlur` (the blur's strength), `fClimaxFlash`, `bAutoModeNPC` (OStim's NPC scenes always use auto mode), `bAutoModeStandingOnFloor`, `bUndress`, `bUndressPlayer`, `bRedress`, `sUndressSlots` (Fallout 4's slots), `fUndressItemDelay` (0, all at once as OStim), `bSounds` (OStim has no off switch for sound), `iSoundFlags` / `iSoundPriority` (how Fallout 4 plays sound files; OStim plays sound descriptors), `sMaleLight` / `sFemaleLight` (OStim chooses lights in its MCM) and `fLightLevelDark` (80; OStim's is a fixed 20, on Skyrim's scale).

## Not in yet

OStim settings that come with features 4Stim doesn't have yet: clutter reset (`SetResetClutter`), the custom time scale (`SetCustomTimescale`), dialogue, strap-ons and other equip objects, and the rest of OStim's MCM. They'll take OStim's names when they come.
