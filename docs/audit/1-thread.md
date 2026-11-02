# Parity audit 1: thread lifecycle (OStim NG vs 4Stim)

Scope: how a thread is built, validated, sorted, placed, locked, ticked, auto-driven, climaxed, ended, cleaned up, and what happens on save/load, death, combat, cell change and with several threads at once.

Sources read. OStim: `skse/src/Main.cpp`, `Core.cpp`, `Core/Core.cpp`, `Core/Thread.cpp`, `Core/Thread.h`, `Core/Thread/*` (AutoControl, Navigation, Furniture, Climax), `Core/ThreadActor.cpp`, `Core/ThreadActor/ThreadActorClimax.cpp`, `Core/ThreadManager.cpp`, `Core/ThreadStarter/*`, `Core/ThreadFlag.h`, `Serial/Manager.cpp`, `Serial/OldThread.h`, `Events/EventListener.cpp`, `GameLogic/GameHooks.h`, `GameAPI/GameActor.cpp` (lock/unlock/lockAtPosition), `GameAPI/GameCamera.cpp`, `GameAPI/GameEvents.cpp`, `Util/ActorUtil.cpp` (sort), `Papyrus/PapyrusThreadBuilder.h`, `Papyrus/PapyrusThread.h`, `MCM/MCMTable.h`. 4Stim: `src/main.cpp` (the sections cited), `src/SceneRegistry.cpp` (AssignRoles), `src/SceneEvents.cpp`, `Data/Scripts/Source/User/FourStimScene.psc`, `FourStimMenu.psc`, `Data/F4SE/Plugins/4Stim.ini`, `docs/API.md`, `ACTORS.md`, `AUTOMODE.md`, `SETTINGS.md`, `SCENES.md`, `ALIGNMENT.md`, `FURNITURE.md`, `TODO.md`.

Nothing here was run in game. Where a claim depends on engine behaviour I couldn't check from code, it says "unverified".

---

## 1. Scenes never end on death, combat, or (for NPC scenes) the player leaving the cell

- **Class**: Missing
- **Severity**: High
- **OStim**: `Thread::loop()` runs every 20 ms for every thread. It stops the thread when any actor `isInCombat()` or `isDead()`, and stops an NPC thread when actor 0 isn't in the player's cell (`Core/Thread.cpp:413-423`). `stop()` queues a normal close (`Thread.cpp:620-622`, `ThreadManager.cpp:50-53`), which frees and redresses everyone and sends the end event.
- **4Stim**: Nothing ends a running scene for these reasons. `GuardScenes` (`src/main.cpp:3369-3510`) only sheathes weapons drawn *outside* combat and leaves real combat alone. Its comment says "a scene can be broken by an attack", and `docs/ACTORS.md:135` repeats it. `AutoplayTick` (`main.cpp:3853-3940`) has no death, combat or cell check. Death and combat are only checked when a scene starts (`CantUse`, `main.cpp:7456-7474`; `FourStimScene.CanUseActor`). Grepped `src/` and the scripts for `IsDead`, `IsInCombat`, `TESDeathEvent`, `CombatEvent`, `CellAttach`, `OnDeath`, `OnCombatStateChanged`, `OnUnload`: only the start-time checks exist. The effects:
  - A partner killed mid-scene stays in the `ActiveScene`, and the survivor keeps animating against the corpse.
  - Actors in an attacked scene stay restrained and on the do-nothing scene package. The NPC scene timer (5 min) or the player ending it are the only ways out.
  - NPC scenes in a cell the player left keep their actors restrained and scene-packaged until the timer runs out.
- **Fallout 4 constraint?** None. `Actor::IsDead`, `IsInCombat` and `parentCell` are all available, and the scene clock already walks every scene four times a second.
- **Suggested fix**: In `AutoplayTick` (or `GuardScenes`), call `EndActiveScene(active, ...)` for a scene when any actor is dead or in combat, or when it has no player and actor 0's `parentCell` isn't the player's (OStim's `isInSameCell`). That should replace the "left to the game" policy, and ACTORS.md:135 needs updating to match.

## 2. Loading a save from inside a player scene leaves the player's controls, FOV, free-camera speed and HUD in scene state

- **Class**: Not considered
- **Severity**: High
- **OStim**: On a load, `Revert` untracks all threads (`Serial/Manager.cpp:290-295`). The old threads' actors are then freed through `OldThread::close` → `freeActor` → `unlock()`, which gives the player back AI-driven=false and the activate handler (`Serial/OldThread.h`, `GameAPI/GameActor.cpp:56-60`). (Caveat: at this OStim commit, `Main.cpp` wires `Core::sessionLoaded` (which calls `closeOldThreads`) to `kNewGame` and the empty `newSession` to `kPostLoadGame`. That looks swapped, so whether OStim really frees actors on a mid-session load is unverified. The intent is clear.)
- **4Stim**: The camera, input and FOV state is reset only in the `kGameLoaded` handler (`main.cpp:9416-9468`: `ReleaseSceneLayer`, `RestoreFOV`, `RestoreFreeCameraSpeed`, `EndSlowMotion`, `g_sceneCameraActive = false`, `g_startViewSaved = false`). F4SE sends `kGameLoaded` once per game launch, not per save load. `kPreLoadGame` (`main.cpp:9364-9405`) clears the scenes but not this state. `ReleaseSavedScene` (`main.cpp:9211-9250`) releases NPCs through `ReleaseAfterLoad`, which returns early for the player (`FourStimScene.psc:228-237`), and nothing calls `UnlockPlayerControls`. After an in-session load from a player scene:
  - The "4Stim" input layer stays allocated (`g_sceneLayer`, `main.cpp:5345-5376`), blocking activate, menu, fighting, VATS, favourites, sprint, jump and POV.
  - The world FOV stays at `SetFreeCamFOV`, and `fFreeCameraTranslationSpeed` stays at the scene value.
  - `g_sceneCameraActive` stays true, so the next scene's `BeginSceneCamera` returns early (`main.cpp:5543-5560`) and gives no free camera or hidden HUD.
  - The next scene's end restores all of this, so it heals after one more scene. Whether the HUD stays hidden across the load is unverified.
- **Fallout 4 constraint?** None.
- **Suggested fix**: In `kPreLoadGame` (or `kPostLoadGame`), run the same reset block as `kGameLoaded`: `ReleaseSceneLayer`, `RestoreFOV`, `RestoreFreeCameraSpeed`, `EndSlowMotion`, clear the camera flags, `SetHUDVisible(true)`. Better: move that block into one function and call it from both places.

## 3. Thread builder: one shared "pending" slot, so concurrent starts overwrite each other; failures after `Start` returns are silent

- **Class**: Different
- **Severity**: High
- **OStim**: `startThread` builds the `Thread` synchronously from its own `ThreadStartParams` (`ThreadManager.cpp:71-106`). NPC threads return -1 synchronously on any failure (`NPCThreadStarter.cpp`, `ThreadStarter.cpp:13-60`). Each thread owns its flags, metadata, duration and furniture, so any number of threads can start in one frame.
- **4Stim**: `StartThread` (`main.cpp:7531-7656`) returns the thread ID at once, then does the real work in a task.
  - It hands its data to the scene that starts later through single global optionals: `g_pendingStart`, `g_pendingFurniture`, `g_pendingSequence`, `g_furnitureSpots` and `g_pendingClutter` (`main.cpp:1404-1446`, `1123-1240`). `TrackSceneStart` picks them up by matching sorted actors (`main.cpp:1856-1913`).
  - A second `Start` (or a picker start, or a resumed save scene) before the first scene's Papyrus setup finishes overwrites the first's slot. The first scene then gets a new `g_nextThreadID++`, so the ID `Start` returned never exists, and it loses its metadata, `NoAutoMode`, `NoPlayerControl`, `NoUndressing`, `EndAfterSequence`, duration, furniture and sequence.
  - 4Stim already works around this for its own save resume, which spaces scenes 4 s apart "each takes the furniture slot" (`main.cpp:9329-9350`).
  - Failures inside the task (no scene fits, `AssignRoles` fails, a non-"npc"-type actor) only log a warning (`main.cpp:7600-7610`). The caller already has a positive ID.
  - `docs/API.md:40` says `Start` returns -1 when "the scene doesn't fit them", but only an actor-count mismatch on a given starting scene is caught synchronously (`main.cpp:7543-7549`).
- **Fallout 4 constraint?** Partly. Scene setup goes through Papyrus (placement, restrain), so it can't be fully synchronous. Nothing forces the single slot.
- **Suggested fix**: Key the pending data by thread ID (a map from thread ID to PendingStart+furniture+sequence), pass the thread ID through `StartPickedCast` → `Begin*Scene` → `PlaySceneIdles`/`PlayGroupIdles` → `TrackSceneStart`, and choose the scene and role order synchronously in `StartThread` so it can return -1 as OStim does. Correct API.md:40 either way.

## 4. With a starting scene, 4Stim reorders the caller's actors and can't fall back when the scene doesn't fit

- **Class**: Different
- **Severity**: High
- **OStim**:
  - With starting nodes given, actors are never sorted. The NPC starter sorts only `if (params.startingNodes.empty())` (`NPCThreadStarter.cpp:22-24`), and the player starter goes straight to `startInner` (`PlayerThreadStarter.cpp:76-80, 127-131`). The caller's array order is the role order.
  - If the actors don't fulfil any starting node, the nodes are cleared and a random start scene is picked instead (`ThreadStarter.cpp:41-52`).
  - `QuickStart` with an unknown scene ID just has no starting node, so it falls back to a random one too (`PapyrusThread.h:12-22`).
  - `setStartingNode` and `addStartingNode` warn and skip unknown IDs (`ThreadStartParams.cpp:7-12, 18-22`).
- **4Stim**:
  - `StartThread` always sorts: penis first, then dominant actors first. It then runs `SceneRegistry::AssignRoles`, which tries every permutation (`main.cpp:7560-7564, 7604-7615`; `SceneRegistry.cpp:1095-1118`), even when the caller named a scene. A mod passing `[female, male]` for a scene whose role 0 needs no penis gets the male in role 0.
  - An unknown first scene returns -1 (`main.cpp:7543-7549`), so `T_QuickStart` with a bad ID fails instead of falling back. A known scene the actors don't fit fails silently in the task.
  - Unknown IDs further down a starting list are kept and fail mid-sequence (`PlayOnActiveScene` warns and drops the sequence, `main.cpp:2196-2199`).
  - `docs/API.md:46` documents the reordering only for "Without a starting scene".
- **Fallout 4 constraint?** None.
- **Suggested fix**: When `builder.starting` is non-empty, use the caller's order unchanged, check `Fits` on every entry, and on failure clear `starting` and pick a start scene as OStim does. Make `TB_SetStartingAnimation`/`TB_AddStartingAnimation` ignore unknown IDs with a warning, and make `T_QuickStart` ignore an unknown scene.

## 5. `AddStartingAnimation` without a duration skips the scene at once; `navigateTo` is ignored

- **Class**: Different
- **Severity**: High (any OStim-style multi-animation start breaks)
- **OStim**: `addStartingNode(id, duration, navigateTo)` uses `node->animationLengthMs` when duration ≤ 0. With `navigateTo`, it inserts the navigation route from the previous node (`ThreadStartParams.cpp:17-41`). `concatStartingSequence` routes the same way (`:57-76`). A single starting node is a plain `ChangeNode`, not a sequence (`Thread.cpp:154-158`).
- **4Stim**:
  - `TB_AddStartingAnimation` stores the duration as given (0 by default) and drops the `navigateTo` argument (unnamed `bool`, `main.cpp:7682-7685`). `TB_ConcatStartingSequence` drops it too (`main.cpp:7695-7705`).
  - The builder turns several entries into a `Sequence` whose 0-second entries `ArmAutoplay` arms with `remaining = 0` (`main.cpp:2109-2125`). `AutoplayTick` then advances on the next 10 ms tick, so every entry without a duration flashes by. File sequences don't have this problem: they default to the scene's `length` (`SceneRegistry.cpp:797-811`).
  - A single starting animation with a duration becomes a one-entry sequence (`main.cpp:7626-7632`). It fires `FourStim_OnSequenceEnd`, and with `EndAfterSequence` it ends the scene, where OStim keeps playing.
  - Neither difference is in API.md (`API.md:44` lists `navigateTo` as if supported) or in TODO.md.
- **Fallout 4 constraint?** None.
- **Suggested fix**: In `StartThread`, replace duration ≤ 0 with the scene's `length` (as file sequences do). Implement `navigateTo` with the same route search auto mode uses (`Reachable`). Keep a single starting animation as a plain start.

## 6. `NoAutoMode` is only honoured at start; `NoPlayerControl` turns auto mode on and blocks speed

- **Class**: Different
- **Severity**: Medium
- **OStim**:
  - `startAutoMode` refuses whenever the thread has `NO_AUTO_MODE` (`ThreadAutoControl.cpp:124-126`). That covers the auto mode key (`EventListener.cpp:164-172`) and the API.
  - `NO_PLAYER_CONTROL` only empties the scene menu's navigation options (`UI/Scene/SceneMenu.cpp:130-132`) and is read by the mod API. It doesn't start auto mode, and the speed keys still work (`EventListener.cpp:150-157`).
  - `OPlayerThread.SetPlayerControl` toggles the flag at runtime (`ScriptAPI/PlayerThreadScript.cpp`).
- **4Stim**:
  - `TrackSceneStart` starts auto mode when `noPlayerControl || (!noAutoMode && ...)` (`main.cpp:1985-1987`), so `NoPlayerControl` + `NoAutoMode` still gets auto mode.
  - `StartAutoMode` never checks `noAutoMode` (`main.cpp:2496-2524`), and neither do `T_StartAutoMode` (`main.cpp:7882-7889`), `FourStim.SetAutoMode` (`main.cpp:4781-4792`) or the auto mode key (`main.cpp:7102-7110`, which checks only `noPlayerControl`).
  - `NoPlayerControl` also blocks the speed keys (`ChangeSceneSpeedImpl`, `main.cpp:5682-5695`).
  - `API.md:44` describes the 4Stim behaviour ("auto mode drives the scene") without marking it as a departure. There is no `SetPlayerControl` counterpart (grepped `SetPlayerControl`: none).
- **Fallout 4 constraint?** None.
- **Suggested fix**: Return early in `StartAutoMode` when `noAutoMode`. Drop the `noPlayerControl ||` start. Let `noPlayerControl` block only navigation, as OStim does, or document the departure.

## 7. Auto speed control only runs in auto mode (OStim: every NPC thread, and player threads with `SetActorSpeedControl`)

- **Class**: Different
- **Severity**: Medium (with default settings, OStim's NPCs speed up the player's manual scenes; 4Stim's don't)
- **OStim**: `loopAutoControl` runs the speed-up block *before* the `if (!autoMode) return;` check, gated only by `!playerThread || autoSpeedControl()` (`ThreadAutoControl.cpp:185-213`). So speed rises with excitement in every NPC thread, `NoAutoMode` ones included, and in the player's manual scenes while `SetActorSpeedControl=1` (OStim's default, `MCMTable.h` entry 0xE1B).
- **4Stim**: `AutoModeTick` returns at once unless `autoMode.on` (`main.cpp:2534-2541`), and the speed block comes after that. The INI comment and docs describe it as part of auto mode (`4Stim.ini` `[AutoMode]` "Auto mode speeds the scene up"; `AUTOMODE.md:14`; `SETTINGS.md` row "Auto mode speeds up").
- **Fallout 4 constraint?** None.
- **Suggested fix**: Move the speed block before the `on` check, gated `(!withPlayer || g_autoConfig.autoSpeed)`, still skipping transitions and sequences. Update the docs.

## 8. OStim's other auto-mode-start settings are missing: `SetForceAIForMasturbation` (solo) and `SetForceAIInConsensualScenes`

- **Class**: Missing
- **Severity**: Medium
- **OStim**: `evaluateAutoMode` (`ThreadAutoControl.cpp:95-117`) puts player threads in auto mode by:
  - `useAutoModeAlways` (`SetAIControl`)
  - for solo scenes, `useAutoModeSolo` (`SetForceAIForMasturbation`)
  - otherwise `useAutoModeVanilla` (`SetForceAIInConsensualScenes`)
  
  (`MCMTable.h:51-55`, names at the 0xE1C-0xE20 entries.)
- **4Stim**: Only `SetAIControl` is read (`main.cpp:372`), as `g_autoConfig.player` (`main.cpp:1985`). The other two aren't read and aren't listed in `SETTINGS.md` "Not in yet".
- **Fallout 4 constraint?** None.
- **Suggested fix**: Add both with OStim's names and defaults (0). Solo player scenes start in auto mode when the first is set; scenes with two or more actors start in auto mode when the second is set.

## 9. `defaultSpeed` and speed carry-over work the other way round from OStim

- **Class**: Different
- **Severity**: Medium
- **OStim**: `ChangeNode` sets speed 0 for one-speed nodes. While no relative speed is known yet (the thread's first node), it uses the node's `defaultSpeed`. After that it maps the relative speed: `relativeSpeed * (speeds-1) + 0.5` (`Thread.cpp:317-323`, relativeSpeed set in `SetSpeed`, `:486-492`). So `defaultSpeed` applies to the first scene of every thread, and auto mode moves keep the relative speed.
- **4Stim**:
  - Every start plays speed index 0: `PlaySceneIdles` uses `speeds[0]` (`main.cpp:4446-4447`), and the group and solo paths do the same.
  - Only auto mode uses `defaultSpeed` (`AutoGoToNow`, `main.cpp:2366`).
  - Manual navigation and `FourThread.NavigateTo` keep the absolute index, clamped (`main.cpp:5739`, `7797`).
  - `SCENES.md:97` and `AUTOMODE.md:14,27` describe this as if it were OStim's rule.
- **Fallout 4 constraint?** None.
- **Suggested fix**: Start at `defaultSpeed` and carry relative speed on node changes as OStim does. Also keep the transition carry speed in relative terms.

## 10. The scene-end event fires before the actors are released, and carries less than OStim's

- **Class**: Different
- **Severity**: Medium
- **OStim**: `Thread::close` frees every actor completely first: unlock, `updateAI`, redress, weapons, scale, reset position, face. Then it restores the furniture, then calls the end listeners, then sends `ostim_thread_end` (`Thread.cpp:672-718`). The event's JSON has `scene`, `actors`, `metadata` and `originator`, and is also sent for threads closed at load (`GameEvents.cpp:39-61`, `OldThread.h` `close()`).
- **4Stim**:
  - `TrackSceneStop` sends `FourStim_OnSceneEnd` (`main.cpp:2013-2051`) from the native task inside `StopPair`/`StopGroup`. Papyrus only then waits 0.5 s before `ReleaseFromScene` (unrestrain, `EvaluatePackage`, foot IK, head tracking, re-arm), then 1.0 s more before `RestoreActorInteraction` (`FourStimScene.psc:157-170, 460-476`, and `EndGroupScene`).
  - A listener that chains a new scene on the end event (common for OStim add-ons) passes `CanUseActor`, restrains the actors, and then has the old scene's delayed `ReleaseFromScene` un-restrain them, re-equip their weapon and re-enable activation in the middle of the new scene.
  - The event has no thread ID, metadata or originator (`SceneEvents.cpp:132-140`). Metadata is the usual way OStim mods recognise their own threads.
  - No end event is sent for scenes released at load (`ReleaseSavedScene`, `main.cpp:9211-9250`).
- **Fallout 4 constraint?** The waits exist for FO4 reasons (collision and pose clearing). Sending the event after them isn't constrained.
- **Suggested fix**: Send `SceneEnded` from Papyrus after `RestoreActorInteraction`, or have `ReleaseFromScene` check that the actor isn't in a new scene before un-restraining. Add the thread ID and metadata to the end event, and send it for scenes released after a load.

## 11. The player is never put back after a furniture scene

- **Class**: Different
- **Severity**: Medium
- **OStim**: `ThreadActor::free` resets every actor, the player included, to `positionBefore` when `SetResetPosition` is on (`ThreadActor.cpp:378-386`, no `isPlayer` check).
- **4Stim**: `MoveApart` skips the player (`main.cpp:2990-3050`), and the player's start position is never recorded: `g_positionBefore` is written only in `SuppressInteraction` (`main.cpp:4155-4161`), which `PrepareActor` calls for NPCs only (`FourStimScene.psc`). The stated reason, "the scene was built on the player's own spot" (`ALIGNMENT.md:13`, `main.cpp:2981-2983`), is false for furniture scenes: `PlaceOnPendingFurniture` moves every actor, the player too, onto the bed, table or wall spot (`main.cpp:1272-1274`), and `MoveSceneToFurniture` moves the player as well. The player is left standing on or inside the furniture.
- **Fallout 4 constraint?** None. The native `SetPosition` placement already used is safe for the player.
- **Suggested fix**: Record the player's position before placement and reset it at the end under `SetResetPosition`, at least when the scene was on furniture.

## 12. Player scenes started by a script skip OStim's start flow (dialogue wait, add actors, role choice, `PlayerAlways`, furniture choice)

- **Class**: Partial
- **Severity**: Medium
- **OStim**: `startPlayerThread` runs the same steps for builder or `QuickStart` player threads as for the hotkey (`PlayerThreadStarter.cpp`):
  1. Waits until no actor is in dialogue (`:17-30`).
  2. Picks furniture with the `SetSelectFurniture` menu, or asks "use bed?" when there is a starting node (`:41-68`).
  3. Offers to add nearby actors (`addActorsAtStart`, default 1, `:76-121`).
  4. Asks the player's role (`playerSelectRole*`, `:127-151`).
  5. Sorts with the `PlayerAlways*` settings (`ActorUtil.cpp:46-70`).
- **4Stim**: `StartThread` does none of these. It picks the nearest furniture with a fitting scene silently, and sorts by penis and dominance only, with no `PlayerAlwaysIndex` and no role prompt (`main.cpp:7560-7616`). The picker does have its own actors, role and place steps (`main.cpp:5850-6015`). Nothing waits for the dialogue menu (grepped `DialogueMenu`, `InDialogue`: none). TODO.md lists only "the 'use the bed?' question for scenes started by scripts". `addActorsAtStart` isn't read (grepped: none).
- **Fallout 4 constraint?** None. The picker UI could host the prompts.
- **Suggested fix**: For builder threads with the player, wait for `DialogueMenu` to close, apply `PlayerAlwaysIndex`, and route to the picker's role and place steps when the settings ask. Add the missing items to TODO.md.

## 13. Builder eligibility is weaker than OStim's `isEligible`

- **Class**: Partial
- **Severity**: Medium
- **OStim**: `isEligible` requires the actor to be not disabled, not deleted, not a child, not dead, to have a non-empty actor type, and to be in no thread (`Core/Core.cpp:33-35`). `startThread` also requires `isLoaded()` and rejects duplicates (`ThreadStarter.cpp:22-37`). `OThreadBuilder.Create` returns -1 at once for an ineligible actor (`PapyrusThreadBuilder.h:12-21`).
- **4Stim**: `CantUse` (`main.cpp:7456-7474`) checks child, dead, combat and in-scene only. The builder never checks disabled, deleted, loaded (3D), actor type or duplicates. An unloaded NPC can be pulled into place by native `SetPosition` (`PlaceActorAt`, `main.cpp:4035-4053`). A duplicate actor in a group isn't rejected (`BeginGroupScene` has no duplicate check; `BeginPairScene` has one). The picker does check type and 3D (`FindCandidates`, `main.cpp:6083-6097`). `BuilderCreate` checks nothing (`main.cpp:7496-7513`).
- **Fallout 4 constraint?** None.
- **Suggested fix**: Extend `CantUse` with disabled, deleted, `Get3D()`, `ActorProperties` type and duplicates, and call it in `BuilderCreate` too.

## 14. A failed start leaves actors half-locked

- **Class**: Not considered
- **Severity**: Medium
- **OStim**: Nothing is applied to actors until the `Thread` is constructed (`ThreadManager::startThreadNoLock`), so a failed start leaves no trace.
- **4Stim**: By the time `PlaySceneIdles` runs, the actors have already been through `SuppressInteraction`, `HolsterForScene`, `IgnorePairCollision`, `PlacePair` and `LockForScene`. The player has also had `LockPlayerControls` and `PrepareSceneCamera`. If both `PlayIdle` calls are refused, `PlaySceneIdles` returns without `TrackSceneStart` (`main.cpp:4466-4469`), and Papyrus still calls `BeginSceneCamera` (`FourStimScene.psc:354`). That leaves:
  - NPCs restrained, with activation and dialogue blocked
  - collision changed
  - the player's input layer on, the HUD hidden and the free camera on
  - no running scene the end key can end (the hotkey sees no player scene)
  
  `PruneScenePackage` removes only the package, and only while the ticker runs.
- **Fallout 4 constraint?** Partly. The Papyrus setup order is a FO4 necessity, but undoing it on failure isn't constrained.
- **Suggested fix**: On failure, run the release path: `RestoreCollision`, `ReleaseFromScene`, `RestoreActorInteraction` and `EndSceneCamera`, through a Papyrus callback or by returning a failure to `Begin*Scene`.

## 15. Climax end conditions in NPC scenes, and the auto mode stage after a climax

- **Class**: Different
- **Severity**: Low
- **OStim**:
  - `climaxInner` applies `SetEndOnBothOrgasm`, or the player, male and female conditions, to every thread, *and additionally* `endNPCSceneOnOrgasm` for NPC threads (`ThreadActorClimax.cpp:113-134`).
  - It then calls `setAutoModeToMainStage()`, so any climax moves foreplay or pull-out to the main stage (`:136`, `ThreadAutoControl.cpp:246-252`).
  - The climax stop timer is re-armed to 4 s by each qualifying climax (`setStopTimer`).
- **4Stim**:
  - NPC scenes use only `endNPCScenes` (`main.cpp:3795-3804`). With `endNPCSceneOnOrgasm=0` and `SetEndOnOrgasm=1`, OStim's NPC scene still ends on a man's climax; 4Stim's doesn't.
  - Only the pull-out stage returns to main (`main.cpp:3762-3765`). A climax during foreplay leaves foreplay running, and excitement has just dropped, so the foreplay threshold is far away again.
  - The end timer is armed once (`endIn < 0`).
- **Fallout 4 constraint?** None.
- **Suggested fix**: Change the condition to `(withPlayer-style conditions) || (!withPlayer && endNPCScenes)`, and set the stage to main from foreplay too.

## 16. Auto mode picks: the standing/bed rule is a preference, and pull-out needs an intercourse scene; the docs say "as OStim"

- **Class**: Different
- **Severity**: Low
- **OStim**: `addFurniture` makes "someone standing" (off furniture) and "no one standing" (bed) hard conditions on the foreplay and sex picks. When nothing passes, auto mode doesn't move (`ThreadAutoControl.cpp:38-53, 64-92`). The pull-out triggers on a man's excitement in the main stage, whatever the scene (`:227-239`).
- **4Stim**: The rule is dropped when no candidate passes (`main.cpp:2448-2460`), and pull-out also requires `IsIntercourse(*scene)` (`main.cpp:2585`). AUTOMODE.md:11-13 and `4Stim.ini` present both as OStim's behaviour.
- **Fallout 4 constraint?** None. (The soft rule may be a sensible choice for small packs.)
- **Suggested fix**: Either match OStim or mark both as departures in AUTOMODE.md.

## 17. `EndAfterSequence`, and navigating during a sequence

- **Class**: Different
- **Severity**: Low
- **OStim**: `clearNodeQueue` (called by `warpTo`, `navigateTo`, `playSequence`) stops the thread when a sequence with `endAfterSequence` is interrupted (`ThreadNavigation.cpp:329-346`). The scene menu offers no navigation while `isInSequence()` or in a transition (`SceneMenu.cpp:130-132`).
- **4Stim**: Navigating away (player, `FourThread.NavigateTo`/`WarpTo`) drops the sequence and the scene goes on (`main.cpp:2088-2095`, `7790-7792`). No `SequenceEnded` event is sent and `endAfterSequence` doesn't end it. The player can navigate during a sequence: the HUD only special-cases transitions (`HUD.cpp:650`).
- **Fallout 4 constraint?** None.
- **Suggested fix**: When a sequence with `endAfterSequence` is interrupted, end the scene, and hide navigation during sequences (or document the departure).

## 18. OStim's API factions (`OStimActorCountFaction`, excitement, times climaxed, time until climax, schlongified, in water) aren't provided, and the docs don't say so

- **Class**: Missing (undocumented)
- **Severity**: Low
- **OStim**: Actors are added to the factions in the `ThreadActor` constructor and `Thread` constructor and removed in `freeActor` (`ThreadActor.cpp:44-50`, `Thread.cpp:74-80`, `Core/Core.cpp:20-25`; `API factions README.txt`). Dialogue and condition-based content keys off them. Furniture-type factions are added and removed as well (`ThreadFurniture.cpp:83-96`).
- **4Stim**: None. `ACTIONS.md:65` mentions skipping *action* factions, and FURNITURE.md:77 mentions skipping ownership. The thread-level API factions aren't mentioned anywhere (grepped docs for `ActorCount`, `faction`).
- **Fallout 4 constraint?** Yes: 4Stim has no plugin to own the factions. The closest equivalents are factions in an optional ESL, or actor values or keywords added by script.
- **Suggested fix**: Document the gap in API.md, or add the factions in the optional add-on ESL.

## 19. Actors released after a load: no idle stop, foot IK or head tracking reset

- **Class**: Partial
- **Severity**: Low (unverified)
- **OStim**: `freeActor(byGameLoad)` → `unlock()`, which stops the translation, resets `bHumanoidFootIKDisable` and the head-tracking variables, and sends `IdleForceDefaultState` (`GameActor.cpp:56-89`).
- **4Stim**: `ReleaseAfterLoad` only stops the translation, un-restrains, calls `EvaluatePackage` and restores interaction (`FourStimScene.psc:228-237`). No `IdleStop`, no `SetAnimationVariableBool("bHumanoidFootIKDisable", false)`, no `SetHeadTracking(true)`, and nothing at all for the player. Whether FO4 saves graph variables, head tracking and the current idle for loaded actors is unverified.
- **Fallout 4 constraint?** No.
- **Suggested fix**: Add the `ReleaseFromScene` resets and an `IdleStop` to `ReleaseAfterLoad`, for the player as well.

## 20. Smaller differences

- **Class**: Different
- **Severity**: Low
- Camera after the scene: OStim returns to third person unless `SetForceFirstPerson` (`GameCamera.cpp:48-66`). 4Stim returns to the view the scene started in (`main.cpp:5575-5580`). This isn't marked as a departure in SETTINGS.md.
- The picker's "Gay vs Straight" for `playerSelectRole*` compares body sex (`PlayerChoosesRole`, `main.cpp:5998-6013`). OStim compares `hasSchlong` (`PlayerThreadStarter.cpp:132-138`), and 4Stim's own `PlayerAlwaysIndex` does too (`main.cpp:5911-5925`).
- `SetRemoveWeaponsAtStart` / `SetRemoveWeaponsWithSlot` (OStim 0xDAB/0xDAE, `ThreadActor.cpp:57-59`, `Thread.cpp:270-276`) aren't settings in 4Stim: weapons always come off at the start (`FourStimScene.HolsterForScene`). The reason is in code comments only, not in SETTINGS.md.
- Thread IDs: OStim reuses freed NPC IDs (`idGenerator.free`, `ThreadManager.cpp:213-215`). 4Stim counts up forever. This is harmless.
- `bResumeScenes=1` (4Stim's own feature) doesn't save thread ID, metadata, flags, sequence, stop timer or climax end timer (`SavedScene`, `main.cpp:8942-8959`), so a resumed scene loses them.

---

## Checked and matching

- Player thread is ID 0, and only one player thread at a time. NPC threads get other IDs (`main.cpp:1866`; OStim `ThreadManager.cpp:76-91`).
- `NPCSceneDuration` applies to NPC scenes, and `SetDuration` to any scene including the player's (`main.cpp:1971-1979`; OStim `Thread.cpp:106-110`). Both end through a faded stop for the player.
- `FourThread.Stop` fades for the player's scene as OStim's `Stop` → `stopFaded` (`PapyrusThread.h:29-34`; `FourStimMenu.EndPlayerScene`).
- Fade timings: 1 s to black, the start or end 0.7 s in, back after 0.55 s (`FourStimMenu.psc:81-100`; OStim `PlayerThreadStarter.cpp:186-198`, `Thread.cpp:624-645`).
- Sort order in the builder: dominant actors first, then penis first, stable (`main.cpp:7560-7564` vs `ActorUtil.cpp:10-23`). `PlayerAlways*` in the picker for two actors (`main.cpp:5911-5925`).
- Start scene choice: `intro`/`idle` for scenes with the player, any scene for NPC scenes, with the standing/bed rule. The fallback chain is documented (`main.cpp:1072-1118`).
- NPC AI held by the scene package, through the alias-package hook (OStim `GameHooks.h:44-71`; 4Stim ScenePackage, ACTORS.md:133-135). `EvaluatePackage` at start and end.
- Lock: foot IK off, head tracking off, keyframed `TranslateTo` at huge speed with +1° at 0.0001 (`FourStimScene.psc:139-200` vs `GameActor.cpp:30-54, 160-169`). Weapons sheathed for the player.
- Alignment applied on every node change (`LockScene` from `TrackSceneChange`/`PlayOnActiveScene`) vs `alignActors` in `ChangeNode`.
- NPCs reset to their start position at the end under `SetResetPosition` (except the player, finding 11).
- Furniture blocked during the scene and unblocked after. Clutter reset (documented as working where OStim's is off). A furniture change picks `intro`/`idle` with the bed rule and recentres the free camera, and sends a furniture-changed event (`main.cpp:2813-2911` vs `ThreadFurniture.cpp:23-79`).
- Auto mode stages, foreplay chance and thresholds, pull-out chance and thresholds, solo masturbation picks, waiting during sequences and transitions, the cooldown from `autoModeAnimDuration*`, and `autoModeLimitToNavigationDistance` (`main.cpp:2402-2595` vs `ThreadAutoControl.cpp`), apart from findings 7, 8, 15 and 16.
- Climax: a penis owner's climax drops the scene to speed 0. An awaiting climax resolves on the annotation or on a scene change. The end delay is 4 s (`fClimaxEndDelay=4.0`). Slow motion is 0.3x for 2.5 s in player scenes only.
- Camera: free camera speed and world FOV are set at start and restored at end (`main.cpp:5407-5463`, `5543-5590` vs `Thread.cpp:87-104, 647-663`). The custom time scale is missing, as documented.
- Default `bResumeScenes=0`: actors of scenes in a save are released on load (equip objects taken back, redressed, scale restored, weapon back, furniture unblocked, clutter restored). This matches what OStim's `closeOldThreads` intends (see the caveat in finding 2 about OStim's own wiring).
- Game pause: both clocks stop in menus (OStim `isPaused`; 4Stim `menuMode`, `main.cpp:3864-3871`).
