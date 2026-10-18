Scriptname FourStimActor Native Hidden
{OStim NG's OActor for 4Stim (docs\API.md): one actor in a scene. The common
functions, with OStim's names; most wrap FourStim.* functions.}

; The thread (scene) the actor is in, -1 if none. The player's is 0.
int Function GetThreadID(Actor Act) Global Native
bool Function IsInOStim(Actor Act) Global
	return FourStim.IsInScene(Act)
EndFunction
; 4Stim's name for IsInOStim.
bool Function IsIn4Stim(Actor Act) Global
	return FourStim.IsInScene(Act)
EndFunction

; Whether they all exist, aren't dead and aren't in a scene already.
bool Function VerifyActors(Actor[] Actors) Global
	int i = 0
	while i < Actors.Length
		if Actors[i] == None || Actors[i].IsDead() || FourStim.IsInScene(Actors[i])
			return false
		endif
		i += 1
	endwhile
	return true
EndFunction

bool Function HasSchlong(Actor Act) Global
	return FourStim.HasPenis(Act)
EndFunction

; ---- Excitement ----
float Function GetExcitement(Actor Act) Global
	return FourStim.GetExcitement(Act)
EndFunction
Function SetExcitement(Actor Act, float Excitement) Global
	FourStim.SetExcitement(Act, Excitement)
EndFunction
Function ModifyExcitement(Actor Act, float Excitement, bool RespectMultiplier = false) Global
	FourStim.AddExcitement(Act, Excitement, RespectMultiplier)
EndFunction
float Function GetExcitementMultiplier(Actor Act) Global
	return FourStim.GetExcitementMultiplier(Act)
EndFunction
Function SetExcitementMultiplier(Actor Act, float Multiplier) Global
	FourStim.SetExcitementMultiplier(Act, Multiplier)
EndFunction

; ---- Climax ----
Function Climax(Actor Act, bool IgnoreStall = false) Global
	if !IgnoreStall && FourStim.IsClimaxStalled(Act)
		return
	endif
	FourStim.Climax(Act)
EndFunction
int Function GetTimesClimaxed(Actor Act) Global
	return FourStim.GetTimesClimaxed(Act)
EndFunction
Function StallClimax(Actor Act) Global
	FourStim.StallClimax(Act, true)
EndFunction
Function PermitClimax(Actor Act) Global
	FourStim.StallClimax(Act, false)
EndFunction
bool Function IsClimaxStalled(Actor Act) Global
	return FourStim.IsClimaxStalled(Act)
EndFunction

; ---- Sound ----
Function Mute(Actor Act) Global
	FourStim.Mute(Act)
EndFunction
Function Unmute(Actor Act) Global
	FourStim.Unmute(Act)
EndFunction
bool Function IsMuted(Actor Act) Global
	return FourStim.IsMuted(Act)
EndFunction

; ---- Clothes and weapons ----
Function Undress(Actor Act) Global
	FourStim.UndressActor(Act)
EndFunction
Function Redress(Actor Act) Global
	FourStim.RedressActor(Act)
EndFunction
Function RemoveWeapons(Actor Act) Global
	FourStimScene.HolsterForScene(Act)
EndFunction
Function AddWeapons(Actor Act) Global
	FourStimScene.RearmAfterScene(Act)
EndFunction

; ---- Equip objects (docs\OPTIONS.md) ----
bool Function EquipObject(Actor Act, string Type) Global
	return FourStim.EquipObject(Act, Type)
EndFunction
Function UnequipObject(Actor Act, string Type) Global
	FourStim.UnequipObject(Act, Type)
EndFunction
string Function GetEquipObjectID(Actor Act, string Type) Global
	return FourStim.GetEquipObjectID(Act, Type)
EndFunction
; The variant used for this actor from now on (saved with the game).
bool Function SetEquipObjectID(Actor Act, string Type, string ID) Global
	return FourStim.SetEquipObjectID(Act, Type, ID)
EndFunction

; ---- Auto transitions ----
bool Function AutoTransition(Actor Act, string Type) Global
	return FourStim.AutoTransition(Act, Type)
EndFunction

; ---- Metadata, kept while the actor's in a scene ----
bool Function HasMetadata(Actor Act, string Metadata) Global Native
Function AddMetadata(Actor Act, string Metadata) Global Native
string[] Function GetMetadata(Actor Act) Global Native
bool Function HasMetaFloat(Actor Act, string MetaID) Global Native
float Function GetMetaFloat(Actor Act, string MetaID) Global Native
Function SetMetaFloat(Actor Act, string MetaID, float Value) Global Native
bool Function HasMetaString(Actor Act, string MetaID) Global Native
string Function GetMetaString(Actor Act, string MetaID) Global Native
Function SetMetaString(Actor Act, string MetaID, string Value) Global Native

; ---- Expressions (docs\EXPRESSIONS.md) ----
; Plays an event expression ("climax", ...) until cleared; false if unknown.
bool Function PlayExpression(Actor Act, string Expression) Global Native
Function ClearExpression(Actor Act) Global Native
