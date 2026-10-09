Scriptname FourStimUndress Hidden

; Undressing for 4Stim scenes (docs\UNDRESS.md). The plugin decides when and
; which biped slots; these take the clothes off and put them back on, and
; tell the plugin what they took off. Needs F4SE (GetWornItem).

; Takes off what aiActorID wears in the slot indices aiSlots (F4SE's
; numbering: 0 = biped slot 30 ... 31 = slot 61).
Function Strip(int aiActorID, int[] aiSlots) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None || aiSlots == None
		return
	endif
	Form[] akRemoved = new Form[0]
	int i = 0
	while i < aiSlots.Length
		Actor:WornItem worn = akActor.GetWornItem(aiSlots[i])
		Armor akArmor = None
		if worn
			akArmor = worn.item as Armor
		endif
		; Only armor and clothes (never the skin or a weapon), each item once.
		if akArmor && akRemoved.Find(akArmor) < 0
			akRemoved.Add(akArmor)
			akActor.UnequipItem(akArmor, false, true)
		endif
		i += 1
	endwhile
	if akRemoved.Length > 0
		FourStim.NoteStripped(akActor, akRemoved)
	endif
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
