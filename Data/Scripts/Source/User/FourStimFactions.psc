Scriptname FourStimFactions Hidden

; OStim's API factions and stat lists (docs\API.md, "Factions, form lists and
; keywords"), in 4Stim.esl: the plugin works out what changes for an actor
; and calls this with form IDs (CommonLibF4 has no native faction calls).
; Ranks are kept by the game and saved with it, as any faction rank.
;
; In this order: leave aiRemove, join aiAdd (rank 0 if not in it, as
; AddToFaction), set aiSet to aiRanks, add 1 to aiIncrement (OStim's
; incrementFaction: rank 1 if not in it, no higher than 101), and put the
; actor in the form lists aiLists (if not there yet).
Function Apply(int aiActorID, int[] aiAdd, int[] aiSet, int[] aiRanks, int[] aiRemove, int[] aiIncrement, int[] aiLists) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	if akActor == None
		return
	endif
	int i = 0
	while i < aiRemove.Length
		Faction f = Game.GetForm(aiRemove[i]) as Faction
		if f
			akActor.RemoveFromFaction(f)
		endif
		i += 1
	endwhile
	i = 0
	while i < aiAdd.Length
		Faction f = Game.GetForm(aiAdd[i]) as Faction
		if f
			akActor.AddToFaction(f)
		endif
		i += 1
	endwhile
	i = 0
	while i < aiSet.Length && i < aiRanks.Length
		Faction f = Game.GetForm(aiSet[i]) as Faction
		if f
			akActor.SetFactionRank(f, aiRanks[i])
		endif
		i += 1
	endwhile
	i = 0
	while i < aiIncrement.Length
		Faction f = Game.GetForm(aiIncrement[i]) as Faction
		if f
			if akActor.IsInFaction(f)
				int rank = akActor.GetFactionRank(f)
				if rank <= 100
					akActor.SetFactionRank(f, rank + 1)
				endif
			else
				akActor.SetFactionRank(f, 1)
			endif
		endif
		i += 1
	endwhile
	i = 0
	while i < aiLists.Length
		FormList l = Game.GetForm(aiLists[i]) as FormList
		if l && !l.HasForm(akActor)
			l.AddForm(akActor)
		endif
		i += 1
	endwhile
EndFunction
