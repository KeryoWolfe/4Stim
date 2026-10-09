Scriptname FourStimPhysics Hidden
{4Stim's physics swaps (see docs/PHYSICS.md), through FO4 Faster HDT-SMP's
DynamicHDT. Called by the plugin with form IDs: actors passed from native
code arrive in Papyrus as the wrong type on this build. Compiling this
script needs DynamicHDT.psc (from FO4 Faster HDT-SMP's Scripts\Source).}

; Swaps aiActorID's physics file asFrom for asTo. The plugin passes each
; path spelled three ways (as given, with "Meshes\", without the prefix)
; and the actor's skin's armor addons (the body meshes). Returns:
;   1, 2 or 3  swapped by file name, with that spelling pair
;   4          swapped on the skin addon found using the file
;   0          the actor has no active physics at all (not loaded, or beyond FSMP's maxActiveActors)
;  -1          it has physics, but nothing using the file (the addons' files are in the Papyrus log)
;  -2          no such actor
int Function Swap(int aiActorID, String asFrom1, String asTo1, String asFrom2, String asTo2, String asFrom3, String asTo3, int[] aiSkinAddons) Global
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
	; By addon: whichever skin addon uses the file now (string == ignores case).
	int i = 0
	while i < aiSkinAddons.Length
		ArmorAddon akAddon = Game.GetForm(aiSkinAddons[i]) as ArmorAddon
		if akAddon
			String sNow = DynamicHDT.QueryCurrentPhysicsFile(akActor, akAddon, true)
			Debug.Trace("FourStimPhysics: " + akActor + " skin addon " + akAddon + " uses physics '" + sNow + "'")
			if sNow != "" && (sNow == asFrom1 || sNow == asFrom2 || sNow == asFrom3)
				if DynamicHDT.ReloadPhysicsFile(akActor, akAddon, asTo2, true, true)
					return 4
				endif
			endif
		endif
		i += 1
	endwhile
	return -1
EndFunction
