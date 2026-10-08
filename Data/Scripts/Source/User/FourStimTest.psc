Scriptname FourStimTest extends Quest

; Console tests. Click an NPC with the console open to select them, then:
;
;   cgf "FourStimTest.Begin"                    solo pose (k905_001) on them
;   cgf "FourStimTest.BeginWith" "k905_005"     solo, any single-actor scene
;   cgf "FourStimTest.End"
;
;   cgf "FourStimTest.BeginSelf"                solo pose on the player
;   cgf "FourStimTest.EndSelf"
;
; Pairs: the console-selected actor takes role 1; pass role 0 by ref ID.
; Use "player" as the ref to take part yourself.
;   cgf "FourStimTest.BeginPair" <role-0 ref>
;   cgf "FourStimTest.BeginPairWith" <role-0 ref> "<scene id>"
;   cgf "FourStimTest.EndPair" <role-0 ref>     (role-1 actor still selected)
;   cgf "FourStimTest.BeginPairCam" <role-0 ref>  NPC-only pair, with free camera
;
;   cgf "FourStimTest.ReloadScenes"             re-read the scene JSON files
;
; Low-level pieces (console-selected actor):
;   Lock, Unlock, Suppress, Restore, Move, StopPose

; Returns the actor currently selected in the console, or None (with an
; on-screen hint) if nothing, or a non-actor, is selected.
Actor Function SelectedActor() Global
	Actor akActor = None
	int id = FourStim.GetSelectedActorID()
	if id != 0
		akActor = Game.GetForm(id) as Actor
	endif
	if akActor == None
		Debug.Notification("4Stim: click an NPC in the console first")
	endif
	return akActor
EndFunction

Function Begin() Global
	BeginWith("k905_001")
EndFunction

Function BeginWith(String asSceneID) Global
	Actor akTarget = SelectedActor()
	if akTarget && !FourStimScene.BeginScene(akTarget, Game.GetPlayer(), 80.0, 0.0, asSceneID)
		Debug.Notification("4Stim: scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

Function End() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStimScene.EndScene(akTarget)
	endif
EndFunction

Function BeginSelf() Global
	if !FourStimScene.BeginScene(Game.GetPlayer(), None, 0.0, 0.0, "k905_001")
		Debug.Notification("4Stim: scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

Function EndSelf() Global
	FourStimScene.EndScene(Game.GetPlayer())
EndFunction

Function BeginPair(Actor akActor0) Global
	BeginPairWith(akActor0, "4StimTest_OStim2PStandingBehindFuckMF")
EndFunction

Function BeginPairWith(Actor akActor0, String asSceneID) Global
	Actor akActor1 = SelectedActor()
	if akActor1 && !FourStimScene.BeginPairScene(akActor0, akActor1, asSceneID, Game.GetPlayer(), 100.0, 180.0)
		Debug.Notification("4Stim: pair scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

Function BeginPairCam(Actor akActor0) Global
	Actor akActor1 = SelectedActor()
	if akActor1 && !FourStimScene.BeginPairScene(akActor0, akActor1, "4StimTest_OStim2PStandingBehindFuckMF", Game.GetPlayer(), 100.0, 180.0, true)
		Debug.Notification("4Stim: pair scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

Function EndPair(Actor akActor0) Global
	Actor akActor1 = SelectedActor()
	if akActor1
		FourStimScene.EndPairScene(akActor0, akActor1)
	endif
EndFunction

Function ReloadScenes() Global
	int n = FourStim.ReloadScenes()
	Debug.Notification("4Stim: loaded " + n + " scene(s)")
EndFunction

Function Lock() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStim.LockActor(akTarget)
	endif
EndFunction

Function Unlock() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStim.UnlockActor(akTarget)
	endif
EndFunction

Function Suppress() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStim.SuppressInteraction(akTarget)
	endif
EndFunction

Function Restore() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStim.RestoreInteraction(akTarget)
	endif
EndFunction

Function Move() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStim.MoveActorTo(akTarget, Game.GetPlayer(), 80.0, 0.0)
	endif
EndFunction

Function StopPose() Global
	Actor akTarget = SelectedActor()
	if akTarget
		FourStim.StopScene(akTarget)
	endif
EndFunction
