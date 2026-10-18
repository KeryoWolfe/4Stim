Scriptname FourStimThreadBuilder Native Hidden
{OStim NG's OThreadBuilder for 4Stim (docs\API.md): set a scene up, then Start
it. Same functions as OStim's.

	int BuilderID = FourStimThreadBuilder.Create(Actors)
	FourStimThreadBuilder.SetFurniture(BuilderID, Bed)
	FourStimThreadBuilder.NoAutoMode(BuilderID)
	int ThreadID = FourStimThreadBuilder.Start(BuilderID)
}

; A builder for these actors (any order: OStim's, dominant actors first, then
; whoever fits the roles). -1 if none were given.
int Function Create(Actor[] Actors) Global Native

; These go first, into the dominant roles.
Function SetDominantActors(int BuilderID, Actor[] Actors) Global Native

; On this furniture (else the nearest with a scene for them, see NoFurniture).
Function SetFurniture(int BuilderID, ObjectReference FurnitureRef) Global Native

; Ends after Duration seconds (0 = as usual).
Function SetDuration(int BuilderID, float Duration) Global Native

; Starts with this scene (else a random intro / idle one); Add queues scenes
; to play one after another, each for Duration seconds (NavigateTo isn't used).
Function SetStartingAnimation(int BuilderID, string Animation) Global Native
Function AddStartingAnimation(int BuilderID, string Animation, float Duration = 0.0, bool NavigateTo = false) Global Native

; Starts with a sequence, or adds one after the starting scenes.
Function SetStartingSequence(int BuilderID, string Sequence) Global Native
Function ConcatStartingSequence(int BuilderID, string Sequence, bool NavigateTo = false) Global Native
Function EndAfterSequence(int BuidlerID) Global Native

Function UndressActors(int BuilderID) Global Native
Function NoAutoMode(int BuilderID) Global Native
; The player's speed, navigation and auto mode keys leave it alone (it plays in auto mode).
Function NoPlayerControl(int BuilderID) Global Native
; OStim's: 4Stim has no post-scene dialogue yet, so there's nothing to turn off.
Function NoPostDialogue(int BuilderID) Global
EndFunction
Function NoUndressing(int BuilderID) Global Native
Function NoFurniture(int BuilderID) Global Native

Function SetMetadata(int BuilderID, string[] Metadata) Global Native
Function SetMetadataCSV(int BuilderID, string Metadata) Global Native

; Starts it: the thread ID it will have, or -1. The builder is used up.
int Function Start(int BuilderID) Global Native
Function Cancel(int BuilderID) Global Native
