Scriptname FourStimErection Hidden

; Erections on bodies with an erection morph (docs\ERECTION.md), through
; LooksMenu's BodyGen. The plugin only calls these when LooksMenu's BodyGen
; script is installed; they're in a script of their own so that nothing
; else of 4Stim's needs LooksMenu. Compiling needs LooksMenu's BodyGen.psc
; (it comes with LooksMenu, in Scripts\Source\User).

; Sets the three morphs on aiActorID under the keyword aiKeywordID (4Stim's
; own, so removing them leaves the actor's other morphs alone) and shows
; them. A morph with no name is skipped.
Function SetMorphs(int aiActorID, int aiKeywordID, string asErection, float afErection, string asUp, float afUp, string asDown, float afDown) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	Keyword akKeyword = Game.GetForm(aiKeywordID) as Keyword
	if akActor == None || akKeyword == None
		return
	endif
	bool bFemale = IsFemale(akActor)
	if asErection != ""
		BodyGen.SetMorph(akActor, bFemale, asErection, akKeyword, afErection)
	endif
	if asUp != ""
		BodyGen.SetMorph(akActor, bFemale, asUp, akKeyword, afUp)
	endif
	if asDown != ""
		BodyGen.SetMorph(akActor, bFemale, asDown, akKeyword, afDown)
	endif
	BodyGen.UpdateMorphs(akActor)
EndFunction

; Removes every morph 4Stim set on aiActorID (those under aiKeywordID) and
; shows the body without them.
Function ClearMorphs(int aiActorID, int aiKeywordID) Global
	Actor akActor = Game.GetForm(aiActorID) as Actor
	Keyword akKeyword = Game.GetForm(aiKeywordID) as Keyword
	if akActor == None || akKeyword == None
		return
	endif
	BodyGen.RemoveMorphsByKeyword(akActor, IsFemale(akActor), akKeyword)
	BodyGen.UpdateMorphs(akActor)
EndFunction

bool Function IsFemale(Actor akActor) Global
	ActorBase akBase = akActor.GetLeveledActorBase()
	return akBase && akBase.GetSex() == 1
EndFunction
