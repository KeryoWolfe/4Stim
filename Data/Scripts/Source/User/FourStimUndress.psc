Scriptname FourStimUndress Hidden

; Undressing for 4Stim scenes (docs\UNDRESS.md). The plugin decides when and
; what (it finds what each actor wears in which slots); these take the
; clothes off and put them back on. Vanilla Papyrus only.

; Takes the items aiItemIDs off aiActorID, afDelay seconds apart (0: all at
; once).
Function Strip(int aiActorID, int[] aiItemIDs, float afDelay = 0.0) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None || aiItemIDs == None
		return
	endif
	int i = 0
	while i < aiItemIDs.Length
		Form akItem = Game.GetForm(aiItemIDs[i])
		if akItem && akActor.IsEquipped(akItem)
			if i > 0 && afDelay > 0.0
				Utility.Wait(afDelay)
			endif
			akActor.UnequipItem(akItem, false, true)
		endif
		i += 1
	endwhile
EndFunction

; Puts the items aiItemIDs back on aiActorID, if they still have them,
; afDelay seconds apart. Redress animations (after a scene, NPCs): item i
; belongs to animation aiGroupOf[i] (-1 = none); each animation (an Idle,
; aiAnimIdles) plays in turn, its items go on afAnimDressAt seconds in, and
; it runs afAnimLengths seconds in all. Items without one go on last.
Function Redress(int aiActorID, int[] aiItemIDs, float afDelay = 0.0, int[] aiGroupOf = None, int[] aiAnimIdles = None, float[] afAnimLengths = None, float[] afAnimDressAt = None) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None || aiItemIDs == None
		return
	endif
	bool bAnimated = false
	if aiAnimIdles && aiAnimIdles.Length > 0 && aiGroupOf && aiGroupOf.Length == aiItemIDs.Length
		Utility.Wait(1.5)  ; let the scene end and the actor be let go first
		int g = 0
		while g < aiAnimIdles.Length
			Idle akIdle = Game.GetForm(aiAnimIdles[g]) as Idle
			bool bPlayed = akIdle && akActor.Is3DLoaded() && !akActor.IsDead() && akActor.PlayIdle(akIdle)
			if bPlayed
				bAnimated = true
				Utility.Wait(afAnimDressAt[g])
			endif
			int j = 0
			while j < aiItemIDs.Length
				if aiGroupOf[j] == g
					DressIn(akActor, aiItemIDs[j])
				endif
				j += 1
			endwhile
			if bPlayed && afAnimLengths[g] > afAnimDressAt[g]
				Utility.Wait(afAnimLengths[g] - afAnimDressAt[g])
			endif
			g += 1
		endwhile
		if bAnimated
			Idle akStop = Game.GetFormFromFile(0x000E9855, "Fallout4.esm") as Idle  ; IdleStop
			if akStop
				akActor.PlayIdle(akStop)
			endif
		endif
	endif
	int i = 0
	int n = 0
	while i < aiItemIDs.Length
		if !bAnimated || aiGroupOf == None || aiGroupOf.Length != aiItemIDs.Length || aiGroupOf[i] < 0
			if n > 0 && afDelay > 0.0
				Utility.Wait(afDelay)
			endif
			DressIn(akActor, aiItemIDs[i])
			n += 1
		endif
		i += 1
	endwhile
EndFunction

; Equips aiItemID on akActor if they still have it and it isn't on.
Function DressIn(Actor akActor, int aiItemID) Global
	Form akItem = Game.GetForm(aiItemID)
	if akItem && akActor.GetItemCount(akItem) > 0 && !akActor.IsEquipped(akItem)
		akActor.EquipItem(akItem, false, true)
	endif
EndFunction
