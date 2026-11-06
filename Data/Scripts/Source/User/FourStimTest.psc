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
; FourLibrary (OLibrary) searches, for the player and the console-selected NPC:
;   cgf "FourStimTest.Library"                  runs each kind of search and
;       checks what it found against the scene's own data (FourMetadata).
;       Results in a message box and the Papyrus log ("FourStimTest:").
;   cgf "FourStimTest.LibraryRange" "<scene id>" 2   GetScenesInRange
;
; FourMetadata (OMetadata) on a scene the player and the console-selected NPC can play:
;   cgf "FourStimTest.Metadata"                 checks each kind of lookup
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
	BeginPairWith(akActor0, "OStim2PStandingBehindFuckMF")
EndFunction

Function BeginPairWith(Actor akActor0, String asSceneID) Global
	Actor akActor1 = SelectedActor()
	if akActor1 && !FourStimScene.BeginPairScene(akActor0, akActor1, asSceneID, Game.GetPlayer(), 100.0, 180.0)
		Debug.Notification("4Stim: pair scene couldn't start (see Papyrus log / 4Stim.log)")
	endif
EndFunction

Function BeginPairCam(Actor akActor0) Global
	Actor akActor1 = SelectedActor()
	if akActor1 && !FourStimScene.BeginPairScene(akActor0, akActor1, "OStim2PStandingBehindFuckMF", Game.GetPlayer(), 100.0, 180.0, true)
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

; ---- FourLibrary ----
; Each check takes its tags and actions from a scene the actors can play
; (found with GetRandomScene), so there's always something to find: a search
; that comes back empty, or with a scene that lacks what it asked for, FAILs.
; "skip" means the sample scene had nothing of that kind to search for.

string Function LibraryLine(string asName, string asFound, bool abOK) Global
	string mark = "FAIL"
	if abOK
		mark = "ok"
	endif
	Debug.Trace("FourStimTest: " + mark + " " + asName + " -> '" + asFound + "'")
	return mark + " " + asName + ": " + asFound + "\n"
EndFunction

Actor[] Function LibraryActors() Global
	Actor akNPC = SelectedActor()
	if akNPC == None
		return None
	endif
	return FourActorUtil.ToArray(Game.GetPlayer(), akNPC)
EndFunction

Function Library() Global
	Actor[] actors = LibraryActors()
	if actors == None
		return
	endif
	string report = ""
	int fails = 0

	string sample = FourLibrary.GetRandomScene(actors)
	report += LibraryLine("GetRandomScene", sample, sample != "")
	if sample == ""
		Debug.MessageBox("4Stim library test: no scene fits the player and this NPC at all.\n" + report)
		return
	endif
	string[] sceneTags = FourMetadata.GetSceneTags(sample)
	string[] tags0 = FourMetadata.GetActorTags(sample, 0)
	string[] tags1 = FourMetadata.GetActorTags(sample, 1)
	string found
	bool ok

	; Multi-actor tags, one per role, for all roles (new)
	if tags0.Length > 0 && tags1.Length > 0
		found = FourLibrary.GetRandomSceneWithMultiActorTagForAllCSV(actors, tags0[0] + "," + tags1[0])
		ok = found != "" && FourMetadata.HasActorTag(found, 0, tags0[0]) && FourMetadata.HasActorTag(found, 1, tags1[0])
		report += LibraryLine("MultiActorTagForAll " + tags0[0] + "," + tags1[0], found, ok)
		fails += (!ok) as int

		; ...for any role (fixed: tag i for role i)
		found = FourLibrary.GetRandomSceneWithMultiActorTagForAnyCSV(actors, tags0[0] + "," + tags1[0])
		ok = found != "" && (FourMetadata.HasActorTag(found, 0, tags0[0]) || FourMetadata.HasActorTag(found, 1, tags1[0]))
		report += LibraryLine("MultiActorTagForAny", found, ok)
		fails += (!ok) as int

		; Every tag of both roles (lists per role)
		found = FourLibrary.GetRandomSceneWithAllMultiActorTagsForAllCSV(actors, Join(tags0) + ";" + Join(tags1))
		ok = found != "" && FourMetadata.HasActorTag(found, 0, tags0[tags0.Length - 1]) && FourMetadata.HasActorTag(found, 1, tags1[tags1.Length - 1])
		report += LibraryLine("AllMultiActorTagsForAll", found, ok)
		fails += (!ok) as int
	else
		report += "skip multi-actor tags: the sample scene's roles have no tags\n"
	endif

	; Scene tag and role tags together
	if sceneTags.Length > 0 && tags0.Length > 0
		found = FourLibrary.GetRandomSceneWithAllSceneTagsAndAnyMultiActorTagForAllCSV(actors, sceneTags[0], tags0[0])
		ok = found != "" && FourMetadata.HasSceneTag(found, sceneTags[0]) && FourMetadata.HasActorTag(found, 0, tags0[0])
		report += LibraryLine("AllSceneTagsAndAnyMultiActorTagForAll " + sceneTags[0] + " / " + tags0[0], found, ok)
		fails += (!ok) as int
	else
		report += "skip scene + role tags: the sample scene has no scene tags\n"
	endif

	; Actions by role
	if FourMetadata.GetActionCount(sample) > 0
		string actionType = FourMetadata.GetActionType(sample, 0)
		int doer = FourMetadata.GetActionActor(sample, 0)
		int target = FourMetadata.GetActionTarget(sample, 0)
		found = FourLibrary.GetRandomSceneWithActionForActor(actors, doer, actionType)
		ok = found != "" && FourMetadata.FindActionForActor(found, doer, actionType) >= 0
		report += LibraryLine("ActionForActor " + actionType + " by " + doer, found, ok)
		fails += (!ok) as int

		found = FourLibrary.GetRandomSceneWithActionForTarget(actors, target, actionType)
		ok = found != "" && FourMetadata.FindActionForTarget(found, target, actionType) >= 0
		report += LibraryLine("ActionForTarget " + actionType + " to " + target, found, ok)
		fails += (!ok) as int

		found = FourLibrary.GetRandomSceneWithActionForActorAndTarget(actors, doer, target, actionType)
		ok = found != "" && FourMetadata.FindActionForActor(found, doer, actionType) >= 0 && FourMetadata.FindActionForTarget(found, target, actionType) >= 0
		report += LibraryLine("ActionForActorAndTarget", found, ok)
		fails += (!ok) as int

		; Superload: the scene tag, both roles' first tags and the action at once
		string tagFilter = ""
		if sceneTags.Length > 0
			tagFilter = sceneTags[0]
		endif
		found = FourLibrary.GetRandomSceneSuperloadCSV(actors, "", "", tagFilter, "", "", "", "", "", "", "", "", "", "", actionType, doer as string)
		ok = found != "" && FourMetadata.FindActionForActor(found, doer, actionType) >= 0 && (tagFilter == "" || FourMetadata.HasSceneTag(found, tagFilter))
		report += LibraryLine("Superload (scene tag + action by role)", found, ok)
		fails += (!ok) as int

		; Superload blacklist: no scene with that action may come back
		found = FourLibrary.GetRandomSceneSuperloadCSV(actors, "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", actionType)
		ok = found == "" || !FourMetadata.HasAction(found, actionType)
		report += LibraryLine("Superload action blacklist " + actionType + " (empty is fine)", found, ok)
		fails += (!ok) as int
	else
		report += "skip actions: the sample scene has none\n"
	endif

	string[] near = FourLibrary.GetScenesInRange(sample, actors, 2)
	report += LibraryLine("GetScenesInRange from the sample, 2 steps: " + near.Length + " scene(s)", "", true)

	string verdict = "all passed"
	if fails > 0
		verdict = fails + " FAILED"
	endif
	Debug.MessageBox("4Stim library test: " + verdict + "\n\n" + report)
EndFunction

Function LibraryRange(string asSceneID, int aiDistance) Global
	Actor[] actors = LibraryActors()
	if actors == None
		return
	endif
	string[] ids = FourLibrary.GetScenesInRange(asSceneID, actors, aiDistance)
	string text = ""
	int i = 0
	while i < ids.Length && i < 20
		text += ids[i] + "\n"
		Debug.Trace("FourStimTest: in range: " + ids[i])
		i += 1
	endwhile
	Debug.MessageBox("4Stim: " + ids.Length + " scene(s) within " + aiDistance + " of " + asSceneID + "\n\n" + text)
EndFunction

string Function Join(string[] akList) Global
	string out = ""
	int i = 0
	while i < akList.Length
		if i > 0
			out += ","
		endif
		out += akList[i]
		i += 1
	endwhile
	return out
EndFunction

; ---- FourMetadata ----
; Lookups on a scene the actors can play, checked against each other.
Function Metadata() Global
	Actor[] actors = LibraryActors()
	if actors == None
		return
	endif
	string id = FourLibrary.GetRandomSceneWithAction(actors, "kissing")
	if id == ""
		id = FourLibrary.GetRandomScene(actors)
	endif
	string report = ""
	int fails = 0
	bool ok

	string[] names = FourMetadata.ScenesToNames(FourLibrary.One(id))
	ok = names.Length == 1 && names[0] == FourMetadata.GetName(id)
	report += LibraryLine("ScenesToNames", names[0], ok)
	fails += (!ok) as int

	report += LibraryLine("GetAnimationId speed 0 (empty for 'idles' scenes)", FourMetadata.GetAnimationId(id, 0), true)

	string[] types = FourMetadata.GetActionTypes(id)
	int[] doers = FourMetadata.GetActionActors(id)
	int[] targets = FourMetadata.GetActionTargets(id)
	int count = FourMetadata.GetActionCount(id)
	ok = types.Length == count && doers.Length == count && targets.Length == count
	report += LibraryLine("GetActionTypes / Actors / Targets: " + count + " action(s)", Join(types), ok)
	fails += (!ok) as int

	if count > 0
		string actionType = types[0]
		int doer = doers[0]
		int target = targets[0]
		int[] found = FourMetadata.FindActionsForActor(id, doer, actionType)
		ok = found.Find(0) >= 0
		report += LibraryLine("FindActionsForActor " + actionType + " by " + doer, found.Length + " found", ok)
		fails += (!ok) as int

		ok = FourMetadata.FindActionForActorAndTarget(id, doer, target, actionType) == 0
		report += LibraryLine("FindActionForActorAndTarget", "", ok)
		fails += (!ok) as int

		ok = FourMetadata.FindActionForMatesAllCSV(id, doer + "," + target, actionType) == 0
		report += LibraryLine("FindActionForMatesAllCSV", "", ok)
		fails += (!ok) as int

		ok = FourMetadata.FindActionForParticipant(id, target, actionType) >= 0
		report += LibraryLine("FindActionForParticipant", "", ok)
		fails += (!ok) as int

		ok = FourMetadata.FindActionsSuperloadCSVv2(id, doer as string, target as string, "", "", "", "", "", actionType).Find(0) >= 0
		report += LibraryLine("FindActionsSuperloadCSVv2 (actor, target, type)", "", ok)
		fails += (!ok) as int

		ok = FourMetadata.FindActionSuperloadCSVv2(id, "", "", "", "", "", "", "", "no_such_action_type") == -1
		report += LibraryLine("FindActionSuperloadCSVv2 with a type it lacks: -1", "", ok)
		fails += (!ok) as int

		string[] tags = FourMetadata.GetActionTags(id, 0)
		string[] allTags = FourMetadata.GetAllActionsTags(id)
		ok = FourMetadata.HasAllActionTagsOverAll(id, allTags) || allTags.Length == 0
		report += LibraryLine("GetAllActionsTags / HasAllActionTagsOverAll", Join(allTags), ok)
		fails += (!ok) as int
		if tags.Length > 0
			ok = FourMetadata.HasActionTag(id, 0, tags[0]) && FourMetadata.HasAllActionTagsOnAny(id, tags) && FourMetadata.GetActionTagOverlap(id, 0, tags).Length == tags.Length
			report += LibraryLine("HasActionTag / HasAllActionTagsOnAny / GetActionTagOverlap", tags[0], ok)
			fails += (!ok) as int
		endif
	endif

	string[] reqs = FourMetadata.GetActorRequirements(id, 0)
	if reqs.Length > 0
		ok = FourMetadata.HasRequirement(id, 0, reqs[0]) && FourMetadata.HasAllRequirements(id, 0, reqs)
		report += LibraryLine("HasRequirement / HasAllRequirements", Join(reqs), ok)
		fails += (!ok) as int
	endif

	string[] actorTags = FourMetadata.GetActorTags(id, 0)
	if actorTags.Length > 0
		ok = FourMetadata.HasAllActorTags(id, 0, actorTags) && FourMetadata.GetActorTagOverlap(id, 0, actorTags).Length == actorTags.Length
		report += LibraryLine("HasAllActorTags / GetActorTagOverlap", Join(actorTags), ok)
		fails += (!ok) as int
	endif

	string verdict = "all passed"
	if fails > 0
		verdict = fails + " FAILED"
	endif
	Debug.MessageBox("4Stim metadata test on " + id + ": " + verdict + "\n\n" + report)
EndFunction
