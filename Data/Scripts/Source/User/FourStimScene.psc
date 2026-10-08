Scriptname FourStimScene Hidden

; High-level scene setup/teardown. Built on the low-level natives in
; FourStim; the step ORDER and WAITS here are load-bearing -- each was
; found by reproducing a real failure:
;
;   - Suppress interaction before moving: an automatic greet can fire
;     within ~1s of an NPC arriving near the player.
;   - Unequip weapons: a drawn weapon layers its arm pose over the scene.
;   - Position before restraining: transform changes are ignored once an
;     actor is locked down.
;   - Restrain right after positioning: otherwise the actor's AI walks or
;     turns them off the mark before the animation starts.
;   - Never EnableAI(false): it stops the actor's animation updates, which
;     freezes real animations on their first frame (static poses hid this).
;   - Pairs: collision between them off BEFORE placing them on one spot,
;     back on the moment the poses clear.
;
; The player can take part in any role. The player is NOT restrained (that
; freezes look/move input); their controls are limited by a 4Stim input layer (menus, favorites, combat,
; activate, jumping etc. blocked; looking kept; movement only in the free
; camera), and is never moved: a scene with the player is built on the
; player's own spot. (SetPlayerAIDriven is NOT used: it also blocks the
; look/move input the free camera needs.)
;
; Camera: scenes with the player hide the vanilla HUD and switch to the
; third-person free camera once the animation starts (speed set by
; fFreeCameraSpeed in 4Stim.ini). NPC-only scenes do the
; same if abFreeCamera is true (for scenes the player starts and wants to
; watch). The camera and HUD are restored first thing when the scene ends.

bool Function IsPlayer(Actor akActor) Global
	return akActor == Game.GetPlayer()
EndFunction

; Returns true if akActor can be used in a scene right now. abQuiet skips
; the log lines, for scanning many candidates.
bool Function CanUseActor(Actor akActor, bool abQuiet = false) Global
	string sReason = ""
	if akActor == None
		sReason = "no actor given"
	elseif akActor.IsChild()
		; Hard rule, not a temporary limitation: children are never eligible.
		sReason = "children are not eligible"
	elseif akActor.IsDead()
		sReason = "actor is dead"
	elseif akActor.IsInCombat()
		sReason = "actor is in combat"
	endif
	if sReason != ""
		if !abQuiet
			Debug.Trace("FourStimScene: " + sReason)
		endif
		return false
	endif
	return true
EndFunction

; Unequips the actor's weapon, if any. abPreventEquip is left false so
; NPCs re-arm on their own once released.
Function HolsterForScene(Actor akActor) Global
	Weapon akWeapon = akActor.GetEquippedWeapon()
	if akWeapon
		akActor.UnequipItem(akWeapon, false, true)
	endif
EndFunction

Function PrepareActor(Actor akActor) Global
	if !IsPlayer(akActor)
		FourStim.SuppressInteraction(akActor)
	endif
	HolsterForScene(akActor)
EndFunction

Function LockForScene(Actor akActor) Global
	if IsPlayer(akActor)
		; No SetRestrained on the player: it also freezes look/move input,
		; which the free camera and first-person look need. The input layer
		; already keeps the player from walking off.
		FourStim.LockPlayerControls()
	else
		akActor.SetRestrained(true)
	endif
EndFunction

Function ReleaseFromScene(Actor akActor) Global
	if IsPlayer(akActor)
		FourStim.UnlockPlayerControls()
	else
		akActor.SetRestrained(false)
		akActor.EvaluatePackage()  ; walk back into their routine
	endif
EndFunction

; Forces the game's full third-person view at scene start -- the proper
; view change, which also swaps the first-person arms for the player's
; body -- after noting the starting view so the scene can return to it.
Function PrepareSceneCamera() Global
	FourStim.SaveStartView()
	Game.ForceThirdPerson()
	Utility.Wait(0.5)  ; let the view change finish
EndFunction

Function RestoreActorInteraction(Actor akActor) Global
	if !IsPlayer(akActor)
		FourStim.RestoreInteraction(akActor)
	endif
EndFunction

; ---- Single-actor scenes ----

; Poses akTarget with role 0 of asSceneID. An NPC target is first moved
; afDistance from akAnchor (offset afAngleOffset degrees from its facing,
; turned to face it); the player is posed where they stand (akAnchor may
; be None).
bool Function BeginScene(Actor akTarget, Actor akAnchor, Float afDistance, Float afAngleOffset, String asSceneID, bool abFreeCamera = false) Global
	if !CanUseActor(akTarget)
		return false
	endif
	if FourStim.GetSceneActorCount(asSceneID) < 1
		Debug.Trace("FourStimScene: no scene '" + asSceneID + "' loaded (see 4Stim.log)")
		return false
	endif
	bool bMove = !IsPlayer(akTarget)
	if bMove && akAnchor == None
		Debug.Trace("FourStimScene: no anchor given")
		return false
	endif

	if abFreeCamera || IsPlayer(akTarget)
		PrepareSceneCamera()
	endif
	PrepareActor(akTarget)
	if bMove
		FourStim.MoveActorTo(akTarget, akAnchor, afDistance, afAngleOffset)
		Utility.Wait(0.1)  ; let the queued SetPosition/SetAngle resolve
	endif
	LockForScene(akTarget)
	FourStim.StartScene(akTarget, None, asSceneID)
	if abFreeCamera || IsPlayer(akTarget)
		FourStim.BeginSceneCamera()
	endif
	return true
EndFunction

Function EndScene(Actor akTarget) Global
	if akTarget == None
		return
	endif
	FourStim.EndSceneCamera()  ; no-op if the scene camera wasn't used
	if !IsPlayer(akTarget)
		akTarget.EnableAI(true)  ; safety net for NPCs frozen by older builds
	endif
	FourStim.StopScene(akTarget)
	Utility.Wait(0.5)
	ReleaseFromScene(akTarget)
	Utility.Wait(1.0)
	RestoreActorInteraction(akTarget)
EndFunction

; ---- Two-actor scenes ----

; akActor0/akActor1 play roles 0/1 of asSceneID, together on one spot:
; afDistance in front of akAnchor, heading = akAnchor's + afHeadingOffset
; (180 = facing the anchor). If the player is one of the two, the scene is
; built on the player's own spot and facing instead, ignoring the
; anchor/distance/offset arguments.
bool Function BeginPairScene(Actor akActor0, Actor akActor1, String asSceneID, Actor akAnchor, Float afDistance, Float afHeadingOffset, bool abFreeCamera = false) Global
	if !CanUseActor(akActor0) || !CanUseActor(akActor1)
		return false
	endif
	if akActor0 == akActor1
		Debug.Trace("FourStimScene: both roles given the same actor")
		return false
	endif
	if FourStim.GetSceneActorCount(asSceneID) < 2
		Debug.Trace("FourStimScene: no two-actor scene '" + asSceneID + "' loaded (see 4Stim.log)")
		return false
	endif
	bool bWithPlayer = IsPlayer(akActor0) || IsPlayer(akActor1)
	if bWithPlayer
		akAnchor = Game.GetPlayer()
		afDistance = 0.0
		afHeadingOffset = 0.0
	endif
	if akAnchor == None
		Debug.Trace("FourStimScene: no anchor given")
		return false
	endif

	if abFreeCamera || bWithPlayer
		PrepareSceneCamera()
	endif
	PrepareActor(akActor0)
	PrepareActor(akActor1)

	; Before placing them on one spot, or their capsules push them apart.
	FourStim.IgnorePairCollision(akActor0, akActor1)
	Utility.Wait(0.1)

	FourStim.PlacePair(akActor0, akActor1, akAnchor, afDistance, afHeadingOffset)
	Utility.Wait(0.1)

	LockForScene(akActor0)
	LockForScene(akActor1)

	FourStim.PlaySceneIdles(akActor0, akActor1, asSceneID)
	if abFreeCamera || bWithPlayer
		FourStim.BeginSceneCamera()
	endif
	return true
EndFunction

; ---- Sequences ----

; Like BeginScene, but plays sequence asSequenceID: its scenes one after
; another for their set times, then stays on the last one.
bool Function BeginSequence(Actor akTarget, Actor akAnchor, Float afDistance, Float afAngleOffset, String asSequenceID, bool abFreeCamera = false) Global
	String sFirst = FourStim.QueueSequence(asSequenceID, akTarget, None)
	if sFirst == ""
		Debug.Trace("FourStimScene: no one-actor sequence '" + asSequenceID + "' loaded (see 4Stim.log)")
		return false
	endif
	return BeginScene(akTarget, akAnchor, afDistance, afAngleOffset, sFirst, abFreeCamera)
EndFunction

; Like BeginPairScene, but plays sequence asSequenceID.
bool Function BeginPairSequence(Actor akActor0, Actor akActor1, String asSequenceID, Actor akAnchor, Float afDistance, Float afHeadingOffset, bool abFreeCamera = false) Global
	String sFirst = FourStim.QueueSequence(asSequenceID, akActor0, akActor1)
	if sFirst == ""
		Debug.Trace("FourStimScene: no two-actor sequence '" + asSequenceID + "' loaded (see 4Stim.log)")
		return false
	endif
	return BeginPairScene(akActor0, akActor1, sFirst, akAnchor, afDistance, afHeadingOffset, abFreeCamera)
EndFunction

Function EndPairScene(Actor akActor0, Actor akActor1) Global
	; Every step applied to both actors together, so they release at once.
	FourStim.EndSceneCamera()  ; no-op if the scene camera wasn't used
	FourStim.StopPair(akActor0, akActor1)
	; Collision back immediately: once the poses clear, both bodies snap
	; onto their shared spot, and waiting leaves them visibly melded.
	FourStim.RestoreCollision(akActor0)
	FourStim.RestoreCollision(akActor1)
	Utility.Wait(0.5)

	ReleaseFromScene(akActor0)
	ReleaseFromScene(akActor1)

	Utility.Wait(1.0)
	RestoreActorInteraction(akActor0)
	RestoreActorInteraction(akActor1)
EndFunction
