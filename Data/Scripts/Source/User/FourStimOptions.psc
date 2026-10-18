Scriptname FourStimOptions Hidden

; The functions behind 4Stim's own HUD options (Data\F4SE\Plugins\4Stim\
; Options\4Stim.json, docs\OPTIONS.md). As OStim's options call theirs:
; (state), (state, actor) or (state, actor, object id).

; How the plugin calls an option's function, 4Stim's or another mod's: an
; Actor passed from the plugin doesn't arrive in Papyrus on this build, so it
; passes the form ID, and this calls asScript.asFunction with OStim's
; arguments: the state, then the actor (aiArgCount 2), then the object id (3).
Function Run(String asScript, String asFunction, String asState, int aiActorID, String asObject, int aiArgCount) Global
	Var[] args = new Var[0]
	args.Add(asState)
	if aiArgCount > 1
		args.Add(Game.GetForm(aiActorID) as Actor)
	endif
	if aiArgCount > 2
		args.Add(asObject)
	endif
	Utility.CallGlobalFunction(asScript, asFunction, args)
EndFunction

; Mute / unmute ("on" = muted now: unmute).
Function ToggleMute(String asState, Actor akActor) Global
	if asState == "on"
		FourStim.Unmute(akActor)
	else
		FourStim.Mute(akActor)
	endif
EndFunction

; Undress / dress ("on" = undressed now: dress).
Function ToggleUndress(String asState, Actor akActor) Global
	if asState == "on"
		FourStim.RedressActor(akActor)
	else
		FourStim.UndressActor(akActor)
	endif
EndFunction

; An equip object type's "toggle" (on / off) and "object" (wear this one).
Function ToggleStrapOn(String asState, Actor akActor) Global
	ToggleObject(akActor, "strapon")
EndFunction

Function ChooseStrapOn(String asState, Actor akActor, String asID) Global
	FourStim.SetEquipObjectID(akActor, "strapon", asID)
EndFunction

Function ToggleTongue(String asState, Actor akActor) Global
	ToggleObject(akActor, "tongue")
EndFunction

Function ChooseTongue(String asState, Actor akActor, String asID) Global
	FourStim.SetEquipObjectID(akActor, "tongue", asID)
EndFunction

Function ToggleObject(Actor akActor, String asType) Global
	if FourStim.IsObjectEquipped(akActor, asType)
		FourStim.UnequipObject(akActor, asType)
	else
		FourStim.EquipObject(akActor, asType)
	endif
EndFunction
