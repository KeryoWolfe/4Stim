# 6. Scripting API, mod events and plugin interface: OStim NG vs 4Stim

Scope: OStim's 29 Papyrus scripts (`data/Scripts/Source`), `skse/src/Papyrus`, `ScriptAPI`, `ModAPI`, `PluginInterface*`, `GameAPI/GameEvents.cpp`, the four API README/list files; against 4Stim's `Data/Scripts/Source/User/Four*.psc`, `src/main.cpp` (natives, thread builder), `src/SceneEvents.cpp`, `include/4Stim/PluginInterface.h`, `docs/API.md`, `docs/EVENTS.md`.

Paths: `O:` = `/tmp/ostim/OStimNG-main/`, `4:` = `/home/claude/4stim/`.

Verdict on "port by renaming": a mod that only calls `OThread` / `OThreadBuilder` with an explicit starting scene, one thread at a time, and reads nothing at scene end, mostly ports by renaming `O` to `Four`. A typical addon does not. It registers mod events (nothing to rename them to), reads `OJSON` / metadata at thread end (gone in 4Stim), uses `OActorUtil.ToArray` / `Sort` (missing), conditions dialogue on `OStimExcitementFaction` (missing), or calls one of the 260 missing `OMetadata` functions. Several renamed calls also fail to compile (`PlayExpression`, `IsClimaxStalled`), or compile and then behave differently (thread state changes are asynchronous, `Climax(…, true)`, `PermitClimax`).

---

## F1. A second scene start before the first is placed overwrites the first one's settings and thread ID
- **Class**: Different (undocumented)
- **Severity**: High
- **OStim**: Each start is independent. NPC threads go straight into `ThreadManager` (`O:skse/src/Core/ThreadStarter/NPCThreadStarter.cpp:11-47`). The player thread's params are captured in its own lambda (`PlayerThreadStarter.cpp:16-29`).
- **4Stim**: `StartThread` puts the builder's options (thread ID, metadata, duration, NoAutoMode, NoPlayerControl, NoUndressing, EndAfterSequence) into one global slot, `std::optional<PendingStart> g_pendingStart` (`4:src/main.cpp:1439`, written at `7645`). The furniture goes into `g_pendingFurniture` (`1213`, `7623`), also one slot. Sequences use `g_pendingSequence` the same way. The scene takes these over only when it actually starts, a moment later, if the actor set matches (`1858-1866`).
  - If two `FourThreadBuilder.Start` / `FourThread.QuickStart` calls run before the first scene is placed (a script starting several NPC scenes in a loop), the second overwrites the first. The first scene then gets a new `g_nextThreadID++` instead of the ID `Start` returned (`1868`), with no metadata, no options and no furniture.
  - If a start fails after `Start` returned (actors don't fit the roles at `7603-7606`, or `FourStimMenu.StartPickedCast` gives up at `FourStimMenu.psc:153-155`), its pending entry stays behind. The next scene with the same actors inherits it.
- **Fallout 4 constraint?**: No. The asynchronous Papyrus placement is 4Stim's design, not something Fallout 4 forces.
- **Suggested fix**: Key the pending starts by thread ID (a map, or carry the ID through `StartPickedCast` into `TrackSceneStart`). Drop an entry when its start fails.

## F2. Thread-ID calls made right after `Start` / `QuickStart` are silently dropped
- **Class**: Different (partly documented: API.md:40 says the scene "starts a moment later"; what that does to other calls isn't said)
- **Severity**: High
- **OStim**: `startNPCThread` returns `ThreadManager::startThread(params)` with the thread already in the map (`NPCThreadStarter.cpp:45`). `IsRunning`, `AddMetadata`, `SetMetaFloat`, `StallClimax`, `GetActors`, `NavigateTo`, `SetSpeed` all work on the next line. Only the player thread is deferred, while it waits for dialogue to end (`PlayerThreadStarter.cpp:17-27`).
- **4Stim**: Every thread is deferred (`main.cpp:7572-7714`, then a Papyrus round trip). Until the scene is placed, `FindThread(id)` returns null, so every `FourThread` setter (wrapped in `WithThread`, `7402-7410`) does nothing, without a warning. The getters return empty or -1. A common OStim pattern, `id = OThreadBuilder.Start(b)` followed by `OThread.AddMetadata(id, "mymod")`, loses the metadata in 4Stim.
- **Fallout 4 constraint?**: Partly. 4Stim places actors in Papyrus. A thread record could still be created at `Start` (status "starting") and setters queued on it.
- **Suggested fix**: Create the `ActiveScene` / thread entry (or a pending-thread record that setters write into) at `Start`. At minimum, document that setters do nothing until `IsRunning` is true.

## F3. Thread setters are asynchronous: reading back right after a write returns the old value
- **Class**: Different (undocumented)
- **Severity**: High
- **OStim**: `ScriptAPI::Thread::addMetadata` / `setMetaFloat`… run inside the native call, so the value is there immediately (`O:skse/src/Papyrus/PapyrusThread.h:238-273`).
- **4Stim**: These go through `WithThread` → `F4SE::GetTaskInterface()->AddTask` (`main.cpp:7402-7410`): `T_AddMetadata` (`7905`), `T_SetMetaFloat` (`7929`), `T_SetMetaString` (`7948`), `T_StallClimax` / `T_PermitClimax` (`7846-7861`), `T_SetSpeed`, `T_StartAutoMode`, `T_Stop`. So `FourThread.AddMetadata(t, "x")` followed by `FourThread.HasMetadata(t, "x")` returns false. Per-actor metadata (`AM_*`, `8149-8220`) is written immediately, so the two halves of the API behave differently.
- **Fallout 4 constraint?**: No. These writes only touch 4Stim's own data under a lock, as `AM_*` already does.
- **Suggested fix**: Do metadata, stall and auto-mode flag writes immediately under a lock. Defer only work that has to touch the game (playing idles).

## F4. Nothing about a thread is readable when it ends
- **Class**: Missing
- **Severity**: High
- **OStim**: `ostim_thread_end` / `ostim_end` carry a JSON with `scene`, `actors`, `metadata` and `originator` (why the thread ended), plus the thread ID in NumArg (`O:skse/src/GameAPI/GameEvents.cpp:39-62`). `OJSON.GetActors` / `GetScene` / `GetMetadata` read it (`OJSON.psc`). This is how addons recognise their own scenes when they end.
- **4Stim**: `FourStim_OnSceneEnd(Actor[], String sceneID)` only (`4:src/SceneEvents.cpp:132-140`). When it fires:
  - the scene has already been erased from `g_activeScenes` (`main.cpp:2022-2023`), so `FourThread.GetMetadata(id)` returns nothing and `FourActor.GetThreadID` returns -1;
  - per-actor metadata is already cleared (`ForgetActorMeta`, `2029`), before `SceneEnded` at `2049`;
  - no thread ID, metadata or end reason is passed (`EndActiveScene`'s reason string isn't forwarded).

  There is no `OJSON`.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Pass the thread ID, the thread metadata and an originator to the end event, or fire the end event before the scene is erased and its metadata cleared. Add a `FourJSON` (or plain arguments) if the OStim JSON form is kept.

## F5. Mod events: other names, no thread IDs, a different registration model, and the documented reason doesn't hold
- **Class**: Documented departure (reason wrong) + Missing events
- **Severity**: High
- **OStim**: SKSE mod events, any script registers with `RegisterForModEvent` (also ReferenceAlias and ActiveMagicEffect scripts), and the registration is saved with the game. Events (`O:skse/src/GameAPI/GameEvents.cpp:12-94`, `OSKSE.psc:96-104`, `list of mod events.txt`):

| OStim event (args) | 4Stim |
|---|---|
| `ostim_prestart` (player thread) | missing |
| `ostim_start` (player thread) | `FourStim_OnSceneStart` (every thread; no player-only form) |
| `ostim_thread_start` (NumArg = threadID) | `FourStim_OnSceneStart(Actor[], sceneID)`: no thread ID |
| `ostim_scenechanged` (StrArg = scene) and `ostim_scenechanged_<sceneID>` | `FourStim_OnSceneChange(Actor[], old, new)`; no per-scene event |
| `ostim_thread_scenechanged` (scene, threadID) | same, no thread ID |
| `ostim_animationchanged` (scene, speed; player thread) | missing (`OnSpeedChange` comes closest) |
| `ostim_thread_speedchanged` (speed as string, threadID) | `FourStim_OnSpeedChange(Actor[], scene, level, count)`; fired less often, see F6 |
| `ostim_orgasm` (scene, actor index, Sender = actor; player thread) / `ostim_actor_orgasm` (scene, threadID, Sender) | `FourStim_OnClimax(actor, Actor[], scene, times)` |
| `ostim_furniturechanged` (type, threadID, Sender = furniture) | `FourStim_OnFurnitureChange` |
| `ostim_event` (threadID, type, actor, target, performer) | `FourStim_OnEvent(...)` without the thread ID |
| `ostim_end` (json; player thread), `ostim_totalend`, `ostim_thread_end` (json, threadID) | `FourStim_OnSceneEnd`; no JSON, see F4 |
| `ostim_spank` (legacy; Sender = target) | missing |
| `ostim_subthread_start/end/orgasm`, `ostim_thirdactor_join/leave` (legacy `OStimSubthread`) | missing |
| (none) | 4Stim's own: `FourStim_OnNPCSceneStart/End`, `FourStim_OnSequenceEnd` |

- **4Stim**: `FourStim.RegisterForSceneEvents(Form)` takes only a Quest or ObjectReference and calls named functions through `SendEvent` (`SceneEvents.cpp:37-75`). Registrations are not saved; scripts must register again on every load (`main.cpp:9365-9368`). API.md:89 gives the reason: "Fallout 4's Papyrus has no mod events".
- **Fallout 4 constraint?**: The reason is weak. F4SE has the equivalent of mod events: `ScriptObject.RegisterForExternalEvent(string eventName, string callback)` / `UnregisterForExternalEvent` (`/tmp/f4se_src/scripts/modified/ScriptObject.psc:22-24`). Any script can use it (aliases and effects too), the registrations are saved in the F4SE co-save (`/tmp/f4se_src/f4se/InternalSerialization.cpp:233`, record `EXEV`), and a native plugin reads them with `F4SE::PapyrusInterface::GetExternalEventRegistrations` (`4:lib/commonlibf4/include/F4SE/Interfaces.h:375`). That allows OStim's own event names (`4stim_thread_start` …) with OStim's argument lists (string, float, Form; and the 5-argument `ostim_event`).
- **Suggested fix**: Send OStim's events under the naming rule (`ostim_*` becomes `4stim_*`) through external events, with OStim's arguments (thread ID in NumArg, actor as Sender). Keep `RegisterForSceneEvents` as a convenience. Fix API.md:89 (see F17).

## F6. Speed and scene-change events fire at other moments than OStim's, or not at all
- **Class**: Different (undocumented)
- **Severity**: Medium
- **OStim**:
  - The start event fires before the first scene; `ChangeNode` follows and fires `scenechanged` for the starting scene (`O:skse/src/Core/Thread.cpp:152-157`).
  - Every `ChangeNode` calls `SetSpeed`, which fires `speedchanged` and the speed listeners, then fires `scenechanged` (`Thread.cpp:316-331`, `521-523`). So each scene change produces a speed event followed by a scene event, and every `OThread.SetSpeed` produces one too.
- **4Stim**:
  - `OnSceneStart` carries the first scene, and no `OnSceneChange` follows for it (`main.cpp:1958`).
  - A scene change fires either `OnSceneChange` or `OnSpeedChange`, never both (`main.cpp:5664-5668`).
  - `SpeedChanged` is sent only from the player's key and navigation path (`5667`), auto mode's speed-ups (`2554`) and the climax slow-down (`3781`). `PlayOnActiveScene` itself sends only `SceneChanged` (`2172-2174`).
  - So `FourThread.SetSpeed` (`7809-7821`), the C++ `Thread::setSpeed` (`8411`), and scene changes on NPC threads produce no speed event and call no `registerSpeedChangedListener` listener.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Send the speed event from inside `PlayOnActiveScene` whenever it plays, as OStim's `SetSpeed` does. Also send a scene-change event for the starting scene after the start event.

## F7. API factions, OStat form lists and action factions are missing (4Stim has no plugin file)
- **Class**: Missing (ACTIONS.md:65 mentions action factions are "skipped until the features they belong to come"; API.md doesn't mention the API factions at all)
- **Severity**: High (for dialogue and condition-based addons)
- **OStim**: `list of API forms.txt` / `API factions README.txt`. Factions that mirror an actor's state during a scene: `OStimExcitementFaction` 0xD93, `OStimActorCountFaction` 0xECA, `OStimTimesClimaxedFaction` 0xE49, `OStimTimeUntilClimaxFaction` 0xE4A (101 = more than 100 s, -1 = can't climax), `OStimSchlongifiedFaction` 0xE9C, `OStimInWaterFaction` 0xEDF, all loaded in `O:skse/src/Util/APITable.cpp:5-13`. Form lists `OStatActorList` 0x815 and `OStatPlayerPartnerList` 0xA03. Per-action `faction` / `statFaction` / `playerStatFaction` / `stimulationFaction` (`actions README.txt:30-50`), e.g. `OStimVaginalSexActorFaction` 0xE90. Also `OStimNoFacialExpressionsFaction` (`OSexIntegrationMain.psc:38`). Addons use these in dialogue and perk conditions, where Papyrus can't be called.
- **4Stim**: Nothing found (grepped for `faction`, `OStat`, `ExcitementFaction` in src, Data, docs). There is no ESP at all.
- **Fallout 4 constraint?**: Fallout 4 condition functions can only read forms such as factions and globals, so a plugin is needed. A small ESL-flagged `4Stim.esp` would do (the owner already ships ESPs for the add-ons). The action `faction` fields could already be honoured for factions from other mods' plugins, with no plugin of 4Stim's own.
- **Suggested fix**: Ship `4Stim.esl` with `4StimExcitementFaction`, `4StimActorCountFaction`, `4StimTimesClimaxedFaction`, `4StimTimeUntilClimaxFaction`, `4StimSchlongifiedFaction` and `4StimInWaterFaction`, plus the two stat form lists, and keep their ranks updated. Read the action `faction` / `statFaction` fields now.

## F8. `FourActor.Climax(Act, IgnoreStall = true)` permanently unstalls the actor, and the climax doesn't happen in the call
- **Class**: Different (undocumented)
- **Severity**: Medium
- **OStim**: `threadActor->orgasm(ignoreStall)`: one climax, done in the call. Any stall stays (`O:skse/src/Papyrus/PapyrusThreadActor.h:83-88`).
- **4Stim**: `FourActor.Climax` (`FourActor.psc:133-138`) calls `FourStim.Climax`. That clears the actor's stall (`Excitement::SetStalled(id, false)`) and sets excitement to 100 (`main.cpp:4705-4711`). The climax happens on a later tick, so `GetTimesClimaxed` right after is unchanged. A mod that stalls an actor and forces one scripted climax loses the stall.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Add a native `Climax(actor, ignoreStall)` that climaxes at once (as `HandleClimax`) without touching the stall flag.

## F9. Thread-level climax stall copied onto each actor; `PermitActors` ignored; `CheckThread` missing
- **Class**: Different (undocumented)
- **Severity**: Medium
- **OStim**: The thread has its own stall flag. `PermitClimax(thread, PermitActors = false)` clears only the thread flag and clears the actors' stalls only when `PermitActors` is true. `IsClimaxStalled(thread)` reads the thread flag. `OActor.IsClimaxStalled(Act, CheckThread = true)` also checks the thread (`PapyrusThread.h:166-191`, `PapyrusThreadActor.h:71-81`).
- **4Stim**: `T_StallClimax` / `T_PermitClimax` set or clear every actor's own flag and ignore `PermitActors` (`main.cpp:7846-7861`), so `PermitClimax(t)` also wipes stalls another mod set on single actors. `T_IsClimaxStalled` returns true if any actor is stalled (`7862-7866`). `FourActor.IsClimaxStalled(Act)` has no `CheckThread` parameter (`FourActor.psc:148`), so OStim code calling it with two arguments fails to compile.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Add a thread stall flag to `ActiveScene` and have the excitement climax check consult it; take `PermitActors` into account; add `CheckThread`.

## F10. `FourThread.NavigateTo` doesn't route through the navigation graph
- **Class**: Different (not in docs; TODO.md:57 lists only `QueueNavigation`'s wait and `WarpTo`'s fades)
- **Severity**: Medium
- **OStim**: `Thread::navigateTo` finds a route of up to `navigationDistanceMax` steps for the actors (`m_currentNode->getRoute`) and plays it step by step. It warps (with fades) only when there is no route (`O:skse/src/Core/Thread/ThreadNavigation.cpp:135-163`).
- **4Stim**: `ThreadGoTo` warps straight to the scene unless a transition is playing (`main.cpp:7776-7801`), even though a route finder already exists for auto mode (`Reachable`, `2293-2330`). So NavigateTo and WarpTo differ only during a transition.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Use `Reachable` for `NavigateTo` and queue the route.

## F11. A given starting scene: 4Stim reorders the actors and fails where OStim falls back
- **Class**: Different (undocumented; API.md:46 describes the ordering only for starts without a scene)
- **Severity**: Medium
- **OStim**:
  - If the given scenes don't fit the actors **in the order given**, they are dropped and a random scene is picked (`ThreadStarter.cpp:47-58`, `Node::fulfilledBy` at `Graph/Node.cpp:63-76`).
  - Actors are sorted (dominant actors first, then those with a penis) only when there is no starting scene (`NPCThreadStarter.cpp:23-25`), so `Actors[i]` plays role i.
  - With a starting scene and no furniture given, it still looks for a bed (`NPCThreadStarter.cpp:12-14`, `PlayerThreadStarter.cpp:41-44`).
  - Duplicate or unloaded actors return -1 (`ThreadStarter.cpp:24-36`).
- **4Stim**:
  - It always re-sorts (penis first, dominant first, `main.cpp:7556-7559`) and then tries every role order (`SceneRegistry.cpp:1095-1117`). For roles without a sex requirement, the mod's order can be swapped.
  - A wrong actor count returns -1 (`7536-7541`); roles that don't fit fail after an ID was returned (`7603-7606`). Neither falls back to a random scene.
  - With a starting scene there is no furniture search (`7575`, `builder.starting.empty()`).
  - No check for duplicate or unloaded actors (`CantUse`, `7457-7476`).
- **Fallout 4 constraint?**: No.
- **Suggested fix**: When starting scenes are given, try the given order first and fall back to a random scene. Search for a bed as OStim does. Reject duplicates.

## F12. FourLibrary: "Position" means a role in 4Stim but an actor in OStim; MultiActorTag semantics differ
- **Class**: Different (API.md:54 says "any role order that fits", but not the consequences)
- **Severity**: Medium
- **OStim**: `getRandomNode` keeps only scenes that `fulfilledBy` the actors in the given order (`O:skse/src/Graph/GraphTable/GraphTableNodes.cpp:205-207`). So `…SingleActorTag(Actors, Position, Tag)` asks for the tag on the role **Actors[Position]** will play, and a returned scene can be started with `Actors` as they are. `…MultiActorTagForAny(Actors, Tags)` checks `Tags[i]` against role i, and at least one i must match (`OLibrary.psc:317-329`).
- **4Stim**: `RandomScene` takes any permutation (`main.cpp:8000-8030`), so Position filters a scene role, which a given actor may not end up in. `…MultiActorTagForAny` asks whether any role has any of the tags (`FourLibrary.psc:88-96`). Tags aren't lowercased (OStim lowercases); unverified whether 4Stim's scene tags are stored lowercase.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Add a fixed-order mode to `FindRandomScene` (the registry already has `ListFilter::fixedOrder`, `SceneRegistry.cpp:1127`) and use it in the OLibrary-named wrappers. Implement per-index MultiActorTag.

## F13. OStim's second C++ API (OstimNG-API) is missing, and the header wrongly says OStim has no alignment API
- **Class**: Missing (undocumented)
- **Severity**: Medium
- **OStim**: Besides the interface map, OStim exports `RequestPluginAPI_Thread` and `RequestPluginAPI_Scene` (`O:skse/src/ModAPI/OstimNG-API-Thread.cpp:663`, `OstimNG-API-Main.cpp:120`).
  - `IThreadInterface` (`OstimNG-API-Thread.h:135-290`): player thread ID, actors with excitement, sex and times climaxed, navigation options with icons, `NavigateToScene`, speed get/set, `GetActorAlignment` / `SetActorAlignment` (offset, scale, rotation, SoS bend), scene search, the options-menu tree, `SetExternalUIEnabled` with control callbacks and key data (used by external UI replacers), `MigrateThread`, `HasCompatibleNode`, thread event callbacks.
  - `ISceneInterface`: `StartScene`, couple, threesome and foursome starts, `StopScene`, auto mode.
- **4Stim**: Nothing (grepped `RequestPluginAPI`, `OstimNG_API`). `include/4Stim/PluginInterface.h:90` and API.md:107 call "Alignment" "4Stim's own; OStim has none". That is wrong: OstimNG-API has alignment get/set.
- **Fallout 4 constraint?**: No (an exported function works the same with F4SE).
- **Suggested fix**: At least document it as not ported and fix the "OStim has none" comment. Port `IThreadInterface` if external UI or companion plugins are a goal.

## F14. Papyrus API scripts with no 4Stim counterpart
- **Class**: Missing (undocumented except OLibrary/OMetadata partially in TODO.md:57)
- **Severity**: Medium
- **OStim → 4Stim**:
  - **`OActorUtil`**: `ToArray`, `EmptyArray`, `CreateArray`, `Sort`, `SelectIndexAndSort`, `GetActorsInRange(V2)`, `HasSchlong`, `FulfillsCondition`, `SayTo` / `SayAs`, `ActorsToNames`. Missing. `ToArray` and `Sort` appear in most addons and OStim's own example (`OThreadBuilder.psc:14`).
  - **`OSequence`**: 12 random-sequence functions. Missing (4Stim has sequences but no random lookup).
  - **`OJSON`**: missing (F4).
  - **`OCSV`**: missing (`FourLibrary.SplitCSV` only).
  - **`OEvent.IsChildOf`**: missing (events have supertypes in 4Stim, `EVENTS.md:49`).
  - **`OActionData.Fulfills{Actor,Target,Performer}Conditions`**: missing.
  - **`OPlayerThread.SetPlayerControl`**: missing (only the builder's `NoPlayerControl`).
  - **`OData`** (per-actor equip object and voice set selection, per-actor and per-action stimulation overrides, reset/export/import): mostly missing. `FourStim.ExportSettings` / `ImportSettings` and `FourActor.SetEquipObjectID` exist.
  - **`OUndress`** (Papyrus undress override hooks, `UsePapyrusUndressing`): missing.
  - **`OSettings`** (settings pages for addons): missing (4Stim uses MCM).
  - **`OStimAddon`**: missing.
  - **`OFurniture.GetOffset` / `GetSceneID` / `ResetClutter`**: missing.
  - **`OSexIntegrationMain`** legacy layer through `OUtils.GetOStim()`: `GetAPIVersion`, `StartScene(Dom, Sub, …)`, `IsActorActive`, `IsActorInvolved`, `AnimationRunning`, `GetActors`, `GetCurrentAnimation`, `EndAnimation`, `GetActorExcitement`, `SetOrgasmStall`, `AddSceneMetadata`, `GetTimesOrgasm`, `IsPlayerInvolved`, `GetFurniture`, ~150 MCM-setting properties: none.
  - For API versioning, F4SE's `F4SE.GetPluginVersion("4Stim")` (`/tmp/f4se_src/scripts/modified/F4SE.psc:20`) would play the role of `GetAPIVersion`, but this isn't documented.
- **Fallout 4 constraint?**: Only `OSKSE` and `OSANative` (SKSE and UIExtensions specifics) are irrelevant.
- **Suggested fix**: Port `FourActorUtil` (`ToArray`, `Sort`, `GetActorsInRange`), `FourSequence`, `FourJSON`, `FourEvent.IsChildOf`, and `FourPlayerThread.SetPlayerControl` first. Document `F4SE.GetPluginVersion("4Stim")` as the API version. List the rest in API.md as not ported.

## F15. FourActor: missing functions and signatures that break compilation
- **Class**: Missing / Different
- **Severity**: Medium
- **OStim** (`OActor.psc`):
  - `float PlayExpression(...)` returns the expression's duration (`:138`); 4Stim returns `bool` (`FourActor.psc:210`). `float d = OActor.PlayExpression(...)` won't compile after renaming (Papyrus has no implicit bool-to-float cast), and the value means something else.
  - `IsClimaxStalled(Act, CheckThread)`: see F9.
  - `IsObjectEquipped`, `SetObjectVariant`, `UnsetObjectVariant` (`:280-300`): exist as `FourStim.*` natives (`FourStim.psc:81-83`) but aren't wrapped in `FourActor`.
  - `UndressPartial` / `RedressPartial` (slot mask): missing.
  - `SetExpressionsEnabled`, `HasExpressionOverride`: missing.
  - Deprecated wrappers `UpdateExpression`, `SortActors`, `GetSceneID`: missing.
  - `VerifyActors`: OStim checks not disabled, not deleted, not a child, not dead, a valid actor type, not in a thread (`O:skse/src/Core/Core.cpp:33-35`). 4Stim checks only None, dead and in a scene (`FourActor.psc:100-109`), so it returns **true for a child**. Starting still refuses children (`CantUse`), but the API answer is wrong.
- **4Stim**: as above.
- **Fallout 4 constraint?**: Only `UndressPartial` (Fallout 4 slots differ; Skyrim masks could be mapped with UNDRESS.md's table).
- **Suggested fix**: Return the expression duration as a float; add the three object-variant wrappers, `CheckThread`, `SetExpressionsEnabled` / `HasExpressionOverride`, and a mask-mapped partial undress; make `VerifyActors` native and use `CantUse` plus the child, disabled and deleted checks.

## F16. FourMetadata has 22 of OMetadata's 282 functions; FourLibrary has 37 of 97
- **Class**: Partial (TODO.md:57 notes only the library's ForAll and action variants)
- **Severity**: Medium
- **OStim → 4Stim** (`OMetadata.psc`): missing groups:
  - `GetAnimationId`; `HasRequirement` and its Any/All/CSV forms;
  - `HasAnyActorTag` / `HasAllActorTags` / overlap, `GetSceneTagOverlap`;
  - `HasActions`, `FindAnyAction`, `FindActions`, `FindAllActions`, and every `…ForActor(s)`, `…ForTarget(s)`, `…ForPerformer(s)`, `…ForActorAndTarget`, `…ForMate(s)`, `…ForParticipant(s)` variant (`FindActionForActor` / `ForTarget` exist; all performer, mate and participant forms are missing), `Find*SuperloadCSV(v2)`;
  - `GetActionTypes` / `GetActionActors` / `Targets` / `Performers` arrays; `GetActionTags`, `HasActionTagOnAny`, Any/All action tags and overlap;
  - custom action data (`Has` / `Get` / `IsCustomAction{Actor,Target,Performer}{Int,Float,String}[List]`, ~150 functions), `GetCustomSceneActor{Int,Float}{Min,Max,Sum,Product}`;
  - `ScenesToNames`.

  `OLibrary`: `GetScenesInRange`, all MultiActorTag `ForAll` / `Any` / `All` CSV-matrix forms, scene-tag + multi-actor combinations, `…ActionForActor` / `ForTarget` / `ForActorAndTarget`, `GetRandomSceneSuperloadCSV`.
- **4Stim**: `FourMetadata.psc`, `FourLibrary.psc`; `FindRandomScene` covers some combinations, but under a different name.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Wrap the common Find* and Has* families in Papyrus over the existing natives (cheap). Decide whether 4Stim's action files will carry OStim's `customData` and whether its scene roles carry requirements.

## F17. API.md attributes 4Stim's own events to OStim
- **Class**: Different (documentation)
- **Severity**: Low
- **OStim**: There is no `ostim_npcscene_start` and no `ostim_thread_sequenceend` (grepped `npcscene` and `sequenceend` in `O:skse/src` and `O:data/Scripts/Source`: no hits).
- **4Stim**: API.md:89 says the scene event functions "are OStim's mod events (`ostim_thread_start`, `ostim_thread_end`, `ostim_npcscene_start`, `ostim_thread_sequenceend`...)". `OnNPCSceneStart/End` and `OnSequenceEnd` are 4Stim additions.
- **Fallout 4 constraint?**: n/a.
- **Suggested fix**: Mark them as 4Stim's own and give the real OStim mapping (the table in F5).

## F18. Player-thread starts don't wait for dialogue to end
- **Class**: Different (undocumented)
- **Severity**: Medium
- **OStim**: `startPlayerThread` polls every 250 ms until no actor is in dialogue, then starts (`PlayerThreadStarter.cpp:17-27`). Addons usually start scenes from a dialogue topic's end fragment, while the conversation is still closing.
- **4Stim**: No wait (grepped `dialogue` / `IsInDialogue` in `FourStimMenu.psc`, `FourStimScene.psc`, `main.cpp` thread builder). `StartPickedCast` starts at once (`FourStimMenu.psc:146-185`). Unverified how Fallout 4's dialogue camera and `PlayIdle` react to this.
- **Fallout 4 constraint?**: No. Fallout 4 has `Actor.IsInDialogueWithPlayer()` / `IsTalking`.
- **Suggested fix**: Wait for dialogue to end before placing the actors, as OStim does.

## F19. Plugin interface: version number and when 4Stim answers
- **Class**: Different
- **Severity**: Low
- **OStim**: `getVersion()` returns the packed DLL version, e.g. 0x07050010 (`O:skse/src/PluginInterfaceImplementation/Threading/ThreadInterfaceImpl.cpp:9-11`), and the header marks functions by ABI version (`ThreadBuilder.h:30`). The listener for interface requests is registered in `SKSEPlugin_Load` (`O:skse/src/Main.cpp:107`), so a plugin can ask during its own `kPostLoad`.
- **4Stim**: `getVersion()` returns 1 (`main.cpp:8505`, `8551`). The request listener is registered only when 4Stim itself receives `kPostLoad` (`main.cpp:9360`). A plugin whose `kPostLoad` handler runs before 4Stim's and asks then gets `interfaceMap == nullptr`; this depends on F4SE's plugin order (unverified), and the header says "at kPostLoad or later". Other differences are documented in the header (namespace, `'4ST'`, scene IDs instead of `Node*`, no partial undress, `setDuration` in float seconds) and are fine. Not documented: OStim's 0x07050010 `…Safe` builder functions are absent, `Thread` has `isRunning()` at a different vtable position, and the builder's start isn't synchronous (F2).
- **Fallout 4 constraint?**: No.
- **Suggested fix**: Register the interface listener in `F4SEPlugin_Load`. Return 4Stim's packed version from `getVersion()` and mark ABI versions in the header.

## F20. `CallEvent` treats default arguments differently
- **Class**: Different
- **Severity**: Low
- **OStim**: `OThread.CallEvent(thread, event, Actor, Target = -1, Performer = -1)`. An Actor out of range (including -1) does nothing; Target -1 becomes the actor; Performer -1 becomes the actor (`PapyrusThread.h:275-289`).
- **4Stim**: Actor -1 becomes role 0 of the thread (through `GetActor(ThreadID, 0)` and `self`). Target -1 stays "none", so `FourStim_OnEvent` gets `None` and the event's `target` stimulation and reaction aren't applied (`main.cpp:4650-4651`, `EVENTS.md:56`). Performer matches.
- **Fallout 4 constraint?**: No.
- **Suggested fix**: In `FourThread.CallEvent`, default Target to the actor and reject Actor < 0, as OStim does.

## F21. Small return-value differences
- **Class**: Different
- **Severity**: Low
- `FourActor.GetExcitementMultiplier` returns 1.0 for an actor not in a scene; OStim returns -1 (`main.cpp:4727`; `PapyrusThreadActor.h:41-47`).
- `FourMetadata.GetMaxSpeed` / `GetDefaultSpeed` return -1 for an unknown scene; OStim returns 0 (`main.cpp:8090`; `PapyrusMetadata.h:69-81`).
- `FourActor.Mute` / `Unmute` are deferred (`main.cpp:4969-4980`), so `IsMuted` right after a call returns the old value.
- `FourThread.Stop`: OStim fades out the player thread when fades are on (`Thread.cpp:624-645`); whether `EndActiveScene` does the same was not checked.

---

## Function map (OStim → 4Stim)

| OStim script | Functions | Present in 4Stim (renamed) | Missing / different |
|---|---|---|---|
| OThread | 37 | 37 in `FourThread` (all natives bound, checked) | `QueueNavigation` / `QueueWarp` don't wait (documented); `WarpTo`'s / `PlaySequence`'s fades and `NavigateTo` flag unused (documented); `NavigateTo` doesn't route (F10); stall semantics (F9); everything asynchronous (F2, F3); `CallEvent` defaults (F20); `PlaySequence` returns bool (compatible) |
| OThreadBuilder | 19 (+Example) | 19 in `FourThreadBuilder` | `NoPostDialogue` does nothing (documented); `NavigateTo` args unused; starting-scene ordering and fallback (F11); single pending slot (F1) |
| OActor | 45 | 35 | F15 |
| OLibrary | 97 | 37 | F12, F16 |
| OMetadata | 282 | 22 | F16 |
| OFurniture | 7 | 4 | `GetOffset`, `GetSceneID`, `ResetClutter` |
| OSequence, OActorUtil, OJSON, OCSV, OEvent, OActionData, OPlayerThread, OData, OUndress, OSettings, OStimAddon, OUtils/OSexIntegrationMain | ~350 | 0 (bits in `FourStim.*`) | F14 |
| OSKSE, OSANative, OAIUtils, OIntUtil, OUtility, OBarsScript, OSexBar, OStimSubthread, OSexIntegrationMCM, OStimPlayerAliasScript | internal / legacy | not needed except `OIntUtil.CreateArray` and `OUtility.Translate` (small) | |
| Mod events | 15 names | 7 function callbacks | F4, F5, F6 |
| API forms | 6 factions + 2 form lists + action factions | 0 | F7 |
| PluginInterface Threads / SexToys | | ported (renamed namespace) | F19 |
| PluginInterface Graph / Furniture / Settings | | missing (documented) | |
| ModAPI (OstimNG-API Thread/Scene) | ~45 | 0 | F13 |

## Checked and matching
- Every `Native` declared in `FourStim`, `FourThread`, `FourThreadBuilder`, `FourLibrary`, `FourMetadata`, `FourActor` and `FourFurniture` is bound in `RegisterPapyrusFunctions`, and nothing is bound that isn't declared (checked with a script against `main.cpp` `BindNativeMethod`).
- `OThread` / `OThreadBuilder` names, argument order and defaults match `FourThread` / `FourThreadBuilder` (`FourThread.CallEvent`'s first parameter is renamed `ActorPosition`; Papyrus doesn't care).
- Thread 0 is the player's; NPC thread IDs count up from 1; `GetSpeed` returns -1 for no thread; `GetActors` is in role order; `GetActorPosition` returns -1 if not found.
- `GetExcitement` returns -1 for actors not in a scene, as OStim; `SetExcitement` / `ModifyExcitement` clamp to 0-100; `ModifyExcitement`'s `RespectMultiplier` defaults to false, as OStim.
- `FourMetadata.GetMaxSpeed` = number of speeds - 1; `GetActionActor` / `Target` return the action's roles.
- `FourFurniture.FindFurniture` / `FindFurnitureOfType`: same parameter order (4Stim gives `Radius` a default; compatible).
- `FourLibrary` random selection leaves out transitions and `noRandomSelection` scenes, as `GraphTable::getRandomNode`.
- `ostim_event`'s performer default (the actor) matches 4Stim's.
- C++: `InterfaceMap`, `InterfaceExchangeMessage` and `PluginInterface` have the same layout; `ThreadInterface` has the same listener set and builder; `SexToy`, `SexToyReloadListener` and `SexToyInterface` match OStim's exactly; the listener accepts requests from any sender (`std::string_view{}` becomes a nullptr sender in CommonLibF4).
- Per-actor metadata is written immediately, lowercased, and kept only while the actor is in a scene (OStim: the same lifetime).
