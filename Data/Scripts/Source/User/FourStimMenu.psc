Scriptname FourStimMenu Hidden

; Hotkey and scene picker glue. Settings come from
; Data\F4SE\Plugins\4Stim.ini.
;
; Hotkey: if the player is in a scene, open its navigation (other scenes
; to move to, plus "End scene"). Otherwise find a partner (crosshair or
; proximity) and open the scene picker: two-actor scenes with a partner,
; solo scenes without one. The plugin calls StartPickedScene with the
; player's choice, and EndPlayerScene when "End scene" is picked.

; aiTargetMode: 0 = Crosshair, 1 = Proximity.
Function OnHotkey(int aiTargetMode, float afMaxDistance, float afCone, float afRadius) Global
	; Already in a scene? Then the hotkey opens its navigation.
	if FourStim.GetPlayerSceneActorID(0) != 0
		FourStim.OpenSceneNavigation()
		return
	endif

	Actor akTarget = FindTarget(aiTargetMode, afMaxDistance, afCone, afRadius)
	int targetID = 0
	if akTarget
		targetID = akTarget.GetFormID()
	endif
	FourStim.OpenScenePicker(targetID)
EndFunction

; Ends the scene the player is in. Called by the plugin when "End scene"
; is picked in the navigation menu.
Function EndPlayerScene() Global
	int id0 = FourStim.GetPlayerSceneActorID(0)
	if id0 == 0
		return
	endif
	Actor akActor0 = Game.GetForm(id0) as Actor
	int id1 = FourStim.GetPlayerSceneActorID(1)
	if id1 != 0
		FourStimScene.EndPairScene(akActor0, Game.GetForm(id1) as Actor)
	else
		FourStimScene.EndScene(akActor0)
	endif
EndFunction

; Called by the plugin when the player picks a scene in the picker.
; aiTargetID is the partner's form ID, or 0 for a solo scene; with
; abTargetFirst, the partner takes role 0.
Function StartPickedScene(String asSceneID, int aiTargetID, bool abTargetFirst = false) Global
	Actor akPlayer = Game.GetPlayer()
	Actor akTarget = None
	if aiTargetID != 0
		akTarget = Game.GetForm(aiTargetID) as Actor
	endif

	bool bStarted
	if akTarget
		; The plugin works out who takes which role from the scene's "sex"
		; per role: the player first unless only the other order fits.
		Actor akRole0 = akPlayer
		Actor akRole1 = akTarget
		if abTargetFirst
			akRole0 = akTarget
			akRole1 = akPlayer
		endif
		bStarted = FourStimScene.BeginPairScene(akRole0, akRole1, asSceneID, akPlayer, 0.0, 0.0)
	else
		bStarted = FourStimScene.BeginScene(akPlayer, None, 0.0, 0.0, asSceneID)
	endif

	if !bStarted
		Debug.Notification("4Stim: scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

; Picks a scene partner near the player, or None.
;   Crosshair: the eligible actor closest to the direction the player
;     faces, within afCone degrees either side, up to afMaxDistance away.
;   Proximity: the nearest eligible actor within afRadius.
Actor Function FindTarget(int aiMode, float afMaxDistance, float afCone, float afRadius) Global
	Actor akPlayer = Game.GetPlayer()
	Keyword kwNPC = Game.GetFormFromFile(0x00013794, "Fallout4.esm") as Keyword  ; ActorTypeNPC
	if kwNPC == None
		Debug.Trace("FourStimMenu: ActorTypeNPC keyword not found")
		return None
	endif

	float fSearch = afMaxDistance
	if aiMode == 1
		fSearch = afRadius
	endif
	ObjectReference[] akRefs = akPlayer.FindAllReferencesWithKeyword(kwNPC, fSearch)

	Actor akBest = None
	float fBestScore = 1000000.0
	int i = 0
	while i < akRefs.Length
		Actor akCandidate = akRefs[i] as Actor
		if akCandidate && akCandidate != akPlayer && FourStimScene.CanUseActor(akCandidate, true)
			float fScore = -1.0
			if aiMode == 1
				fScore = akPlayer.GetDistance(akCandidate)
			else
				float fAngle = Math.Abs(akPlayer.GetHeadingAngle(akCandidate))
				if fAngle <= afCone
					fScore = fAngle
				endif
			endif
			if fScore >= 0.0 && fScore < fBestScore
				akBest = akCandidate
				fBestScore = fScore
			endif
		endif
		i += 1
	endwhile
	return akBest
EndFunction
