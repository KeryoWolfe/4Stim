Scriptname FourStimFurniture Hidden

; Furniture in use by a scene, and the clutter around it (docs\FURNITURE.md).
; Called by the plugin with form IDs.

; OStim's disableUse / enableUse: no one can use the furniture a scene is on
; (its activation blocked, and its prompt hidden), until the scene leaves it.
; Only furniture that wasn't blocked already is blocked, and the plugin is
; told (FourStim.NoteFurnitureBlocked), so it only ever unblocks its own.
Function Block(int aiRefID) Global
	ObjectReference akRef = Game.GetForm(aiRefID) as ObjectReference
	if akRef == None || akRef.IsActivationBlocked()
		return
	endif
	akRef.BlockActivation(true, true)
	FourStim.NoteFurnitureBlocked(aiRefID)
EndFunction

Function Unblock(int aiRefID) Global
	ObjectReference akRef = Game.GetForm(aiRefID) as ObjectReference
	if akRef
		akRef.BlockActivation(false, false)
	endif
EndFunction

; SetResetClutter: a loose item the scene knocked about, back where it lay
; when the scene started (angles in degrees).
Function PutBack(int aiRefID, float afX, float afY, float afZ, float afAngleX, float afAngleY, float afAngleZ) Global
	ObjectReference akRef = Game.GetForm(aiRefID) as ObjectReference
	if akRef == None || akRef.IsDisabled() || !akRef.Is3DLoaded()
		return
	endif
	akRef.SetPosition(afX, afY, afZ)
	akRef.SetAngle(afAngleX, afAngleY, afAngleZ)
EndFunction

; ---- OStim NG's OFurniture (docs\API.md) ----

; The furniture around CenterRef a scene for ActorCount actors could be played
; on, nearest first (0 = any count). Radius 0 = fFurnitureRadius, SameFloor 0 =
; fFurnitureHeight (how far up or down it may be). Furniture in use or blocked
; is left out.
ObjectReference[] Function FindFurniture(int ActorCount, ObjectReference CenterRef, float Radius = 0.0, float SameFloor = 0.0) Global Native

; The nearest furniture of Type (or a kind of it) around CenterRef, or None.
ObjectReference Function FindFurnitureOfType(string Type, ObjectReference CenterRef, float Radius = 0.0, float SameFloor = 0.0) Global Native

string Function GetFurnitureType(ObjectReference FurnitureRef) Global
	return FourStim.GetFurnitureType(FurnitureRef)
EndFunction

; Whether SubType is SuperType or a kind of it.
bool Function IsChildOf(string SuperType, string SubType) Global
	return FourStim.IsFurnitureChildOf(SuperType, SubType)
EndFunction
