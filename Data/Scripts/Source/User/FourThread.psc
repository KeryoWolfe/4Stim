Scriptname FourThread Native Hidden
{OStim NG's OThread for 4Stim (docs\API.md): running scenes by thread ID.
The player's scene is thread 0, as OStim's player thread; scenes without the
player count up from 1. Same functions and arguments as OStim's.}

; Starts a scene for Actors: StartingAnimation (a scene ID, "" = a random
; intro / idle scene that fits), on FurnitureRef (None = the nearest furniture
; with a scene for them, unless furniture is off). Returns the thread ID the
; scene will have, or -1; the scene starts a moment later (IsRunning).
int Function QuickStart(Actor[] Actors, string StartingAnimation = "", ObjectReference FurnitureRef = None) Global Native

bool Function IsRunning(int ThreadID) Global Native
Function Stop(int ThreadID) Global Native
int Function GetThreadCount() Global Native
int[] Function GetAllThreadIDs() Global Native

; The scene playing.
string Function GetScene(int ThreadID) Global Native

; To SceneID: after the transition playing, if one is (NavigateTo), or at once
; (WarpTo; UseFades isn't used yet).
Function NavigateTo(int ThreadID, string SceneID) Global Native
Function WarpTo(int ThreadID, string SceneID, bool UseFades = False) Global Native

; OStim waits Duration before these; 4Stim goes there as NavigateTo / WarpTo.
Function QueueNavigation(int ThreadID, string SceneID, float Duration) Global
	NavigateTo(ThreadID, SceneID)
EndFunction
Function QueueWarp(int ThreadID, string SceneID, float Duration) Global
	WarpTo(ThreadID, SceneID)
EndFunction

; The scene's auto transition of Type ("climax", "pullout"...): any role's,
; or Index's.
bool Function AutoTransition(int ThreadID, string Type) Global
	return FourStim.AutoTransition(GetActor(ThreadID, 0), Type, -1)
EndFunction
bool Function AutoTransitionForActor(int ThreadID, int Index, string Type) Global
	return FourStim.AutoTransition(GetActor(ThreadID, 0), Type, Index)
EndFunction

; 0-based.
int Function GetSpeed(int ThreadID) Global Native
Function SetSpeed(int ThreadID, int Speed) Global Native

; Plays a sequence on the thread (NavigateTo / UseFades aren't used yet).
bool Function PlaySequence(int ThreadID, string Sequence, bool NavigateTo = false, bool UseFades = false) Global Native

; In role order.
Actor[] Function GetActors(int ThreadID) Global Native
Actor Function GetActor(int ThreadID, int Index) Global Native
int Function GetActorPosition(int ThreadID, Actor Act) Global Native

; No one climaxes while stalled (excitement stops just short).
Function StallClimax(int ThreadID) Global Native
Function PermitClimax(int ThreadID, bool PermitActors = false) Global Native
bool Function IsClimaxStalled(int ThreadID) Global Native

ObjectReference Function GetFurniture(int ThreadID) Global Native
string Function GetFurnitureType(int ThreadID) Global Native
Function ChangeFurniture(int ThreadID, ObjectReference FurnitureRef, string SceneID = "") Global
	FourStim.ChangeFurniture(GetActor(ThreadID, 0), FurnitureRef, SceneID)
EndFunction

bool Function IsInAutoMode(int ThreadID) Global Native
Function StartAutoMode(int ThreadID) Global Native
Function StopAutoMode(int ThreadID) Global Native

; Metadata: tags and values other mods put on a thread (kept while it runs).
bool Function HasMetadata(int ThreadID, string Metadata) Global Native
Function AddMetadata(int ThreadID, string Metadata) Global Native
string[] Function GetMetadata(int ThreadID) Global Native
bool Function HasMetaFloat(int ThreadID, string MetaID) Global Native
float Function GetMetaFloat(int ThreadID, string MetaID) Global Native
Function SetMetaFloat(int ThreadID, string MetaID, float Value) Global Native
bool Function HasMetaString(int ThreadID, string MetaID) Global Native
string Function GetMetaString(int ThreadID, string MetaID) Global Native
Function SetMetaString(int ThreadID, string MetaID, string Value) Global Native

; Fires an event (docs\EVENTS.md); ActorPosition, Target and Performer are
; positions. (OStim calls the first "Actor", which Papyrus here takes for the type.)
Function CallEvent(int ThreadID, string EventName, int ActorPosition, int Target = -1, int Performer = -1) Global
	FourStim.CallEvent(GetActor(ThreadID, 0), EventName, ActorPosition, Target, Performer)
EndFunction
