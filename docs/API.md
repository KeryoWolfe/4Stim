# 4Stim for other mods

What other mods can use: OStim NG's Papyrus API scripts with 4Stim's names, scene events, and a C++ interface for F4SE plugins. Anything OStim calls "OStim" is "4Stim" here; the rest keeps OStim's names, so a Skyrim mod's calls port by renaming the script.

| OStim NG | 4Stim | What |
|---|---|---|
| `OThread` | `FourThread` | running scenes by thread ID |
| `OThreadBuilder` | `FourThreadBuilder` | starting scenes with options |
| `OLibrary` | `FourLibrary` | random scenes by tags and actions |
| `OMetadata` | `FourMetadata` | what a scene is |
| `OActor` | `FourActor` | one actor in a scene |
| `OFurniture` | `FourFurniture` | furniture types and finding furniture |
| `OActorUtil` | `FourActorUtil` | actor arrays, sorting, conditions, actors in range |
| `OSequence` | `FourSequence` | random sequences by furniture and tags |
| `OSexIntegrationMain` events | `FourStim.RegisterForSceneEvents` | scene events (below) |
| `PluginInterface` (SKSE) | `include/4Stim/PluginInterface.h` (F4SE) | C++ |

The API scripts are named `Four...` (OStim's `O...` prefix; Papyrus names can't start with a digit, so not `4...`). `FourStim.psc` keeps 4Stim's own functions (scene start, excitement, equip objects, furniture...); the scripts above wrap many of them under OStim's names.

## Threads

A thread is a running scene. The player's scene is thread **0**, as OStim's player thread; scenes without the player get **1, 2, 3...** in the order they start (OStim counts them from 1 too). `FourActor.GetThreadID(akActor)` gives an actor's thread (-1 if none).

```papyrus
Actor[] actors = new Actor[2]
actors[0] = Game.GetPlayer()
actors[1] = akPartner

; Quick: a scene that fits, on the nearest furniture with a scene for them.
int thread = FourThread.QuickStart(actors)

; With options:
int builder = FourThreadBuilder.Create(actors)
FourThreadBuilder.SetFurniture(builder, akBed)
FourThreadBuilder.SetStartingSequence(builder, "mysequence")
FourThreadBuilder.EndAfterSequence(builder)
FourThreadBuilder.NoPlayerControl(builder)
FourThreadBuilder.SetMetadataCSV(builder, "mymod,romance")
thread = FourThreadBuilder.Start(builder)
```

`Start` / `QuickStart` return the thread ID (-1 if it can't start: an actor is dead, a child, in combat, not loaded, given twice, or already in a scene or a starting one). As in OStim, the thread exists from then on: `IsRunning` is true, and metadata, `GetActors` (in the order given) and the stall flag work on the next line, read and written at once (`AddMetadata` then `HasMetadata` is true). The scene itself plays a moment later, once Papyrus has placed the actors: `GetScene` is "" until then, and what needs the scene playing (`SetSpeed`, `NavigateTo` / `WarpTo`, `Stop`, `StartAutoMode` / `StopAutoMode`, `StallClimax`) is kept and done as soon as it plays. Any number of threads can start at once, each with its own options and furniture. A start that fails after `Start` returned (no scene fits, the game refuses the animations) ends the thread: `IsRunning` goes false and `4Stim.log` says why; one that hasn't started within 30 seconds is dropped.

### FourThreadBuilder

`Create(Actor[])`, `SetDominantActors(id, Actor[])` (they take the first roles, as OStim), `SetFurniture(id, ref)`, `NoFurniture(id)`, `SetDuration(id, seconds)` (the scene ends after it), `SetStartingAnimation(id, scene)`, `AddStartingAnimation(id, scene, duration, navigateTo)` (several make a sequence), `SetStartingSequence(id, sequence)`, `ConcatStartingSequence(id, sequence, navigateTo)`, `EndAfterSequence(id)`, `UndressActors(id)` (everyone undressed at the start), `NoUndressing(id)` (scenes don't undress them; the HUD still can), `NoAutoMode(id)`, `NoPlayerControl(id)` (no speed or navigation keys, auto mode drives the scene), `SetMetadata(id, string[])` / `SetMetadataCSV(id, csv)`, `Start(id)`, `Cancel(id)`. `NoPostDialogue` is there and does nothing (4Stim has no post-scene dialogue).

Without a starting scene, the builder picks a scene the way the scene picker would for those actors: an intro or idle scene that fits them (and the furniture). Its actors get OStim's order when it fits the scene's roles (dominant actors first, then those with a penis), another order when it doesn't.

With starting scenes, as OStim's:
- The actors play the roles in the order given (`Actors[i]` is role i). If the first starting scene doesn't fit them in that order, it's dropped and a start scene is picked as above. An unknown scene ID is left out with a warning (so `QuickStart` with a bad ID starts a random scene).
- Without `SetFurniture` or `NoFurniture`, the nearest furniture the first starting scene is made for is used.
- One starting scene is a plain start. Several make a sequence: an entry with no duration plays for its scene's `length` (10 s if it has none), and `navigateTo` puts in the way there from the entry before (through navigations, at most `navigationDistanceMax` steps; transitions play for their length, other scenes on the way half a second).

### FourThread

`QuickStart`, `IsRunning`, `Stop`, `GetThreadCount`, `GetAllThreadIDs`, `GetScene`, `NavigateTo` (after a transition that's playing) / `WarpTo` (at once), `QueueNavigation` / `QueueWarp` (go at once: 4Stim doesn't wait the duration), `GetSpeed` / `SetSpeed` (0-based), `PlaySequence`, `GetActors` / `GetActor(id, index)` / `GetActorPosition`, `StallClimax` / `PermitClimax` / `IsClimaxStalled`, `GetFurniture` / `GetFurnitureType` / `ChangeFurniture`, `IsInAutoMode` / `StartAutoMode` / `StopAutoMode`, `AutoTransition` / `AutoTransitionForActor`, `CallEvent`, and metadata: `HasMetadata` / `AddMetadata` / `GetMetadata`, `Has` / `Get` / `SetMetaFloat`, `Has` / `Get` / `SetMetaString` (from `Start` until the end event is sent).

When a thread ends, as OStim's: 4Stim frees its actors first (unrestrained, collision, interaction back, which takes about 1.5 s in Papyrus), and then sends the end event (`4stim_thread_end` and the rest, the scene event functions, the plugin interface's thread stop). A scene a listener starts with the same actors isn't undone by the release. The thread's metadata is still readable inside the end event. If the release never reports back, the event goes out 4 seconds after the stop.

## FourLibrary

Random scenes for a set of actors, with OStim's function names: `GetRandomScene(actors)`, `GetRandomFurnitureScene(actors, type)`, and for each, scene tags (`...WithSceneTag`, `...WithAnySceneTag`, `...WithAllSceneTags`, `CSV` versions), one actor's tags (`...WithSingleActorTag(actors, position, tag)`, `Any` / `All`, `CSV`), any actor's tags (`...WithMultiActorTagForAny`), and actions (`...WithAction`, `...WithAnyAction`, `...WithAllActions`, `CSV`), each also as `GetRandomFurnitureScene...` with a furniture type. A scene qualifies if these actors can play it (any role order that fits their sexes and actor properties), it's not a transition, and it isn't marked `noRandomSelection`. They return "" when none fits.

All 97 of OLibrary's functions are there, under OStim's names and arguments, wrapping five natives mods can also call directly: `FindRandomScene(actors, furnitureType, anySceneTags, allSceneTags, anyActions, allActions, position, anyActorTags, allActorTags)` for scene tags, actions and one role's tags; `FindRandomSceneByRoleTags(actors, furnitureType, actorTags, allPerRole, forAll, sceneTags, sceneMode)` for OStim's multi-actor tags (`"a,b;c"`: one list per role, any or all of each, for any role or all of them, with any / all scene tags); `FindRandomSceneByActionRoles(actors, furnitureType, types, all, byActor, actorPosition, byTarget, targetPosition)` for the actions done by or to a role; `FindRandomSceneSuperload(actors, furnitureType, filters)` behind `GetRandomSceneSuperloadCSV` (its 44 filter strings, in OStim's order and formats); and `GetScenesInRange(id, actors, distance)`, the scenes up to `distance` navigations away that the actors can play, sorted by name, as OStim's. As in OStim, only the superload matches an action by its type's tags as well as its type. One difference: the random picks accept any actor order that fits a scene, where OStim's keep the order given, so a role's tags are checked on the scene's roles, not on the actor given in that place. `GetAllScenes()` lists every scene, `SplitCSV("a,b")` splits a list.

## FourMetadata

By scene ID: `GetName`, `GetActorCount`, `IsTransition`, `GetDestination`, `GetFurnitureType`, `GetModpack`, `IsNoRandomSelection`, `GetMaxSpeed` / `GetDefaultSpeed`, `GetNavigations`, `GetAutoTransition` / `GetAutoTransitionForActor`, `GetSceneTags` / `HasSceneTag` / `HasAnySceneTag` / `HasAllSceneTags` (+`CSV`), `GetActorTags` / `HasActorTag`, `GetActionCount`, `HasAction`, `HasActionTag`, `FindAction` / `FindActionForActor` / `FindActionForTarget`, `GetActionType`, `GetActionActor` / `GetActionTarget` / `GetActionPerformer`.

## FourActor

`GetThreadID`, `IsInOStim` (and 4Stim's `IsIn4Stim`), `VerifyActors`, `HasSchlong`, excitement (`Get` / `Set` / `ModifyExcitement`, multiplier), `Climax(act, ignoreStall)`, `GetTimesClimaxed`, `StallClimax` / `PermitClimax` / `IsClimaxStalled(act, checkThread)` (as OStim's: an actor's stall and their thread's are separate, a climax reached or asked for while stalled waits until both are lifted, and `Climax(act, true)` climaxes now without lifting the stall), `Mute` / `Unmute` / `IsMuted`, `Undress` / `Redress`, `RemoveWeapons` / `AddWeapons` (put back when the scene ends either way), equip objects (`EquipObject`, `UnequipObject`, `Get` / `SetEquipObjectID`), `AutoTransition`, per-actor metadata (as the thread's, kept while they're in a scene), `PlayExpression(act, event)` (an expression event from `Expressions\`, for its duration, which it returns, as OStim's) / `ClearExpression`, `SetExpressionsEnabled` / `HasExpressionOverride`, `UndressPartial` / `RedressPartial` (by slot mask), `IsObjectEquipped` / `SetObjectVariant` / `UnsetObjectVariant`, and OStim's deprecated `UpdateExpression` and `GetSceneID`. `SortActors`. `VerifyActors` turns down children, disabled and deleted actors too, as OStim's.

The equip object chosen for an actor (`SetEquipObjectID`, or the HUD's options) is saved with the game, as OStim saves its actors' equip objects.

## FourFurniture

`FindFurniture(actorCount, centerRef, radius = 0, sameFloor = 0)`: the furniture around `centerRef` a scene for that many actors could go on, nearest first (0 = any count; radius 0 = `SetFurnitureSearchDistance`, sameFloor 0 = `fFurnitureHeight`); furniture in use or blocked is left out. `FindFurnitureOfType(type, centerRef, radius, sameFloor)`: the nearest of a type (or a kind of it), or None. `GetFurnitureType(ref)`, `IsChildOf(superType, subType)`. (Walls aren't objects, so these don't return them. `GetOffset(ref)`: where a scene goes on the piece, in its own frame, as OStim's `[x, y, z, rotation, scale]` (4Stim's spot for the type: its markers, else its bounds, plus the type's offset; rotation in radians). `GetSceneID(ref)`: the thread using it, -1 if none. `ResetClutter(centerRef, radius)` is there for add-ons and does nothing, as in OStim.)

## Factions, form lists and keywords

OStim's API also works through game forms in `OStim.esp`, which add-ons read in the Creation Kit's conditions (dialogue, voice-set conditions, quests). 4Stim's are in **`4Stim.esl`** (a light plugin: enable it; it takes no load order slot). Each keeps OStim's form ID, and its editor ID reads "4Stim" where OStim's reads "OStim" (`OStat...` and `NoStim` stay). A data file that names `{"mod": "OStim.esp", "formid": ...}` gets the same form in `4Stim.esl`. As OStim's:

- **In a scene:** `4StimExcitementFaction` (rank = excitement, updated about once a second), `4StimTimesClimaxedFaction` (rank = climaxes this scene), `4StimTimeUntilClimaxFaction` (-1), `4StimActorCountFaction` (rank = the scene's actor count), `4StimSchlongifiedFaction` (has a penis). Left when the scene ends.
- **Doing an action now:** each action side's `faction` (`4StimVaginalSexActorFaction`, `4StimAnyOralTargetFaction`...), for the actor's side of every action they're on in the scene playing; and the furniture type's `faction` (`4StimBedFaction`, `4StimSeatFaction`...; FURNITURE.md).
- **Stats that stay** (saved with the game, as faction ranks are): an action side's `statFaction` (+1 per scene they did it in), `climaxStatFaction` (+1 per climax while doing it), `partnerClimaxStatFaction` (+1 per scene their partner climaxed at it), and with the player: `playerStatFaction`, `playerClimaxStatFaction`, `playerPartnerClimaxStatFaction`. Ranks stop at 101, as OStim's.
- **Form lists:** `OStatActorList` (everyone who's been in a scene), `OStatPlayerPartnerList` (everyone who's been in one with the player), and each action side's `playerStatList`, `playerClimaxStatList`, `playerPartnerClimaxStatList`.
- **Keywords:** `NoStim` on furniture keeps it out of scenes; `4StimNoStrip` (or any keyword with "NoStrip" in it) on clothes keeps them on (UNDRESS.md).

The action data comes from OStim's actions and action tags (ACTIONS.md), copied by `tools/sync-action-forms.py`; `tools/make-api-plugin.py` writes `4Stim.esl` from OStim's `OStim.esp`. Differences: factions are changed through Papyrus (`FourStimFactions.psc`), a moment after the scene changes; OStim's `OStimInWaterFaction` isn't set (4Stim doesn't check for water yet), `4StimTimeUntilClimaxFaction` stays -1 (OStim counts down climax animations), and OStim's 137 globals (its MCM settings) aren't in the plugin: 4Stim's settings are in `4Stim.ini`. Without `4Stim.esl` none of this happens, and `4Stim.log` says so once.

## Mod events

OStim sends its events as SKSE mod events (`RegisterForModEvent` / `SendModEvent`, through SKSE's `ModCallbackEvent` in OStim's `GameAPI/GameUtil.cpp`). Fallout 4's Papyrus and F4SE have none, so 4Stim's plugin keeps the registrations itself and calls the callbacks through the script engine, the way SKSE does. The events are OStim's, with "ostim_" renamed "4stim_", and carry OStim's arguments. This is 4Stim's own system for now: 4Stim sends its events through it and its add-ons can use it; a version shared by other mods may come later.

```papyrus
; on a quest or reference script
FourModEvents.RegisterForModEvent(self, "4stim_thread_start", "OnThreadStart")

Function OnThreadStart(string asEventName, string asStrArg, float afNumArg, Form akSender)
	int threadID = afNumArg as int
EndFunction
```

The differences from OStim's, all forced by Fallout 4:

- Register through `FourModEvents.RegisterForModEvent(self, ...)` instead of `RegisterForModEvent(...)`: a plugin can't add functions to Fallout 4's base script without replacing F4SE's `ScriptObject.psc`.
- The callback is a `Function`, not an `Event`: Fallout 4 only allows the game's own events as `Event`s. It takes the same arguments.
- The sender of thread events is None where OStim's is its main quest: 4Stim has no plugin file.

As with SKSE's, registrations last until a game is loaded; register again after each load (for example from the player's `OnPlayerLoadGame`, through `RegisterForRemoteEvent`). Every script on the registered form that has the callback gets it. `UnregisterForModEvent(self, name)`, `UnregisterForAllModEvents(self)`, and `SendModEvent(name, strArg, numArg, sender)` for add-ons to send their own.

| Event | strArg | numArg | sender | OStim's |
|---|---|---|---|---|
| `4stim_thread_start` | | thread ID | None | `ostim_thread_start` |
| `4stim_thread_scenechanged` | scene ID | thread ID | None | `ostim_thread_scenechanged` |
| `4stim_thread_speedchanged` | the speed's index (0 = slowest), as text | thread ID | None | `ostim_thread_speedchanged` |
| `4stim_thread_end` | JSON (FourJSON) | thread ID | None | `ostim_thread_end` |
| `4stim_actor_orgasm` | scene ID | thread ID | the actor | `ostim_actor_orgasm` |
| `4stim_furniturechanged` | furniture type | thread ID | the furniture (None for a wall) | `ostim_furniturechanged` |
| `4stim_event` | see below | | | `ostim_event` |
| `4stim_thread_sequenceend` | sequence ID | thread ID | None | 4Stim's own: a sequence played to its end |

The player's scene is thread 0, as OStim's main thread, and also gets OStim's older main-thread events: `4stim_prestart` and `4stim_start`; `4stim_scenechanged` (scene ID) and `4stim_scenechanged_<sceneID>`; `4stim_animationchanged` (scene ID, speed index); `4stim_orgasm` (scene ID, the actor's index, sender the actor); `4stim_end` (JSON, -1) and `4stim_totalend`; `4stim_spank` (sender the target, for any event that is a spank or a kind of one).

The order is OStim's (`Thread.cpp`): a start sends `4stim_thread_start`, then the first scene's speed and scene events; a scene change sends the speed event, then the scene event; every speed change sends the speed event, whoever made it (the player, auto mode, a climax, `FourThread.SetSpeed`).

`4stim_event` (an event of docs/EVENTS.md, from an annotation or `FourThread.CallEvent`) has OStim's `ostim_event` arguments instead of the four:

```papyrus
FourModEvents.RegisterForModEvent(self, "4stim_event", "OnEvent")
Function OnEvent(int aiThreadID, string asType, Form akActor, Form akTarget, Form akPerformer)
```

The end events' JSON is OStim's: `{"scene", "actors": [{"mod", "formid"}], "metadata", "originator"}` (the originator is always "normal": 4Stim's threads don't change actors yet). `FourJSON` reads it, as OStim's `OJSON`:

```papyrus
Function OnThreadEnd(string asEventName, string asJson, float afThreadID, Form akSender)
	Actor[] actors = FourJSON.GetActors(asJson)
	string sceneID = FourJSON.GetScene(asJson)
	string[] metadata = FourJSON.GetMetadata(asJson)
EndFunction
```

### Scene events (4Stim's older way)

Before mod events, 4Stim called fixed functions on registered scripts, and still does. A script on a quest or reference registers with `FourStim.RegisterForSceneEvents(self)` (again on every game load), and 4Stim calls these functions on its scripts if they have them:

```papyrus
Function FourStim_OnSceneStart(Actor[] akActors, String asSceneID)
Function FourStim_OnSceneChange(Actor[] akActors, String asOldID, String asNewID)
Function FourStim_OnSpeedChange(Actor[] akActors, String asSceneID, int aiLevel, int aiCount)
Function FourStim_OnSceneEnd(Actor[] akActors, String asSceneID)
Function FourStim_OnNPCSceneStart(Actor[] akActors, String asSceneID)   ; scenes without the player
Function FourStim_OnNPCSceneEnd(Actor[] akActors, String asSceneID)
Function FourStim_OnSequenceEnd(Actor[] akActors, String asSequenceID)  ; a sequence played to its end
Function FourStim_OnFurnitureChange(Actor[] akActors, ObjectReference akFurniture, String asType, String asSceneID)
Function FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)
Function FourStim_OnEvent(String asEvent, Actor akActor, Actor akTarget, Actor akPerformer, Actor[] akActors, String asSceneID)
```

These are 4Stim's own, not OStim's (OStim has no NPC-scene or sequence events); they carry the actors instead of a thread ID (`FourActor.GetThreadID` turns an actor into one). For anything ported from OStim, use the mod events.

## C++: the plugin interface

`include/4Stim/PluginInterface.h` is OStim's SKSE plugin interface for F4SE: copy the one header into your plugin; nothing else of 4Stim's is needed to build. At F4SE's `kPostLoad` message or later, dispatch an `InterfaceExchangeMessage` to "4Stim" and read back the interface map:

```cpp
#include "4Stim/PluginInterface.h"

FourStimAPI::InterfaceExchangeMessage message;
F4SE::GetMessagingInterface()->Dispatch(FourStimAPI::InterfaceExchangeMessage::MESSAGE_TYPE, &message, sizeof(message), "4Stim");
if (message.interfaceMap) {
	auto threads = static_cast<FourStimAPI::ThreadInterface*>(message.interfaceMap->queryInterface(FourStimAPI::ThreadInterface::NAME));
	threads->registerThreadStartListener(&myListener);
}
```

- **"Threads"** (OStim's `ThreadInterface`): `getThread(id)`; listeners for thread start, speed change, scene change (OStim's node changed), climax and thread stop; `createThreadBuilder(count, actors)` (the Papyrus builder's options; `start()` / `cancel()` free it); `getActor(actor)`. A `Thread` gives its ID, whether it's the player's, its actors, scene, speed and furniture, and can navigate, warp, change speed or stop; a `ThreadActor` gives the game actor, undresses / redresses, removes / adds weapons, and (4Stim) reads and sets excitement and climaxes.
- **"Alignment"** (4Stim's own interface; OStim's alignment get / set is in its second C++ API, `OstimNG-API`'s thread interface, which 4Stim doesn't have yet): a thread actor's alignment offset in the current scene (sideways, forward, up, rotation, scale, penis bend), read, set (saved, applied at once) or reset, as the HUD's Align tab does.
- **"SexToys"** (OStim's `SexToyInterface`, the same): device plugins register a reload listener once and add their toys whenever it's called.

Listeners are called on the game's main thread, and the `Thread` / `ThreadActor` objects stay valid for the session (a stopped thread says so with `isRunning()`).

Departures from OStim (4Stim's code, not Fallout 4): the namespace is `FourStimAPI` and the message type `'4ST'`, so a plugin can talk to both; scenes are passed by ID instead of OStim's `Graph::Node` objects, and OStim's graph, furniture and settings interfaces aren't there; there's no `undressPartial` / `redressPartial`.

## Sex toys

OStim's sex toys: a device plugin (for buttplug.io, say) registers toys through the "SexToys" interface, and 4Stim drives them from the player's scene. An action's `toySlot` (on its actor, target or performer, OStim's field: `"genital"`, `"anus"`...) is where a toy goes. Each toy goes to the first slot of the scene's actions it's allowed on (`sToySynchronization`), and gets:

- an `update(baseline, peak, peakInterval)` when the scene changes, on each of the action's peaks (with the average time between the last five), or every second for actions without peaks, no more often than the toy's update interval;
- a `peak(fToyClimaxMagnitude, 1)` when its actor climaxes (`bToyClimax`);
- `stop()` when the scene moves on, `turnOn()` / `turnOff()` as it gets used and when the scene ends.

The strength comes from `sToyScaling`: none (always the strongest, OStim's default), the excitement rate, the excitement (0-1), or the scene speed (0-1), between `fToyMinScale` and `fToyMaxScale`. Departures in the inputs: 4Stim's rate includes the sex and actor excitement multipliers (OStim's is the base rate times 1 + the relative speed), its excitement is 0-1 (OStim passes 0-100, so with its 0-1 scale defaults it's always at the top), and its speed reaches 1 at the top speed (OStim's speed / speed count never does). Toys are registered again on every save load and each time the player's scene starts (OStim's reload).

### Intiface Central, built in

With `bIntiface=1` (MCM: Sex toys, "Use Intiface Central"), 4Stim connects to [Intiface Central](https://intiface.com/central/) itself and registers every device it has as a toy, so no other mod is needed: start Intiface Central, connect the devices there, start its server, and turn the setting on (in game or before). 4Stim speaks the Buttplug protocol (version 3) to `sIntifaceAddress` (`ws://127.0.0.1:12345`, Intiface Central's default), asks it to scan for devices on connecting (`bIntifaceScan`), picks up devices that connect or drop later, and tries again every few seconds while the server isn't running. 4Stim.log says what it found.

Each device's vibrate-type motors (Buttplug's `ScalarCmd`: vibrate, oscillate, constrict...) all run at the toy's strength: the baseline, a pulse on each peak (about a third of the time between peaks), and the climax pulse. Strokers (`LinearCmd`) and rotating devices aren't driven yet. Departure from OStim: OStim has no device support of its own, only the interface for device plugins; 4Stim has both.

Departure from OStim: OStim's toy settings are per toy and per slot, saved with the game and edited in its own toy menu; 4Stim has OStim's global settings only, in `4Stim.ini` `[SexToys]` and the MCM's Sex toys page.

## playbackSpeed

A scene's speeds can each set OStim's `playbackSpeed` (docs/SCENES.md): the animation plays that much faster or slower. OStim sets its `OStimSpeed` animation graph variable; Fallout 4's graphs have none, so 4Stim scales the time step of the scene actors' animation updates instead (`bUsePlaybackSpeed`, on by default).
