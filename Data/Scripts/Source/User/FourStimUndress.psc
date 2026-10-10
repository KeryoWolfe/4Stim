Scriptname FourStimUndress Hidden

; Undressing for 4Stim scenes (docs\UNDRESS.md). The plugin decides when and
; what (it finds what each actor wears in which slots); these take the
; clothes off and put them back on. Vanilla Papyrus only.

; Takes the items aiItemIDs off aiActorID.
Function Strip(int aiActorID, int[] aiItemIDs) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None || aiItemIDs == None
		return
	endif
	int i = 0
	while i < aiItemIDs.Length
		Form akItem = Game.GetForm(aiItemIDs[i])
		if akItem && akActor.IsEquipped(akItem)
			akActor.UnequipItem(akItem, false, true)
		endif
		i += 1
	endwhile
EndFunction

; Puts the items aiItemIDs back on aiActorID, if they still have them.
Function Redress(int aiActorID, int[] aiItemIDs) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None || aiItemIDs == None
		return
	endif
	int i = 0
	while i < aiItemIDs.Length
		Form akItem = Game.GetForm(aiItemIDs[i])
		if akItem && akActor.GetItemCount(akItem) > 0 && !akActor.IsEquipped(akItem)
			akActor.EquipItem(akItem, false, true)
		endif
		i += 1
	endwhile
EndFunction
