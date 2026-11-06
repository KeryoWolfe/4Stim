Scriptname FourActor Native Hidden
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

; Whether they can all be in a scene, as OStim's isEligible: they exist,
; aren't disabled, deleted, children or dead, and aren't in a scene already.
bool Function VerifyActors(Actor[] Actors) Global
	int i = 0
	while i < Actors.Length
		if Actors[i] == None || Actors[i].IsDisabled() || Actors[i].IsDeleted() || Actors[i].IsChild() || Actors[i].IsDead() || FourStim.IsInScene(Actors[i])
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
	FourStim.Climax(Act, IgnoreStall)
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
bool Function IsClimaxStalled(Actor Act, bool CheckThread = true) Global
	return FourStim.IsClimaxStalled(Act, CheckThread)
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
; Plays an event expression ("climax", ...) for its duration, which it
; returns in seconds (-1 if the event has no expression), as OStim's.
float Function PlayExpression(Actor Act, string Expression) Global Native
Function ClearExpression(Actor Act) Global Native
; Off: the scene's expressions stop (an event one still plays); the
; overrides (an open mouth for oral) stay unless AllowOverride is false.
Function SetExpressionsEnabled(Actor Act, bool Enabled, bool AllowOverride = true) Global Native
; Whether the actor's face is overridden (an oral action's open mouth...).
bool Function HasExpressionOverride(Actor Act) Global Native

; ---- Undressing by slot mask (the Creation Kit's armor slot masks) ----
Function UndressPartial(Actor Act, int Mask) Global Native
Function RedressPartial(Actor Act, int Mask) Global Native

; ---- Equip object state (OStim's) ----
bool Function IsObjectEquipped(Actor Act, string Type) Global
	return FourStim.IsObjectEquipped(Act, Type)
EndFunction
bool Function SetObjectVariant(Actor Act, string Type, string Variant, float Duration = 0.0) Global
	return FourStim.SetObjectVariant(Act, Type, Variant, Duration)
EndFunction
Function UnsetObjectVariant(Actor Act, string Type) Global
	FourStim.UnsetObjectVariant(Act, Type)
EndFunction

; ---- OStim's deprecated names, kept so old add-ons work ----
Function UpdateExpression(Actor Act) Global
	ClearExpression(Act)
EndFunction
int Function GetSceneID(Actor Act) Global
	return GetThreadID(Act)
EndFunction
