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
| `OEvent` | `FourEvent` | `IsChildOf` for events |
| `OData` | `FourData` | OStim's data functions: equip objects, voice sets, stimulation, undressing mask, settings export / import, `Localize`; `ReloadScene` reloads every scene |
| `OSexIntegrationMain` events | `FourStim.RegisterForSceneEvents` | scene events (below) |
| `PluginInterface` (SKSE) | `include/4Stim/PluginInterface.h` (F4SE) | C++ |
| `OstimNG-API` (SKSE) | `include/4Stim/FourStimNG-API.h` (F4SE) | C++: scenes and threads for external UIs |

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

`Create` returns -1 for an actor who isn't eligible, as OStim's `isEligible`: disabled, deleted, a child, dead, without an actor type (ACTORS.md), or already in a scene or a starting one. `Start` / `QuickStart` return the thread ID (-1 if it can't start: one of those, or in combat, not loaded, or given twice). As in OStim, the thread exists from then on: `IsRunning` is true, and metadata, `GetActors` (in the order given) and the stall flag work on the next line, read and written at once (`AddMetadata` then `HasMetadata` is true). The scene itself plays a moment later, once Papyrus has placed the actors: `GetScene` is "" until then, and what needs the scene playing (`SetSpeed`, `NavigateTo` / `WarpTo`, `Stop`, `StartAutoMode` / `StopAutoMode`, `StallClimax`) is kept and done as soon as it plays. Any number of threads can start at once, each with its own options and furniture. A start that fails after `Start` returned (no scene fits, the game refuses the animations) ends the thread: `IsRunning` goes false and `4Stim.log` says why; one that hasn't started within 30 seconds is dropped.

**A scene with the player goes through OStim's start flow first** (its `PlayerThreadStarter`), so the thread's scene starts once that's done (the 30 seconds count from then):
1. It waits while the player is in dialogue: a scene started from a topic's end fragment starts once the conversation has closed.
2. Furniture, unless `SetFurniture`, `NoFurniture` or `SetEnableFurniture=0`: with `SetSelectFurniture`, the picker asks "Which furniture do you want to use?" (None, then the furniture near that a scene for them plays on); without it, the nearest such piece. With a starting scene, it asks "Do you want to use the nearby ...?" about the nearest furniture that scene plays on (OStim asks about the nearest bed, whatever the scene).
3. Without a starting scene: with `addActorsAtStart`, "Add actor?" with the eligible actors within 2000 units, again after each one added, while some scene takes one more there.
4. Without a starting scene: the player's position ("Which position do you want to be in?": Dom, Sub, Third...) when `playerSelectRoleStraight` / `Gay` / `Threesome` says (for two: the same or not by who has a penis, as OStim's), else `PlayerAlwaysDom...` / `Sub...` place them. The scene then keeps that position.

These are OStim's message boxes, asked in 4Stim's picker; closing it answers None / No. 4Stim's own picker (the hotkey) has its own steps, as before.

### FourThreadBuilder

`Create(Actor[])`, `SetDominantActors(id, Actor[])` (they take the first roles, as OStim), `SetFurniture(id, ref)`, `NoFurniture(id)`, `SetDuration(id, seconds)` (the scene ends after it), `SetStartingAnimation(id, scene)`, `AddStartingAnimation(id, scene, duration, navigateTo)` (several make a sequence), `SetStartingSequence(id, sequence)`, `ConcatStartingSequence(id, sequence, navigateTo)`, `EndAfterSequence(id)`, `UndressActors(id)` (everyone undressed at the start), `NoUndressing(id)` (scenes don't undress them; the HUD still can), `NoAutoMode(id)`, `NoPlayerControl(id)` (no speed or navigation keys; as OStim's, it doesn't turn auto mode on), `SetMetadata(id, string[])` / `SetMetadataCSV(id, csv)`, `NoPostDialogue(id)` (no voice set `postSceneDialogue` as it ends, SOUND.md), `Start(id)`, `Cancel(id)`.

Without a starting scene, the builder picks a scene the way the scene picker would for those actors: an intro or idle scene that fits them (and the furniture). Its actors get OStim's order when it fits the scene's roles (dominant actors first, then those with a penis), another order when it doesn't.

With starting scenes, as OStim's:
- The actors play the roles in the order given (`Actors[i]` is role i). If the first starting scene doesn't fit them in that order, it's dropped and a start scene is picked as above. An unknown scene ID is left out with a warning (so `QuickStart` with a bad ID starts a random scene).
- Without `SetFurniture` or `NoFurniture`, the nearest furniture the first starting scene is made for is used.
- One starting scene is a plain start. Several make a sequence: an entry with no duration plays for its scene's `length` (10 s if it has none), and `navigateTo` puts in the way there from the entry before (through navigations, at most `navigationDistanceMax` steps; transitions play for their length, other scenes on the way half a second).

### FourThread

`QuickStart`, `IsRunning`, `Stop`, `GetThreadCount`, `GetAllThreadIDs`, `GetScene`, `NavigateTo` (after a transition that's playing) / `WarpTo` (at once), `QueueNavigation` / `QueueWarp` (go at once: 4Stim doesn't wait the duration), `GetSpeed` / `SetSpeed` (0-based), `PlaySequence`, `GetActors` / `GetActor(id, index)` / `GetActorPosition`, `StallClimax` / `PermitClimax` / `IsClimaxStalled`, `GetFurniture` / `GetFurnitureType` / `ChangeFurniture`, `IsInAutoMode` / `StartAutoMode` / `StopAutoMode`, `AutoTransition` / `AutoTransitionForActor`, `CallEvent`, and metadata: `HasMetadata` / `AddMetadata` / `GetMetadata`, `Has` / `Get` / `SetMetaFloat`, `Has` / `Get` / `SetMetaString` (from `Start` until the end event is sent).

When a thread ends, as OStim's: 4Stim frees its actors first (unrestrained, collision, interaction back, which takes about 1.5 s in Papyrus), and then sends the end event (`4stim_thread_end` and the rest, the scene event functions, the plugin interface's thread stop). A scene a listener starts with the same actors isn't undone by the release. The thread's metadata is still readable inside the end event. If the release never reports back, the event goes out 4 seconds after the stop.

## FourLibrary

Random scenes for a set of actors, with OStim's function names: `GetRandomScene(actors)`, `GetRandomFurnitureScene(actors, type)`, and for each, scene tags (`...WithSceneTag`, `...WithAnySceneTag`, `...WithAllSceneTags`, `CSV` versions), one actor's tags (`...WithSingleActorTag(actors, position, tag)`, `Any` / `All`, `CSV`), any actor's tags (`...WithMultiActorTagForAny`), and actions (`...WithAction`, `...WithAnyAction`, `...WithAllActions`, `CSV`), each also as `GetRandomFurnitureScene...` with a furniture type. A scene qualifies if these actors can play it in the order given, as OStim's (role 0 the first actor, by their sexes and actor properties; sort them first, `FourActorUtil.Sort`), it's not a transition, and it isn't marked `noRandomSelection`. They return "" when none fits.

All 97 of OLibrary's functions are there, under OStim's names and arguments, wrapping five natives mods can also call directly: `FindRandomScene(actors, furnitureType, anySceneTags, allSceneTags, anyActions, allActions, position, anyActorTags, allActorTags)` for scene tags, actions and one role's tags; `FindRandomSceneByRoleTags(actors, furnitureType, actorTags, allPerRole, forAll, sceneTags, sceneMode)` for OStim's multi-actor tags (`"a,b;c"`: one list per role, any or all of each, for any role or all of them, with any / all scene tags); `FindRandomSceneByActionRoles(actors, furnitureType, types, all, byActor, actorPosition, byTarget, targetPosition)` for the actions done by or to a role; `FindRandomSceneSuperload(actors, furnitureType, filters)` behind `GetRandomSceneSuperloadCSV` (its 44 filter strings, in OStim's order and formats); and `GetScenesInRange(id, actors, distance)`, the scenes up to `distance` navigations away that the actors can play, sorted by name, as OStim's. As in OStim, only the superload matches an action by its type's tags as well as its type. One difference: the random picks accept any actor order that fits a scene, where OStim's keep the order given, so a role's tags are checked on the scene's roles, not on the actor given in that place. `GetAllScenes()` lists every scene, `SplitCSV("a,b")` splits a list.

## FourMetadata

All 282 of OMetadata's functions, under OStim's names and arguments, by scene ID: the scene (`GetName`, `ScenesToNames`, `GetAnimationId` (OStim's `animation` name of a speed, "" for 4Stim's `idles` speeds), speeds, transitions, navigations, furniture, modpack), its tags and its roles' tags and requirements (`Has` / `HasAny` / `HasAll`, overlaps, `CSV` forms), its actions (types, roles, `FindAction(s)` / `FindAnyAction` / `FindAllActions` for actors, targets, performers, mates and participants, single or several positions, and the `FindAction(s)SuperloadCSVv2` searches), action tags (`HasActionTag(id, index, tag)` as OStim's, `OnAny`, `OverAll`, overlaps) and custom action data (`GetCustomActionActorInt` and its kin for targets and performers, ints, floats, strings and their lists, and `GetCustomSceneActor{Int,Float}{Min,Max,Sum,Product}`), read from an action file's `ints`, `floats`, `strings`, `intLists`, `floatLists` and `stringLists` on each side, as OStim's. They're built on a few natives other mods can call too: `FindActionsBy(id, types, mode, positions, positions2)`, `FindActionsSuperload`, `HasCustomAction` / `GetCustomAction{Int,Float,String}[List]`, `GetCustomSceneActor{Int,Float}`, `GetActorRequirements`, and list helpers (`ListHasAny` / `ListHasAll` / `ListOverlap`, `IntsOfCSV` / `FloatsOfCSV`). One change from 4Stim before 0.5.0: `HasActionTag` takes an action index, as OStim's; `HasActionTagOnAny(id, tag)` is the whole scene.

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

`include/4Stim/PluginInterface.h` is OStim's SKSE plugin interface for F4SE: copy the one header into your plugin; nothing else of 4Stim's is needed to build. At F4SE's `kPostLoad` message or later, get the interface map from 4Stim's exported `requestInterfaceMap` (`FourStimAPI::getInterfaceMap()`, OStim's `getInferfaceMap`; include `Windows.h` first), or dispatch an `InterfaceExchangeMessage` to "4Stim" and read it back:

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
- **"Alignment"** (4Stim's own interface; OStim's alignment get / set is in its second C++ API's thread interface, which 4Stim has too, below): a thread actor's alignment offset in the current scene (sideways, forward, up, rotation, scale, penis bend), read, set (saved, applied at once) or reset, as the HUD's Align tab does.
- **"SexToys"** (OStim's `SexToyInterface`, the same): device plugins register a reload listener once and add their toys whenever it's called.

Listeners are called on the game's main thread, and the `Thread` / `ThreadActor` objects stay valid for the session (a stopped thread says so with `isRunning()`).

Departures from OStim (4Stim's code, not Fallout 4): the namespace is `FourStimAPI` and the message type `'4ST'`, so a plugin can talk to both; scenes are passed by ID instead of OStim's `Graph::Node` objects, and OStim's graph, furniture and settings interfaces aren't there; there's no `undressPartial` / `redressPartial`.

## C++: the scene and thread API (OStim's OstimNG-API)

`include/4Stim/FourStimNG-API.h` is OStim's second C++ API (its `ModAPI`, `OstimNG-API`), which external scene UIs such as OStim Prism and companion plugins use. One header, no 4Stim code needed to build; call `GetAPI` at F4SE's `kPostLoad` or later (nullptr without 4Stim):

```cpp
#include "4Stim/FourStimNG-API.h"

auto scenes = FourStimNG_API::Scene::GetAPI("MyPlugin", 1, 0, 0);
auto threads = FourStimNG_API::Thread::GetAPI("MyPlugin", 1, 0, 0);
std::uint32_t id;
scenes->StartCoupleScene("MyPlugin", nullptr, "OStim2PStandingApartMF", dom, sub, &id);
threads->RegisterEventCallback([](auto event, std::uint32_t thread, void*) { /* ... */ }, nullptr);
```

- **Scene interface** (exported `RequestPluginAPI_Scene`): `StartScene` (up to 256 actors), `StartCoupleScene` / `StartThreesomeScene` / `StartFoursomeScene` (furniture and starting scene optional), `StopScene`, `SetAutoMode`, `TryGetAutoMode`.
- **Thread interface** (exported `RequestPluginAPI_Thread`): the player's thread (always 0, as OStim's) and any thread by ID: its scene and name, actors (form ID, excitement, female, has a penis, times climaxed), navigations (scene, destination past transitions, icon, label, border; none in a transition, a sequence or without player control, as OStim's), navigate, speed, transition / sequence / auto mode / player control, alignment get / set (the Align tab's offsets), scene search (by name, for the player's thread's actor count and furniture) and scene info, the options menu (4Stim's options pages: back / exit first, then the page's items, as OStim's), actor checks (`IsActorInAnyThread`, `GetActorPosition`, `HasCompatibleNode`), `MigrateThread`, the navigation rules (`IsUnrestrictedNavigation`, `IsIntendedSexOnly`) and 4Stim.ini's keys (`GetKeyData`).
- **Events**: `RegisterEventCallback` gets ThreadStarted, NodeChanged and ThreadEnded for the player's thread, as OStim's; `RegisterControlCallback` gets the navigation keys (Up, Down, Left, Right, Yes, Toggle, No) pressed in the scene you're in or watch, and the search, align and hide keys (SearchMenu, AlignMenu, KEY_HIDE).
- **External UI**: `SetExternalUIEnabled(true)` keeps 4Stim's scene HUD closed and the search picker shut; the navigation keys then only reach the control callbacks, for the external UI to act on.

Departures from OStim: the namespace is `FourStimNG_API` and the DLL `4Stim.dll`; actors and furniture are CommonLibF4's `RE::Actor*` / `RE::TESObjectREFR*`, and the plugin version is three numbers (the exports take an 8-byte `PluginVersion`, REL::Version's layout); `MigrateThread` starts a new 4Stim scene on the same furniture with the same auto mode and the actors' excitement, but not their undressing, and without a callback it refuses to block the game's main thread (-1); options items have no description yet. Strings handed out stay valid until the next call that fills the same kind of buffer.

`cgf "FourStimTest.PluginAPI"` (with a scene running) checks the exports the way another plugin reaches them and reads the player's thread through them; the callbacks it registers log events and control keys to 4Stim.log.

## Sex toys

OStim's sex toys: a device plugin (for buttplug.io, say) registers toys through the "SexToys" interface, and 4Stim drives them from the player's scene. An action's `toySlot` (on its actor, target or performer, OStim's field: `"genital"`, `"anus"`...) is where a toy goes. Each toy goes to the first slot of the scene's actions it's allowed on (`sToySynchronization`), and gets:

- an `update(baseline, peak, peakInterval)` when the scene changes, on each of the action's peaks (with the average time between the last five), or every second for actions without peaks, no more often than the toy's update interval;
- a `peak(fToyClimaxMagnitude, 1)` when its actor climaxes (`bToyClimax`);
- `stop()` when the scene moves on, `turnOn()` / `turnOff()` as it gets used and when the scene ends.

The strength comes from `sToyScaling`: none (always the strongest, OStim's default), the excitement rate, the excitement (0-1), or the scene speed (0-1), between `fToyMinScale` and `fToyMaxScale`. As OStim's, the rate is the actions' base rate times 1 + the relative speed (no sex or actor multipliers) and the speed is the speed / the number of speeds (it never reaches 1). Departure: the excitement is 0-1 (OStim passes 0-100, so with its 0-1 scale defaults it's always at the top). Toys are registered again on every save load and each time the player's scene starts (OStim's reload).

### Intiface Central, built in

With `bIntiface=1` (MCM: Sex toys, "Use Intiface Central"), 4Stim connects to [Intiface Central](https://intiface.com/central/) itself and registers every device it has as a toy, so no other mod is needed: start Intiface Central, connect the devices there, start its server, and turn the setting on (in game or before). 4Stim speaks the Buttplug protocol (version 3) to `sIntifaceAddress` (`ws://127.0.0.1:12345`, Intiface Central's default), asks it to scan for devices on connecting (`bIntifaceScan`), picks up devices that connect or drop later, and tries again every few seconds while the server isn't running. 4Stim.log says what it found.

Each device's vibrate-type motors (Buttplug's `ScalarCmd`: vibrate, oscillate, constrict...) all run at the toy's strength: the baseline, a pulse on each peak (about a third of the time between peaks), and the climax pulse. Strokers (`LinearCmd`) and rotating devices aren't driven yet. Departure from OStim: OStim has no device support of its own, only the interface for device plugins; 4Stim has both.

OStim's toy settings are per toy and per slot too, as OStim's: `4Stim.ini`'s `[SexToys]` (and the MCM's Sex toys page) are OStim's "all" toys in "all" slots, and `Data\F4SE\Plugins\4Stim\Toy Settings.json` holds the rest, in OStim's export layout: `{"toySettings": {"<toy id>" or "all": {"slotSettings": {"<slot>" or "all": {"enabled", "synchronizationType", "scalingType", "minScale", "maxScale", "minMagnitude", "maxMagnitude", "doPeaks", "minPeak", "maxPeak", "doClimax", "climaxMagnitude"}}}}}` (OStim's numbers for the types: synchronization 0 player, 1 male, 2 female, 3 scene, 4 individual; scaling 0 none, 1 stimulation rate, 2 stimulation value, 3 speed, 4 individual). As OStim's, the synchronization and the scaling (with the magnitudes, peaks and climax that go with it) come from the first of every toy and slot, every toy in this slot, this toy in every slot, this toy in this slot whose type isn't individual; set `sToySynchronization` / `sToyScaling` to `individual` for the file's to count. A toy or slot with `"enabled": false` gets nothing. The file is read again when a save loads. Departure from OStim: OStim edits these in its toy menu and keeps them in the save; 4Stim has no such menu, so they're in the file.

## playbackSpeed

A scene's speeds can each set OStim's `playbackSpeed` (docs/SCENES.md): the animation plays that much faster or slower. OStim sets its `OStimSpeed` animation graph variable; Fallout 4's graphs have none, so 4Stim scales the time step of the scene actors' animation updates instead (`bUsePlaybackSpeed`, on by default).
