Scriptname FourStimPhysics Hidden
{4Stim's physics swaps (see docs/PHYSICS.md), through FO4 Faster HDT-SMP's
DynamicHDT. Called by the plugin with a form ID: actors passed from native
code arrive in Papyrus as the wrong type on this build. Compiling this
script needs DynamicHDT.psc (from FO4 Faster HDT-SMP's Scripts\Source).}

; Swaps aiActorID's physics file asFrom for asTo, trying each spelling pair
; in turn (the plugin passes the same paths written the ways meshes name
; them). Returns:
;   1, 2 or 3  swapped, with that spelling pair
;   0          the actor has no active physics at all (not loaded, or beyond FSMP's maxActiveActors)
;  -1          it has physics, but none using the file under any spelling
;  -2          no such actor
int Function Swap(int aiActorID, String asFrom1, String asTo1, String asFrom2, String asTo2, String asFrom3, String asTo3) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None
		return -2
	endif
	if !DynamicHDT.HasPhysicsSystem(akActor)
		return 0
	endif
	if DynamicHDT.SwapPhysicsFile(akActor, asFrom1, asTo1, true, true)
		return 1
	endif
	if DynamicHDT.SwapPhysicsFile(akActor, asFrom2, asTo2, true, true)
		return 2
	endif
	if DynamicHDT.SwapPhysicsFile(akActor, asFrom3, asTo3, true, true)
		return 3
	endif
	return -1
EndFunction

; A scene ended: afDelay seconds later (out of the scene's pose, in their own
; idle), the swap undone, then the physics rebuilt from the reference pose.
; Undoing it while the body was still in the scene's pose could rebuild the
; physics around that pose (a penis left stretched, stuck at an angle).
; Returns what Swap returns, or -4 if they're in a new scene by then (the
; swap stays: it's that scene's now).
int Function SwapAfterScene(int aiActorID, String asFrom1, String asTo1, String asFrom2, String asTo2, String asFrom3, String asTo3, float afDelay) Global
	Utility.Wait(afDelay)
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor && FourStim.IsInScene(akActor)
		return -4
	endif
	int result = Swap(aiActorID, asFrom1, asTo1, asFrom2, asTo2, asFrom3, asTo3)
	Utility.Wait(0.2)
	ResetNow(aiActorID)
	return result
EndFunction

; A scene ended for an actor without a swap: afDelay seconds later, their
; physics rebuilt from the reference pose all the same.
Function ResetAfterScene(int aiActorID, float afDelay) Global
	Utility.Wait(afDelay)
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor && !FourStim.IsInScene(akActor)
		ResetNow(aiActorID)
	endif
EndFunction

Function ResetNow(int aiActorID) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor && DynamicHDT.HasPhysicsSystem(akActor)
		DynamicHDT.ResetPhysics(akActor, true)  ; full: from the reference pose
	endif
EndFunction
