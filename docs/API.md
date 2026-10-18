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

`Start` / `QuickStart` return the thread ID the scene will have (-1 if it can't start: an actor is dead, a child, in combat or already in a scene, or the scene doesn't fit them). The scene itself starts a moment later, once the actors are placed: `FourThread.IsRunning(thread)` says when.

### FourThreadBuilder

`Create(Actor[])`, `SetDominantActors(id, Actor[])` (they take the first roles, as OStim), `SetFurniture(id, ref)`, `NoFurniture(id)`, `SetDuration(id, seconds)` (the scene ends after it), `SetStartingAnimation(id, scene)`, `AddStartingAnimation(id, scene, duration, navigateTo)` (several make a sequence), `SetStartingSequence(id, sequence)`, `ConcatStartingSequence(id, sequence, navigateTo)`, `EndAfterSequence(id)`, `UndressActors(id)` (everyone undressed at the start), `NoUndressing(id)` (scenes don't undress them; the HUD still can), `NoAutoMode(id)`, `NoPlayerControl(id)` (no speed or navigation keys, auto mode drives the scene), `SetMetadata(id, string[])` / `SetMetadataCSV(id, csv)`, `Start(id)`, `Cancel(id)`. `NoPostDialogue` is there and does nothing (4Stim has no post-scene dialogue).

Without a starting scene, the builder picks a scene the way the scene picker would for those actors: an intro or idle scene that fits them (and the furniture). Its actors get OStim's order when it fits the scene's roles (dominant actors first, then those with a penis), another order when it doesn't.

### FourThread

`QuickStart`, `IsRunning`, `Stop`, `GetThreadCount`, `GetAllThreadIDs`, `GetScene`, `NavigateTo` (after a transition that's playing) / `WarpTo` (at once), `QueueNavigation` / `QueueWarp` (go at once: 4Stim doesn't wait the duration), `GetSpeed` / `SetSpeed` (0-based), `PlaySequence`, `GetActors` / `GetActor(id, index)` / `GetActorPosition`, `StallClimax` / `PermitClimax` / `IsClimaxStalled`, `GetFurniture` / `GetFurnitureType` / `ChangeFurniture`, `IsInAutoMode` / `StartAutoMode` / `StopAutoMode`, `AutoTransition` / `AutoTransitionForActor`, `CallEvent`, and metadata: `HasMetadata` / `AddMetadata` / `GetMetadata`, `Has` / `Get` / `SetMetaFloat`, `Has` / `Get` / `SetMetaString` (kept while the thread runs).

## FourLibrary

Random scenes for a set of actors, with OStim's function names: `GetRandomScene(actors)`, `GetRandomFurnitureScene(actors, type)`, and for each, scene tags (`...WithSceneTag`, `...WithAnySceneTag`, `...WithAllSceneTags`, `CSV` versions), one actor's tags (`...WithSingleActorTag(actors, position, tag)`, `Any` / `All`, `CSV`), any actor's tags (`...WithMultiActorTagForAny`), and actions (`...WithAction`, `...WithAnyAction`, `...WithAllActions`, `CSV`), each also as `GetRandomFurnitureScene...` with a furniture type. A scene qualifies if these actors can play it (any role order that fits their sexes and actor properties), it's not a transition, and it isn't marked `noRandomSelection`. They return "" when none fits.

All of them use one native: `FindRandomScene(actors, furnitureType, anySceneTags, allSceneTags, anyActions, allActions, position, anyActorTags, allActorTags)`, which mods can call directly for combinations OStim spells out as separate functions (its multi-actor "ForAll" and action actor / target variants aren't wrapped yet). `GetAllScenes()` lists every scene, `SplitCSV("a,b")` splits a list.

## FourMetadata

By scene ID: `GetName`, `GetActorCount`, `IsTransition`, `GetDestination`, `GetFurnitureType`, `GetModpack`, `IsNoRandomSelection`, `GetMaxSpeed` / `GetDefaultSpeed`, `GetNavigations`, `GetAutoTransition` / `GetAutoTransitionForActor`, `GetSceneTags` / `HasSceneTag` / `HasAnySceneTag` / `HasAllSceneTags` (+`CSV`), `GetActorTags` / `HasActorTag`, `GetActionCount`, `HasAction`, `HasActionTag`, `FindAction` / `FindActionForActor` / `FindActionForTarget`, `GetActionType`, `GetActionActor` / `GetActionTarget` / `GetActionPerformer`.

## FourActor

`GetThreadID`, `IsInOStim` (and 4Stim's `IsIn4Stim`), `VerifyActors`, `HasSchlong`, excitement (`Get` / `Set` / `ModifyExcitement`, multiplier), `Climax`, `GetTimesClimaxed`, `StallClimax` / `PermitClimax` / `IsClimaxStalled`, `Mute` / `Unmute` / `IsMuted`, `Undress` / `Redress`, `RemoveWeapons` / `AddWeapons` (put back when the scene ends either way), equip objects (`EquipObject`, `UnequipObject`, `Get` / `SetEquipObjectID`), `AutoTransition`, per-actor metadata (as the thread's, kept while they're in a scene), `PlayExpression(act, event)` / `ClearExpression` (an expression event from `Expressions\`, held until cleared).

The equip object chosen for an actor (`SetEquipObjectID`, or the HUD's options) is saved with the game, as OStim saves its actors' equip objects.

## FourFurniture

`FindFurniture(actorCount, centerRef, radius = 0, sameFloor = 0)`: the furniture around `centerRef` a scene for that many actors could go on, nearest first (0 = any count; radius 0 = `SetFurnitureSearchDistance`, sameFloor 0 = `fFurnitureHeight`); furniture in use or blocked is left out. `FindFurnitureOfType(type, centerRef, radius, sameFloor)`: the nearest of a type (or a kind of it), or None. `GetFurnitureType(ref)`, `IsChildOf(superType, subType)`. (Walls aren't objects, so these don't return them.)

## Scene events

A script on a quest or reference registers with `FourStim.RegisterForSceneEvents(self)` (again on every game load: registrations aren't saved). 4Stim then calls these functions on its scripts if they have them:

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

These are OStim's mod events (`ostim_thread_start`, `ostim_thread_end`, `ostim_npcscene_start`, `ostim_thread_sequenceend`...) as calls on the registered scripts: Fallout 4's Papyrus has no mod events. `FourThread` / `FourActor.GetThreadID` turn the actors into a thread ID.

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
- **"Alignment"** (4Stim's own): a thread actor's alignment offset in the current scene (sideways, forward, up, rotation, scale, penis bend), read, set (saved, applied at once) or reset, as the HUD's Align tab does.
- **"SexToys"** (OStim's `SexToyInterface`, the same): device plugins register a reload listener once and add their toys whenever it's called.

Listeners are called on the game's main thread, and the `Thread` / `ThreadActor` objects stay valid for the session (a stopped thread says so with `isRunning()`).

Departures from OStim (4Stim's code, not Fallout 4): the namespace is `FourStimAPI` and the message type `'4ST'`, so a plugin can talk to both; scenes are passed by ID instead of OStim's `Graph::Node` objects, and OStim's graph, furniture and settings interfaces aren't there; there's no `undressPartial` / `redressPartial`.

## Sex toys

OStim's sex toys: a device plugin (for buttplug.io, say) registers toys through the "SexToys" interface, and 4Stim drives them from the player's scene. An action's `toySlot` (on its actor, target or performer, OStim's field: `"genital"`, `"anus"`...) is where a toy goes. Each toy goes to the first slot of the scene's actions it's allowed on (`sToySynchronization`), and gets:

- an `update(baseline, peak, peakInterval)` when the scene changes, on each of the action's peaks (with the average time between the last five), or every second for actions without peaks, no more often than the toy's update interval;
- a `peak(fToyClimaxMagnitude, 1)` when its actor climaxes (`bToyClimax`);
- `stop()` when the scene moves on, `turnOn()` / `turnOff()` as it gets used and when the scene ends.

The strength comes from `sToyScaling`: none (always the strongest, OStim's default), the excitement rate, the excitement (0-1), or the scene speed (0-1), between `fToyMinScale` and `fToyMaxScale`. Toys are registered again on every save load and each time the player's scene starts (OStim's reload).

### Intiface Central, built in

With `bIntiface=1` (MCM: Sex toys, "Use Intiface Central"), 4Stim connects to [Intiface Central](https://intiface.com/central/) itself and registers every device it has as a toy, so no other mod is needed: start Intiface Central, connect the devices there, start its server, and turn the setting on (in game or before). 4Stim speaks the Buttplug protocol (version 3) to `sIntifaceAddress` (`ws://127.0.0.1:12345`, Intiface Central's default), asks it to scan for devices on connecting (`bIntifaceScan`), picks up devices that connect or drop later, and tries again every few seconds while the server isn't running. 4Stim.log says what it found.

Each device's vibrate-type motors (Buttplug's `ScalarCmd`: vibrate, oscillate, constrict...) all run at the toy's strength: the baseline, a pulse on each peak (about a third of the time between peaks), and the climax pulse. Strokers (`LinearCmd`) and rotating devices aren't driven yet. Departure from OStim: OStim has no device support of its own, only the interface for device plugins; 4Stim has both.

Departure from OStim: OStim's toy settings are per toy and per slot, saved with the game and edited in its own toy menu; 4Stim has OStim's global settings only, in `4Stim.ini` `[SexToys]` and the MCM's Sex toys page.

## playbackSpeed

A scene's speeds can each set OStim's `playbackSpeed` (docs/SCENES.md): the animation plays that much faster or slower. OStim sets its `OStimSpeed` animation graph variable; Fallout 4's graphs have none, so 4Stim scales the time step of the scene actors' animation updates instead (`bUsePlaybackSpeed`, on by default).
