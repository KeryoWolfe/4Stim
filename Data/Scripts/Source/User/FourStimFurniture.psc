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
