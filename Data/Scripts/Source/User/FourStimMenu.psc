Scriptname FourStimMenu Hidden

; Hotkey and scene picker glue. Settings come from
; Data\F4SE\Plugins\4Stim.ini.
;
; Hotkey: if the player is in a scene, open its navigation (other scenes
; to move to, plus "End scene"). Otherwise find a partner (crosshair or
; proximity) and open the scene picker, which walks through who's in the
; scene (that partner already picked), where, and starts an idle scene.
; The plugin calls StartPickedCast with the cast, and EndPlayerScene when
; "End scene" is picked.

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

; Ends a scene the player isn't watching (by its actors' form IDs, in
; role order), leaving the camera alone. Called by the plugin when an NPC
; scene ends after a climax.
Function EndSceneOf(int[] aiActorIDs) Global
	Actor[] akActors = new Actor[aiActorIDs.Length]
	int i = 0
	while i < aiActorIDs.Length
		akActors[i] = Game.GetForm(aiActorIDs[i]) as Actor
		i += 1
	endwhile
	FourStimScene.EndGroupScene(akActors, false)
EndFunction

; A climax the player sees: the parts done through Papyrus. The plugin
; shakes the free camera itself (the game's shake doesn't move it) and does
; the blur and glow; this shakes a normal camera (afShake > 0) and rumbles.
Function ClimaxEffects(float afShake, bool abRumble) Global
	if afShake > 0.0
		Game.ShakeCamera(None, 0.6 * afShake, 1.2)
	endif
	if abRumble
		Game.ShakeController(0.6, 0.6, 0.8)
	endif
EndFunction

; Ends the scene the player is in. Called by the plugin when "End scene"
; is picked in the navigation menu.
Function EndPlayerScene() Global
	int count = FourStim.GetPlayerSceneActorCount()
	if count == 0
		return
	endif
	Actor akActor0 = Game.GetForm(FourStim.GetPlayerSceneActorID(0)) as Actor
	if count == 1
		FourStimScene.EndScene(akActor0)
	elseif count == 2
		FourStimScene.EndPairScene(akActor0, Game.GetForm(FourStim.GetPlayerSceneActorID(1)) as Actor)
	else
		Actor[] akActors = new Actor[count]
		int i = 0
		while i < count
			akActors[i] = Game.GetForm(FourStim.GetPlayerSceneActorID(i)) as Actor
			i += 1
		endwhile
		FourStimScene.EndGroupScene(akActors)
	endif
EndFunction

; Called by the plugin when a new scene starts from the picker: the cast's
; form IDs already in role order (the player among them).
Function StartPickedCast(String asSceneID, int[] aiActorIDs) Global
	Actor akPlayer = Game.GetPlayer()
	int count = aiActorIDs.Length
	Actor[] akActors = new Actor[count]
	int i = 0
	while i < count
		akActors[i] = Game.GetForm(aiActorIDs[i]) as Actor
		if akActors[i] == None
			Debug.Notification("4Stim: an actor for the scene is gone")
			return
		endif
		i += 1
	endwhile

	bool bStarted
	if count == 1
		bStarted = FourStimScene.BeginScene(akActors[0], None, 0.0, 0.0, asSceneID)
	elseif count == 2
		bStarted = FourStimScene.BeginPairScene(akActors[0], akActors[1], asSceneID, akPlayer, 0.0, 0.0)
	else
		bStarted = FourStimScene.BeginGroupScene(akActors, asSceneID)
	endif
	if !bStarted
		Debug.Notification("4Stim: scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

; Older entry point (one partner), kept for anything still calling it.
Function StartPickedScene(String asSceneID, int aiTargetID, bool abTargetFirst = false) Global
	int[] ids
	if aiTargetID == 0
		ids = new int[1]
		ids[0] = Game.GetPlayer().GetFormID()
	else
		ids = new int[2]
		ids[0] = Game.GetPlayer().GetFormID()
		ids[1] = aiTargetID
		if abTargetFirst
			ids[0] = aiTargetID
			ids[1] = Game.GetPlayer().GetFormID()
		endif
	endif
	StartPickedCast(asSceneID, ids)
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
