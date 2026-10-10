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
; afDelay seconds apart. With aiIdleID (an Idle), that idle plays first, for
; afIdleLength seconds (a scene just ended: "getting dressed").
Function Redress(int aiActorID, int[] aiItemIDs, float afDelay = 0.0, int aiIdleID = 0, float afIdleLength = 3.0) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None || aiItemIDs == None
		return
	endif
	Idle akIdle = None
	if aiIdleID != 0
		akIdle = Game.GetForm(aiIdleID) as Idle
	endif
	if akIdle
		Utility.Wait(0.5)  ; let the scene's pose clear first
		akActor.PlayIdle(akIdle)
		Utility.Wait(afIdleLength)
	endif
	int i = 0
	while i < aiItemIDs.Length
		Form akItem = Game.GetForm(aiItemIDs[i])
		if akItem && akActor.GetItemCount(akItem) > 0 && !akActor.IsEquipped(akItem)
			if i > 0 && afDelay > 0.0
				Utility.Wait(afDelay)
			endif
			akActor.EquipItem(akItem, false, true)
		endif
		i += 1
	endwhile
EndFunction
