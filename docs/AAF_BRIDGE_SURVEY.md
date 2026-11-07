# AAF bridge survey

**Question:** can "AAF Stub (4Stim)" (today an empty `AAF.esm`, 185 bytes) become a bridge that answers AAF's scripting API with 4Stim scenes, so the AAF add-ons people already have run on 4Stim?

**Short answer: yes, and the surface is small.** The 17 installed add-ons that script against AAF use 6 of AAF's ~40 API functions, 5 of its 15 events, and 4 of its keywords. None uses equipment sets, morphs, overlays, MFG, banners, stats, `ChangePosition` or `StopScene`. The hard parts aren't the API: they are AAF's **tag vocabulary** (add-ons pick and react to scenes by AAF tags, which 4Stim scenes don't carry), **actor order** in event arguments, and **status codes** add-ons react to.

Surveyed 7 November 2026 from the owner's MO2 mods folder (`D:\Naked Commonwealth\mods`). Sources: each mod's `.psc` where shipped; the BA2 or `.pex` string tables where not; every plugin's masters and its references into AAF.esm. Nothing here has been built or tested.

## 1. What's installed, and what each add-on uses

Enabled in the current profile: only **Commonwealth Captives** (+ its NPC Patch) and **Torture Devices** (which has no AAF scripting). Everything else below is installed but disabled. The original AAF Sexual Harassment (and its Gay / Gender Neutral versions) wasn't scanned: Sexual Harassment + is its successor and replaces it.

Calls are to `AAF:AAF_API` (the script on `AAF_MainQuest`, 0x000F99). `Settings` lists the `SceneSettings` fields the add-on sets. `Args` lists the event argument slots it reads.

| Add-on | Calls | Events listened to | Settings fields set | Args read | AAF.esm forms |
|---|---|---|---|---|---|
| AAF Violate | GetVersion, GetBuild, GetSceneSettings, StartScene, GetProtectedEquipmentKeywords, GetAAFStatus | OnAAFReady, OnSceneInit, OnAnimationStart, OnAnimationStop, OnSceneEnd | duration, preventFurniture, scanRadius, furniturePreference, locationObject, isNPCControlled, usePackages, skipWalk, includeTags, excludeTags, meta | 0-3 | ActorBusy (conditions on INFO/PACK + property), ActorLocked, GenderOverride_Female (plugin master) |
| AAF Sex Em Up | GetVersion, GetBuild, GetSceneSettings, StartScene | OnAAFReady, OnSceneInit, OnAnimationStart, OnAnimationChange, OnAnimationStop | position (None), duration, scanRadius, furniturePreference, preventFurniture, usePackages, skipWalk, isNPCControlled, locationObject, includeTags, meta | 0-9 | ActorBusy (GetFormFromFile; also removes it from the player as a "stuck" fix) |
| Sexual Harassment + | GetSceneSettings, StartScene, GetProtectedEquipmentKeywords | OnAnimationStart, OnAnimationStop | position (None), duration, usePackages, preventFurniture, isNPCControlled, locationObject, skipWalk, includeTags, excludeTags, meta | 0, 1, 3 | ActorBusy, ActorBlocked, ActorLocked (GetFormFromFile) |
| AAF Bound In Public | GetSceneSettings, StartScene, GetProtectedEquipmentKeywords | OnAnimationStart, OnAnimationStop | duration, usePackages, locationObject, excludeTags, meta | 1 | API only |
| Commonwealth Captives | GetVersion, GetBuild, GetSceneSettings, StartScene | OnSceneInit, OnAnimationStop | duration, skipWalk, preventFurniture, isNPCControlled, locationObject, excludeTags, meta | 0, 3, 4, 5 | ActorLocked (plugin master) |
| Morning Sex Alternative | GetSceneSettings, StartScene | none | preventFurniture, locationObject, furniturePreference, scanRadius, includeTags, excludeTags, duration, usePackages, skipWalk, isNPCControlled, meta | none | ActorBusy, ActorLocked |
| AML Ivy Real Sex | GetVersion, GetSceneSettings, StartScene | OnSceneInit, OnSceneEnd | meta, usePackages, skipWalk, isNPCControlled, duration, includeTags, excludeTags, combinedTags, preventFurniture | 0-5 | ActorBusy (plugin master) |
| Autonomy Enhanced Redux (BA2, no source) | GetAPI, GetSceneSettings, StartScene | OnAnimationStart, OnAnimationStop | duration, furniturePreference, includeTags, excludeTags, skipWalk, preventFurniture, isNPCControlled | n/a | plugin master, no form references |
| Brothel / HotC / Magnolia sex scenes (same author, 3 mods) | GetVersion, GetSceneSettings, StartScene | OnSceneEnd | skipWalk, preventFurniture, furniturePreference, scanRadius, locationObject, isNPCControlled, duration, excludeTags, meta (reads combinedTags) | 0-4 | AAF_MainQuest as a filled property |
| AAF Family Planning Enhanced | none (listens only) | OnAnimationStart, OnAnimationChange, OnAnimationStop | none | 0, 1, 3 | GetFormFromFile 0x01000F99 |
| AAF Dirty Sex | GetVersion, GetBuild | OnSceneInit, OnAnimationStop | none | 0-4 | API only |
| AAF Spectators | GetVersion, GetBuild | OnAnimationStart, OnSceneEnd | none | 0, 3, 4 | ActorBusy, ActorLocked (plugin master) |
| Devious Devices | none | OnSceneInit, OnAnimationStart, OnAnimationStop | none | 0-2 | ActorBusy property |
| Sex Attributes | none | OnAnimationStart, OnAnimationStop | none | 1-4 | API only |
| Sex Education (+ Sex Stats tab) | GetVersion, GetBuild | OnAnimationStart, OnAnimationStop | none | 0-4 | API only |

**No AAF scripting:** Torture Devices (its own idles; ships AAF animation / furniture / position / tag XML), Mutated Lust (AAF XML only), Kinky Custom Kollars, Legalize The Commonwealth 2, Depravity, SS2 Settlers at Play, the three Hookers of the Commonwealth editions (pre-AAF; the "AAF HotC Sex Scenes" patch is the AAF part), QUAAF / Extended / Ultimate AAF patches (XML).

### Totals

- **Functions used:** `StartScene` (11 add-ons), `GetSceneSettings` (11), `GetVersion` / `GetBuild` (10, version checks), `GetProtectedEquipmentKeywords` (3), `GetAAFStatus` (1, a debug readout), `GetAPI` (1). **Unused:** StopScene, StopSceneWithAbruptStop, ChangePosition, GetPositionSettings, every equipment / morph / overlay / MFG / banner / narration / stat / relationship / setting / actor-data / position-data / furniture-list / actor-type function, and all legacy functions (QuickScene, StartSceneByPosition, roles, GetAttraction).
- **Events used:** `OnAnimationStop` (11), `OnAnimationStart` (10), `OnSceneInit` (6), `OnSceneEnd` (6), `OnAnimationChange` (2), `OnAAFReady` (2). **Unused:** OnJoinAnimation, OnStatEvent, OnActorData, OnCustomCommand, OnPositionData, OnAddCancel, OnStageEvent, OnWalkInit, OnFurnitureListLoaded.
- **SceneSettings fields used:** duration, preventFurniture, usePackages, skipWalk, isNPCControlled, locationObject, scanRadius, furniturePreference, includeTags, excludeTags, combinedTags, meta, position (always `None`). **Unused:** formation, megaScene, scanLocation, playOnce, start/stopEquipmentSet, isRootLocked, ignoreCombat, ignoreActorLocations, keepPowerarmor.
- **AAF.esm forms used:** `AAF_MainQuest` 0x000F99 (with `AAF:AAF_API` attached), `AAF_ActorBusy` 0x00915A, `AAF_ActorLocked` 0x017CEA, `AAF_ActorBlocked` 0x022BB0, `AAF_GenderOverride_Female` 0x0121BC (Male is 0x0121BB). Twelve plugins have AAF.esm as a master.

## 2. What the bridge has to be

The stub must keep the name `AAF.esm` (twelve plugins master it; scripts call `Game.GetFormFromFile(0x0F99, "AAF.esm")`), and it can't coexist with real AAF, as today.

1. **A generated AAF.esm** with the same records at the same FormIDs: `AAF_MainQuest` (0x000F99, start-game-enabled, script `AAF:AAF_API`) and the keywords above, plus the protected-equipment form list. Records written by a 4Stim tool (as `make-equip-plugin.py` does), not copied out of AAF.esm.
2. **A script named `AAF:AAF_API`** (`Scripts\AAF\AAF_API.pex`) with the same public surface: the two structs with the same field names and types, the 15 custom events, the functions add-ons are compiled against, with the same names and signatures. Compiled add-ons bind by name, so every function in AAF's API must at least exist (returning a harmless default), or calls into a missing one fail at runtime. Only the ones in §1 need real behaviour.
3. **A listener** that turns every 4Stim scene's events into AAF's: `FourStim_OnSceneStart` → `OnSceneInit` + `OnAnimationStart`, `FourStim_OnSceneChange` → `OnAnimationChange`, `FourStim_OnSceneEnd` → `OnAnimationStop` + `OnSceneEnd`. **Every** scene, not only ones the bridge started: that is what makes Family Planning, Sex Attributes, Sex Education, Dirty Sex, Spectators and Devious Devices work with scenes started from 4Stim's own menu.
4. **Keywords on actors:** `AAF_ActorBusy` on every actor in a scene (Violate's dialogue and packages condition on it; most add-ons skip busy actors), removed at the end, including after a load (SEU ships a "remove stuck AAF_ActorBusy" fix because AAF sometimes left it). Actors with `AAF_ActorLocked` / `AAF_ActorBlocked` are refused by StartScene. `AAF_GenderOverride_*` should feed 4Stim's sex override (`ActorProperties`, the `agender` / sex list).

**Licence.** AAF_API.psc says "Copyright All Rights Reserved". The bridge reimplements the interface (names, signatures, struct fields, event argument layout, FormIDs) from scratch, writes no AAF code, and ships nothing from AAF's archive. That is the same footing as the existing stub. Worth a sentence in the README, and asking AAF's author is still the clean route.

## 3. Function mapping

| AAF | 4Stim | Notes |
|---|---|---|
| `GetAPI()` | the quest's own script | Global, returns the 0x0F99 quest cast to AAF_API |
| `GetVersion()` / `GetBuild()` | constants | Add-ons gate on a minimum (Violate, SEU, Captives). Return a value at or above the newest AAF they check: read their checks before choosing |
| `GetAAFStatus()` | 0 if not running, else ready + 1 | as AAF; Violate only prints it |
| `GetSceneSettings()` | a fresh struct with AAF's defaults | AAF's: duration = its default (MCM), usePackages True, preventFurniture False, excludeTags `"default_excludetags"`, the rest None/0/False. Use 4Stim's duration setting for duration |
| `StartScene(actors, settings)` | `FourThreadBuilder`: `Create`, `SetDuration`, `SetFurniture` / `NoFurniture`, `NoPlayerControl` (isNPCControlled), `SetMetadataCSV` (meta), `SetStartingAnimation` (a scene picked from the tags), `Start` | The scene is picked from the tags (§4). `locationObject` = furniture if it's furniture, else the place to play (an actor or marker). `scanRadius` / `furniturePreference` = 4Stim's furniture search. `skipWalk` / `usePackages`: 4Stim doesn't walk actors to the spot; accept and ignore (document). Returns nothing in AAF: the result arrives as `OnSceneInit` |
| `StopScene(actor)` | `FourThread.Stop` | Unused by installed add-ons; cheap to support |
| `GetProtectedEquipmentKeywords()` | the bridge's form list | Filled from 4Stim's no-strip keywords **plus** add-ons' `*_protectedEquipmentData.xml` (DD, Captives, BIP ship them). 4Stim's undress should honour the same list |
| everything else | stub | Returns None / 0 / False, logs once |

## 4. Tags: the real work

AAF add-ons choose animations by **AAF tags**, and several decide what to do from the tags in event args. 4Stim scenes have OStim's scene tags, actor tags and actions instead.

**Tags add-ons ask for (includeTags / excludeTags / combinedTags):** `Aggressive`, `Rough`, `Neutral`, `Masturbation`, `Sequential` (Violate's bad end), `LegsTied` / `LegsFree`, `PenisToVagina`, `PenisToAnus`, `PenisToMouth`, `PenisToEither`, `StrapOnToVagina` / `Anus` / `Either` (Ivy), and to exclude: `Spanking`, `Jackoff`, `Pose`, `Utility`, `Tease`, `Foreplay`, `Finish`, `TitJob`, `Missionary`, `FromFront`, `Kissing`, `Hugging`, `MouthToMouth`, `FFrame_MR`, plus AAF's `default_excludetags`.

**Tags add-ons read from events:** `Pose`, `NonSex`, `Kissing`, `Hugging` (Spectators, Dirty Sex), Family Planning's watch list (set in its plugin), Captives' and Sex Attributes' checks.

**Plan:** a mapping table from 4Stim's actions and tags to AAF's, both ways:
- actions → AAF: `vaginalsex` → `PenisToVagina`, `analsex` → `PenisToAnus`, `blowjob` / `deepthroating` / `penilelicking` → `PenisToMouth`, `vulvaleating` / `vulvallicking` → `MouthToVagina`-style oral tags (check AAF's tagging standard), `kissing` → `Kissing` + `MouthToMouth`, `hugging` → `Hugging`, `masturbation` / `malemasturbation` → `Masturbation` / `Jackoff`, `boobjob` → `TitJob`, `handjob` / `footjob` → `Handjob` / `Footjob`, `spanking` → `Spanking`, no sex action → `NonSex`; a scene with no actions → `Pose`.
- scene tags → AAF: an `aggressive` scene tag (used by OStim packs) → `Aggressive` + `Rough`, otherwise `Neutral`; `intro` / `idle` → `Foreplay` / `Pose`.
- a pack-authored `aafTags` list on a scene (a 4Stim addition, documented as such) for anything the table can't derive (`LegsTied`, `Missionary`, `FromFront`).
- `default_excludetags` expands to AAF's default (AAF's MCM value; check the source before fixing it).

The scene search then runs through `FourLibrary.FindRandomScene*` with the translated filters; a request that matches nothing returns a failing `OnSceneInit` (status ≠ 0). Ivy reacts to that by retrying with fewer tags, so the status codes need to follow AAF's.

## 5. Event arguments

AAF's events carry a `Var[]`; add-ons index it directly, so the bridge must fill AAF's layout exactly:

- **Animation events** (`OnAnimationStart` / `Change` / `Stop`, `OnSceneEnd`): [0] status (0 = OK), [1] actors, [2] position id, [3] tags (string array), [4] meta (the CSV the add-on passed: Violate, SEU and Spectators look for their own markers in it), [5] location FormID, [6] leaving actors, [7] stage, [8] joinable, [9] idle list, [10-13] action log. Add-ons read up to [9] (SEU).
- **OnSceneInit:** [0] status, [1] actors, [2] doppelganger (None: 4Stim has none), [3] location FormID, [4] coordinates, [5] meta, [6] duration, [7] isNPCControlled, [8] added actor, [9] position id, [10] tags.

**Actor order matters.** Family Planning treats actor 0 as the one "cummed in" and the rest as candidate fathers; AAF positions put the receiving partner first. OStim (and so 4Stim) puts the giver (usually the man) at role 0. The bridge must reorder: targets of penetrative actions first, then the actors, then the rest. The position id should be the 4Stim scene id.

**Status codes:** use AAF's for "actor busy", "no animation found", "actor invalid"; Ivy and Dirty Sex branch on them. Take the values from AAF's documented API, not guesses.

## 6. What's out of scope (and why that's fine)

- **AAF XML** (animationData, positionData, tagData, furnitureData) from animation packs and Torture Devices: those describe AAF animations, not 4Stim scenes. Packs have to be converted (AnimBridge) as today. `protectedEquipmentData.xml` is the exception: small, and worth reading (§3).
- **Walking to the spot** (usePackages / skipWalk), doppelgangers, the AAF HUD, MFG / morph / overlay / equipment sets: no installed add-on depends on them through the API.
- **OnStatEvent / stats:** no installed add-on reads AAF's stats.

## 7. Proposed build order

1. **Events only** (smallest useful step): generated AAF.esm (quest + keywords), `AAF:AAF_API` with every function stubbed, the listener sending the five used events for every 4Stim scene, `AAF_ActorBusy` on and off. Tests with what's enabled now: Commonwealth Captives' reward scene end, plus Family Planning / Sex Attributes / Dirty Sex if enabled.
2. **StartScene** with meta, duration, furniture and NPC control, tags ignored (random fitting scene). Commonwealth Captives starts its scenes this way.
3. **Tags** (§4): the mapping table, `aafTags`, failing `OnSceneInit` codes. Then Violate, SEU, Sexual Harassment+, Ivy, Morning Sex.
4. `GetProtectedEquipmentKeywords` + the XML reader; `AAF_GenderOverride_*`.

Open questions for the owner: whether to put the bridge in 4Stim's plugin (natives the Papyrus side calls) or keep it all Papyrus; the exact version number to report; whether to ask AAF's author.
