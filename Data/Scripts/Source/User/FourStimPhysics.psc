Scriptname FourStimPhysics Hidden
{4Stim's physics swaps (see docs/PHYSICS.md), through FO4 Faster HDT-SMP's
DynamicHDT. Called by the plugin with a form ID: actors passed from native
code arrive in Papyrus as the wrong type on this build. Compiling this
script needs DynamicHDT.psc (from FO4 Faster HDT-SMP's Scripts\Source).}

; Swaps aiActorID's physics file asFrom for asTo. True if FSMP did.
Bool Function Swap(int aiActorID, String asFrom, String asTo) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None
		Debug.Trace("FourStimPhysics: no actor " + aiActorID)
		return false
	endif
	return DynamicHDT.SwapPhysicsFile(akActor, asFrom, asTo, true, true)
EndFunction
