Scriptname FourActorUtil Native Hidden
{OStim NG's OActorUtil for 4Stim (docs\API.md): utility functions for actors,
with OStim's names and arguments.}

; Whether the actor has a penis (OStim's schlong; 4Stim's actor properties,
; docs\ACTORS.md).
bool Function HasSchlong(Actor Act) Global
	return FourStim.HasPenis(Act)
EndFunction

; Whether the actor fulfills the perk's conditions (they needn't have the perk).
bool Function FulfillsCondition(Actor Act, Perk Condition) Global Native
bool Function FulfillsAnyCondition(Actor Act, Perk[] Conditions) Global Native
bool Function FulfillsAllConditions(Actor Act, Perk[] Conditions) Global Native

; The actor says the topic to the target. Fallout 4 departure: OStim's SayAs
; plays it in another voice type; Fallout 4's Papyrus can't, so SayAs says
; it in the actor's own voice.
Function SayTo(Actor Act, Actor Target, Topic Dialogue) Global
	if Act && Dialogue
		Act.Say(Dialogue, None, false, Target)
	endif
EndFunction
Function SayAs(Actor Act, Actor Target, Topic Dialogue, VoiceType Voice) Global
	SayTo(Act, Target, Dialogue)
EndFunction

; Arrays of actors.
Actor[] Function EmptyArray() Global Native
Actor[] Function CreateArray(int Size, Actor Filler = None) Global Native
Actor[] Function ToArray(Actor One = None, Actor Two = None, Actor Three = None, Actor Four = None, Actor Five = None, Actor Six = None, Actor Seven = None, Actor Eight = None, Actor Nine = None, Actor Ten = None) Global Native

; Sorts actors for a scene, as OStim: the dominant actors first, then those
; with a penis; then the player moved to PlayerIndex, or for two without
; one, where the PlayerAlways... settings put them.
Actor[] Function Sort(Actor[] Actors, Actor[] DominantActors, int PlayerIndex = -1) Global Native

; OStim asks the player for their role here when its PlayerSelectRole...
; settings say so. 4Stim asks in its scene picker, not from scripts: this
; sorts as Sort.
Actor[] Function SelectIndexAndSort(Actor[] Actors, Actor[] DominantActors) Global
	return Sort(Actors, DominantActors)
EndFunction

; Actors around Center, nearest first, not disabled. OStimActorsOnly: only
; actors with an actor type (docs\ACTORS.md). Condition: only those who
; fulfill the perk's conditions.
Actor[] Function GetActorsInRangeV2(ObjectReference Center, float Range, bool IncludeCenter = false, bool IncludePlayer = true, bool OStimActorsOnly = false, Perk Condition = None) Global Native
Actor[] Function GetActorsInRange(ObjectReference Center, float Range, bool IncludeCenter = false, bool IncludePlayer = true, Perk Condition = None) Global
	return GetActorsInRangeV2(Center, Range, IncludeCenter, IncludePlayer, false, Condition)
EndFunction

; Their display names.
string[] Function ActorsToNames(Actor[] Actors) Global Native
