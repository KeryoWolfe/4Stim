Scriptname FourStim Native Hidden

; Low-level natives implemented in the 4Stim F4SE plugin. Scene logic and
; step ordering live in FourStimScene; call that rather than these directly.

; ---- Scene registry ----
; Scenes come from JSON files in Data\F4SE\Plugins\4Stim\Scenes\ (see
; SCENES.md). Scene IDs are case-insensitive.

; Number of actors (roles) in a scene, or 0 if no such scene is loaded.
int Function GetSceneActorCount(String asSceneID) Global Native

; Re-reads all scene files (and the action types). Returns how many scenes loaded.
int Function ReloadScenes() Global Native

; ---- Actions ----
; What a scene's actors do to each other: an action type (id or alias, see
; docs\ACTIONS.md) with an actor (doing it), a target (it's done to) and a
; performer (moving), each a role index of the scene.

; Whether a scene has an action of this type / with this action tag
; ("oral", "intercourse", "sexual"...).
bool Function SceneHasAction(String asSceneID, String asType) Global Native
bool Function SceneHasActionTag(String asSceneID, String asTag) Global Native

; Number of actions in a scene.
int Function GetSceneActionCount(String asSceneID) Global Native

; Index of the first action of asType ("" = any type) whose actor / target is
; that role (-1 = any role), or -1 if there's none.
int Function FindSceneAction(String asSceneID, String asType, int aiActor = -1, int aiTarget = -1) Global Native

; The type id of action aiIndex, or "".
String Function GetSceneActionType(String asSceneID, int aiIndex) Global Native

; The role of action aiIndex's actor (aiWhich 0), target (1) or performer
; (2), or -1.
int Function GetSceneActionRole(String asSceneID, int aiIndex, int aiWhich) Global Native

; Every action type id loaded.
String[] Function GetActionTypes() Global Native

; An action type's display name / tags ("" / empty if there's no such type).
String Function GetActionName(String asType) Global Native
String[] Function GetActionTags(String asType) Global Native
bool Function ActionHasTag(String asType, String asTag) Global Native

; ---- Events ----
; Events (docs\EVENTS.md), as OStim's: spank, wash... from the files in
; Data\F4SE\Plugins\4Stim\Events\. Animations fire them with the
; 4StimEvent annotation; scripts with CallEvent.

; Fires asEvent in the scene akInScene is in: role aiActor did it to role
; aiTarget, role aiPerformer moving (OStim's OThread.CallEvent). aiActor -1:
; akInScene's role; aiPerformer -1: the actor's. Plays its sound, gives its
; stimulation and reactions, shakes the camera if you're in or watching the
; scene, and calls FourStim_OnEvent. False if there's no such event.
bool Function CallEvent(Actor akInScene, String asEvent, int aiActor = -1, int aiTarget = -1, int aiPerformer = -1) Global Native

; Every event id loaded.
String[] Function GetEvents() Global Native

; ---- Actors: properties and equip objects ----
; Actor properties and equip objects (docs\ACTORS.md), as OStim's: files in
; Data\F4SE\Plugins\4Stim\Actor Properties\ and Equip Objects\.

; Whether akActor has a penis (OStim's OActor.HasSchlong): an actor
; property's "sex" if one gives them one (a futa), else whether he's a man.
bool Function HasPenis(Actor akActor) Global Native

; akActor's actor type ("npc" for people), "" if they can't be in scenes.
String Function GetActorType(Actor akActor) Global Native

; OStim's OActor equip object functions, for actors in a scene: puts on an
; object of asType ("strapon", "tongue"...; false if there's none of that
; type), takes it off, says if it's on, sets its variant ("cum"...) for
; afDuration seconds (0: until unset; false if it has no such variant), or
; back to the plain one. Everything put on goes away when the scene ends.
bool Function EquipObject(Actor akActor, String asType) Global Native
Function UnequipObject(Actor akActor, String asType) Global Native
bool Function IsObjectEquipped(Actor akActor, String asType) Global Native
bool Function SetObjectVariant(Actor akActor, String asType, String asVariant, float afDuration = 0.0) Global Native
Function UnsetObjectVariant(Actor akActor, String asType) Global Native

; Every equip object type there's a file for.
String[] Function GetEquipObjectTypes() Global Native

; Which object of asType akActor wears (the file name, without .json), and
; choosing another (worn now: swapped). Until a game is loaded. The HUD's
; options do this (docs\OPTIONS.md).
String Function GetEquipObjectID(Actor akActor, String asType) Global Native
bool Function SetEquipObjectID(Actor akActor, String asType, String asID) Global Native

; OStim's OActor.Mute / Unmute / IsMuted, for actors in a scene: no moans,
; climax or event sounds while muted (an actor property's "mute" counts as
; muted).
Function Mute(Actor akActor) Global Native
Function Unmute(Actor akActor) Global Native
bool Function IsMuted(Actor akActor) Global Native

; The weapon an actor held before their scene, which FourStimScene takes off
; and puts back on when the scene ends: noted, and taken back (None if none;
; forgotten once taken). Kept in the save with the scene.
Function RememberSceneWeapon(Actor akActor, Form akWeapon) Global Native

; A drawn weapon put away (the game's own sheathe; Fallout 4's Papyrus has
; none). Called by FourStimScene.HolsterForScene.
Function SheatheForScene(Actor akActor) Global Native
int Function TakeSceneWeaponID(Actor akActor) Global Native  ; a form ID (a Form returned from the plugin arrived as the wrong type)

; Tells the plugin aiActorID's clothes changed (4Stim's undressing calls it
; after taking things off or putting them on): heels are measured again and
; the High Heels System's height turned off or back on to match.
Function HeelsChanged(int aiActorID) Global Native

; ---- Settings (the MCM page, docs\MCM.md) ----

; Reads 4Stim.ini and the MCM's settings again (the MCM calls it when a
; setting changes).
Function ReloadSettings() Global Native

; The settings in effect to / from Documents\My Games\Fallout4\4Stim\mcmsettings.json
; (OStim's export file), and back to 4Stim.ini's values (the MCM's changes
; forgotten). With a notification.
bool Function ExportSettings() Global Native
bool Function ImportSettings() Global Native
Function UseIniSettings() Global Native

; What a scene key does, by name (the MCM's hotkeys use it): "hotkey",
; "npcscene", "speedup", "speeddown", "automode", "pullout", "end",
; "search", "align", "freecam", "hideui".
Function PressKey(String asAction) Global Native

; ---- Furniture ----
; Furniture types and scenes on furniture (docs\FURNITURE.md), as OStim's
; OFurniture and OThread furniture functions.

; akFurniture's furniture type ("bed", "chair"...; "" if none).
String Function GetFurnitureType(ObjectReference akFurniture) Global Native

; Whether furniture of asSubType takes scenes made for asSuperType (the same
; type, or a kind of it: a bench is a kind of chair). OStim's IsChildOf.
bool Function IsFurnitureChildOf(String asSuperType, String asSubType) Global Native

; The furniture akActor's scene is on (None off furniture, or at a wall,
; which is no object), and its type ("" = none).
ObjectReference Function GetSceneFurniture(Actor akActor) Global Native
String Function GetSceneFurnitureType(Actor akActor) Global Native

; Moves akActor's scene onto akFurniture with asSceneID, or ("") a random
; intro / idle scene that fits there. The old furniture is free again and
; its clutter put back (SetResetClutter); with the player in it, behind a
; fade (SetUseFades). FourStim_OnFurnitureChange follows. OStim's
; OThread.ChangeFurniture.
Function ChangeFurniture(Actor akActor, ObjectReference akFurniture, String asSceneID = "") Global Native

; Opens the "Change furniture" list for the scene the HUD follows.
Function OpenFurnitureMenu() Global Native

; Internal: FourStimFurniture.Block blocked aiRefID; FourStimMenu's fade
; before a furniture change.
Function NoteFurnitureBlocked(int aiRefID) Global Native
Function ApplyFurnitureChange() Global Native

; ---- Excitement ----
; Every actor in a scene has an excitement from 0 to 100 that rises with the
; scene's actions (their stimulation, docs\ACTIONS.md). At 100 they climax:
; FourStim_OnClimax goes to registered scripts, and the scene may end
; (4Stim.ini, [Excitement]). These work on actors in a running scene only.

; 0-100, or -1 if the actor isn't in a scene.
float Function GetExcitement(Actor akActor) Global Native
Function SetExcitement(Actor akActor, float afValue) Global Native
; abUseMultiplier: scale afValue by the actor's sex and own multipliers.
Function AddExcitement(Actor akActor, float afValue, bool abUseMultiplier = true) Global Native

; How many times the actor climaxed in this scene.
int Function GetTimesClimaxed(Actor akActor) Global Native

; Makes the actor climax now (even if it was stalled).
Function Climax(Actor akActor) Global Native

; While stalled, the actor's excitement stops at 100 without a climax,
; until StallClimax(akActor, false) or Climax(akActor).
Function StallClimax(Actor akActor, bool abStall) Global Native
bool Function IsClimaxStalled(Actor akActor) Global Native

; The actor's own multiplier on excitement gain (1 = normal), on top of the
; ini's male / female multipliers. Reset when the scene ends.
float Function GetExcitementMultiplier(Actor akActor) Global Native
Function SetExcitementMultiplier(Actor akActor, float afMultiplier) Global Native

; Seconds until the actor climaxes at the current rate, or -1 if the
; current scene won't take them there.
float Function GetTimeUntilClimax(Actor akActor) Global Native

; ---- Undressing ----
; Scene actors take off what the scene's actions need, and put it back on
; when the scene ends (4Stim.ini [Undress], docs\UNDRESS.md).

; Takes off everything in the undress slots / puts back on what 4Stim took off.
Function UndressActor(Actor akActor) Global Native
Function RedressActor(Actor akActor) Global Native

; Tells 4Stim akItems were taken off akActor, to put back on when the scene
; ends (4Stim tracks what it takes off itself; this is for other mods).
Function NoteStripped(Actor akActor, Form[] akItems) Global Native

; ---- Running scenes ----
; Any number of scenes can run at once, each with its own actors (an actor is
; in one scene at most).

; Whether akActor is in a running scene.
bool Function IsInScene(Actor akActor) Global Native

; ---- Auto mode ----
; The scene moves on by itself, as OStim's auto mode (docs\AUTOMODE.md).
; NPC-only scenes start in auto mode, player scenes don't (4Stim.ini).

; Turns auto mode on / off for the scene akActor is in.
Function SetAutoMode(Actor akActor, bool abOn) Global Native
bool Function IsAutoMode(Actor akActor) Global Native

; Plays the scene akActor is in on to its asEvent auto transition ("climax",
; "pullout"... from the scene file), of role aiRole or, with -1, the first
; role that has one. False if there's none.
bool Function AutoTransition(Actor akActor, String asEvent, int aiRole = -1) Global Native

; A scene role's tags ("standing", "kneeling", "lyingback"...).
bool Function SceneActorHasTag(String asSceneID, int aiRole, String asTag) Global Native
String[] Function GetSceneActorTags(String asSceneID, int aiRole) Global Native

; ---- Animation ----

; Plays role 0 of the scene on akActor (single-actor scenes). akUnused is
; kept for compatibility; pass None.
Function StartScene(Actor akActor, Actor akUnused, String asSceneID) Global Native

; Plays roles 0 and 1 of a two-actor scene on the same frame.
Function PlaySceneIdles(Actor akActor0, Actor akActor1, String asSceneID) Global Native

; Returns the actor to its default animation state (IdleStop).
Function StopScene(Actor akActor) Global Native

; Clears both actors' poses (IdleStop) on the same frame.
Function StopPair(Actor akActor0, Actor akActor1) Global Native

; ---- Interaction ----

; Blocks the Activate/Talk prompt and stops dialogue, including an
; automatic proximity greet. Call BEFORE moving the actor near the player.
Function SuppressInteraction(Actor akActor) Global Native

; Reverses SuppressInteraction.
Function RestoreInteraction(Actor akActor) Global Native

; ---- Positioning ----

; Places akTarget afDistance units from akAnchor, offset by afAngleOffset
; degrees from akAnchor's facing, turned to face akAnchor.
Function MoveActorTo(Actor akTarget, Actor akAnchor, Float afDistance, Float afAngleOffset) Global Native

; Puts both actors on ONE spot with the same heading -- paired animations
; share an origin and carry the offset between actors themselves. The spot
; is afDistance in front of akAnchor; heading is akAnchor's plus
; afHeadingOffset (180 = facing the anchor). Use 0/0 to build the scene on
; the anchor's own spot (e.g. when the anchor is a participant).
Function PlacePair(Actor akActor0, Actor akActor1, Actor akAnchor, Float afDistance, Float afHeadingOffset) Global Native

; ---- Collision ----

; Makes the two actors' collision capsules ignore each other (they still
; collide with the world), so actors sharing one origin don't shove each
; other apart. Saves each actor's original collision filter.
Function IgnorePairCollision(Actor akActor0, Actor akActor1) Global Native

; Restores the actor's original collision filter saved by IgnorePairCollision.
Function RestoreCollision(Actor akActor) Global Native

; ---- Group scenes (any number of actors, in role order) ----
; The pair natives above for a whole cast. FourStimScene.BeginGroupScene and
; EndGroupScene wrap them.
Function IgnoreGroupCollision(Actor[] akActors) Global Native
; On the furniture picked for the scene, else on the player's spot (or the
; first actor's).
Function PlaceGroup(Actor[] akActors) Global Native
; Starts every role of asSceneID on the same frame.
Function PlayGroupIdles(Actor[] akActors, String asSceneID) Global Native
Function StopGroup(Actor[] akActors) Global Native

; ---- Camera and HUD ----

; Notes whether the player is in first person, so a scene can force third
; person and still return them to first person at the end. Call before
; Game.ForceThirdPerson().
Function SaveStartView() Global Native

; Hides the vanilla HUD and switches to the third-person free camera
; (SetUseFreeCam, at SetCameraSpeed, with SetFreeCamFOV in 4Stim.ini),
; remembering the view the player was in.
Function BeginSceneCamera() Global Native

; Leaves the free camera, returns to the view the player started in (or
; first person, SetForceFirstPerson), and shows the HUD again. Does nothing
; if BeginSceneCamera wasn't called.
Function EndSceneCamera() Global Native

; SetUseFades in 4Stim.ini: fade to black as scenes with the player start
; and end, as OStim does.
bool Function UseFades() Global Native

; 4Stim's own fade is up (true) or gone (false): the scene camera changes
; during it instead of waiting for it to clear.
Function SetSceneFade(bool abFading) Global Native

; ---- Scene picker ----

; Opens the scene picker for a new scene, with the actor with form ID
; aiTargetID already picked (0 = no one): with the player in it (unless the
; player is in a scene already), or, OpenNPCScenePicker, without. If that
; actor is in a scene, the picker opens on that scene's options. When a
; scene is picked, the plugin calls FourStimMenu.StartPickedCast.
Function OpenScenePicker(int aiTargetID) Global Native
Function OpenNPCScenePicker(int aiTargetID) Global Native

; ---- Speed and navigation (the focused scene) ----
; The focused scene is the one the player is in, or an NPC scene the player
; started to watch with the free camera. It's the scene the HUD shows.

; Gives the in-scene HUD input focus (navigation, speed, Utility, End
; scene). If the HUD isn't available (bEnableHUD=0 or its movie is
; missing), opens the picker in navigation mode instead: the scene's
; navigation links, plus "End scene" (which calls FourStimMenu.EndPlayerScene).
Function OpenSceneNavigation() Global Native

; Changes the scene speed by aiDelta (+1 faster, -1 slower), within the
; scene's speeds. Returns the new speed (1-based), or 0 if there's no
; focused scene. Also bound to the speed keys in 4Stim.ini.
int Function ChangeSceneSpeed(int aiDelta) Global Native

; Moves the focused scene to another scene with the same number of actors
; (normally one of the current scene's navigation links), keeping roles and
; the current speed where possible. Returns false if it can't.
bool Function NavigateScene(String asSceneID) Global Native

; ---- Sequences ----
; A sequence is a fixed run of scenes, each played for a set time (see
; SCENES.md). FourStimScene.BeginSequence / BeginPairSequence start one.

; Queues asSequenceID for the next scene started with these actors
; (akActor1 None for a solo sequence) and returns that scene's ID: the
; sequence's first scene, which the caller then starts. Returns "" if
; there's no such sequence or it's for a different number of actors.
String Function QueueSequence(String asSequenceID, Actor akActor0, Actor akActor1) Global Native

; Plays asSequenceID, from its first scene, on the scene akActor is already
; in. Returns false if there's no such sequence (other problems, like a
; wrong actor count, are logged in 4Stim.log).
bool Function StartSequenceOnScene(Actor akActor, String asSequenceID) Global Native

; Stops the sequence akActor's scene is playing; the scene stays where it
; is. Moving the scene yourself (NavigateScene, the HUD) also stops it.
Function StopSequence(Actor akActor) Global Native

; ---- Player controls ----

; Limits the player's controls for a scene with a 4Stim input layer:
; menus, favorites, combat, activate, sneaking, jumping, VATS, running and
; the game's own change-view are blocked; looking stays on, and movement
; stays on so the free camera can fly.
Function LockPlayerControls() Global Native

; Restores the player's controls.
Function UnlockPlayerControls() Global Native

; ---- Focused scene ----

; Form ID of the actor in role 0 or 1 of the focused scene, or 0 if there
; isn't one (or the role is empty). Use Game.GetForm(id).
int Function GetPlayerSceneActorID(int aiRole) Global Native

; How many actors the scene the player is in has (0 = not in a scene).
int Function GetPlayerSceneActorCount() Global Native

; ---- Scene events, for other mods ----

; After this, the plugin calls these functions on every script attached to
; akReceiver (a Quest or ObjectReference; from a ReferenceAlias script, pass
; GetOwningQuest() and put the functions on the quest's script), if it has
; them:
;   Function FourStim_OnSceneStart(Actor[] akActors, String asSceneID)
;   Function FourStim_OnSceneChange(Actor[] akActors, String asOldID, String asNewID)
;   Function FourStim_OnSpeedChange(Actor[] akActors, String asSceneID, int aiLevel, int aiCount)
;   Function FourStim_OnSceneEnd(Actor[] akActors, String asSceneID)
;   Function FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)
;   Function FourStim_OnEvent(String asEvent, Actor akActor, Actor akTarget, Actor akPerformer, Actor[] akActors, String asSceneID)
; akActors is in role order; aiLevel is 0 for the slowest speed. In
; FourStim_OnEvent, akTarget / akPerformer are None when the event has none.
; Registrations are NOT saved: register again on every game load.
bool Function RegisterForSceneEvents(Form akReceiver) Global Native
Function UnregisterForSceneEvents(Form akReceiver) Global Native

; ---- Debug / testing ----

; Restrains AND disables AI in one call. Not used by scenes: disabling AI
; freezes animations. Kept for console testing.
Function LockActor(Actor akActor) Global Native
Function UnlockActor(Actor akActor) Global Native

; Form ID of the actor selected in the console, or 0. Use
; Game.GetForm(id) as Actor. (Returns an ID, not an Actor: returning
; objects from natives crashed on this build.)
int Function GetSelectedActorID() Global Native
