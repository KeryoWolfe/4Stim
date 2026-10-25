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
;  -6          none using asFrom, but one already uses asTo (a swap FSMP kept, e.g. in a save)
int Function Swap(int aiActorID, String asFrom1, String asTo1, String asFrom2, String asTo2, String asFrom3, String asTo3) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None
		return -2
	endif
	if !DynamicHDT.HasPhysicsSystem(akActor)
		return 0
	endif
	; verbose_log off: FSMP's verbose log for a swap is long, and written while
	; the game waits on the rebuild. An empty pair is skipped (the plugin
	; passes only the spelling that worked last time).
	if DynamicHDT.SwapPhysicsFile(akActor, asFrom1, asTo1, true, false)
		return 1
	endif
	if asFrom2 != "" && DynamicHDT.SwapPhysicsFile(akActor, asFrom2, asTo2, true, false)
		return 2
	endif
	if asFrom3 != "" && DynamicHDT.SwapPhysicsFile(akActor, asFrom3, asTo3, true, false)
		return 3
	endif
	; Already on asTo? (Papyrus compares strings without case.)
	String[] sFiles = CurrentFiles(akActor, false)
	int i = 0
	while i < sFiles.Length
		if sFiles[i] == asTo1 || (asTo2 != "" && sFiles[i] == asTo2) || (asTo3 != "" && sFiles[i] == asTo3)
			return -6
		endif
		i += 1
	endwhile
	return -1
EndFunction

; The physics file FSMP has on each worn piece of aiActorID (F4SE's
; GetWornItem, FSMP's QueryCurrentPhysicsFile): the live state, not what the
; meshes name.
String[] Function CurrentFiles(Actor akActor, bool abLabels) Global
	String[] sFiles = new String[0]
	ArmorAddon[] akDone = new ArmorAddon[0]
	int i = 0
	while i < 44
		Actor:WornItem worn = akActor.GetWornItem(i)
		ArmorAddon akAddon = None
		if worn
			akAddon = worn.model as ArmorAddon
		endif
		if akAddon && akDone.Find(akAddon) < 0
			akDone.Add(akAddon)
			String sFile = DynamicHDT.QueryCurrentPhysicsFile(akActor, akAddon, false)
			if sFile != "" && abLabels
				sFiles.Add("slot " + (i + 30) + ": " + sFile)
			elseif sFile != ""
				sFiles.Add(sFile)
			endif
		endif
		i += 1
	endwhile
	return sFiles
EndFunction

; For the plugin's log when a swap finds no piece using its file: what each
; worn piece uses, and whether FSMP has the physics soft suspended.
String Function Describe(int aiActorID) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None
		return "no such actor"
	endif
	String sOut = ""
	if DynamicHDT.IsSoftSuspended(akActor)
		sOut = "physics soft suspended; "
	endif
	String[] sFiles = CurrentFiles(akActor, true)
	if sFiles.Length == 0
		return sOut + "no worn piece has a physics file"
	endif
	int i = 0
	while i < sFiles.Length
		sOut += sFiles[i] + "; "
		i += 1
	endwhile
	return sOut
EndFunction

; A scene started: afDelay seconds later (the plugin staggers its actors, so
; FSMP's rebuilds don't all land in the scene's first frame with everything
; else), the swap, if they're still in a scene. Returns what Swap returns, or
; -5 if the scene was over by then.
int Function SwapLater(int aiActorID, String asFrom1, String asTo1, String asFrom2, String asTo2, String asFrom3, String asTo3, float afDelay) Global
	Utility.Wait(afDelay)
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor && !FourStim.IsInScene(akActor)
		return -5
	endif
	return Swap(aiActorID, asFrom1, asTo1, asFrom2, asTo2, asFrom3, asTo3)
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
	Utility.Wait(1.0)  ; the swap's rebuild done before the next one starts
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
