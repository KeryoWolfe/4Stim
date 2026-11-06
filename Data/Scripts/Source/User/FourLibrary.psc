Scriptname FourLibrary Native Hidden
{OStim NG's OLibrary for 4Stim (docs\API.md): random scenes for actors, by
scene tags, actor tags and actions. Same names and arguments as OStim's; the
scenes are ones these actors can play (any role order that fits), not
transitions or scenes marked noRandomSelection. "" if none fits. All 97 of
OLibrary's functions, wrapping five natives (FindRandomScene and the four
below it).}

; Every scene ID.
string[] Function GetAllScenes() Global Native

; The one search the functions below use: FurnitureType ("" = off furniture),
; scene tags (any of / all of), action types (any of / all of), and actor tags
; (any of / all of) for the role at Position (-1 = any role). None or an empty
; array asks nothing.
string Function FindRandomScene(Actor[] Actors, string FurnitureType, string[] AnySceneTags, string[] AllSceneTags, string[] AnyActions, string[] AllActions, int Position, string[] AnyActorTags, string[] AllActorTags) Global Native

; "a,b,c" -> ["a", "b", "c"].
string[] Function SplitCSV(string CSV) Global Native

; OLibrary's multi-actor tag searches: ActorTags is one list per role ("a,b;c":
; role 0 any / all of a and b, role 1 c), each role needing any of its list
; (AllPerRole false) or all of it, for any role or for all of them (ForAll).
; SceneMode: 0 no scene tags, 1 any of SceneTags (CSV), 2 all of them.
string Function FindRandomSceneByRoleTags(Actor[] Actors, string FurnitureType, string ActorTags, bool AllPerRole, bool ForAll, string SceneTags, int SceneMode) Global Native

; OLibrary's action searches by role: an action of any of Types (or each of
; them, All) done by role ActorPosition (when ByActor) to role TargetPosition
; (when ByTarget).
string Function FindRandomSceneByActionRoles(Actor[] Actors, string FurnitureType, string[] Types, bool All, bool ByActor, int ActorPosition, bool ByTarget, int TargetPosition) Global Native

; GetRandomSceneSuperloadCSV's search: Filters holds its 44 strings after
; FurnitureType, in OStim's order.
string Function FindRandomSceneSuperload(Actor[] Actors, string FurnitureType, string[] Filters) Global Native

; The scenes reachable from Id in up to Distance navigations that these actors
; can play, sorted by name (OStim's getNodesInRange).
string[] Function GetScenesInRange(string Id, Actor[] Actors, int Distance = 0) Global Native

; One tag per role: ["a", "b"] -> "a;b", the role lists FindRandomSceneByRoleTags takes.
string Function RolesOf(string[] Tags) Global
	string out = ""
	int i = 0
	while i < Tags.Length
		if i > 0
			out += ";"
		endif
		out += Tags[i]
		i += 1
	endwhile
	return out
EndFunction

; One tag per role from a CSV: "a,b" -> "a;b".
string Function RolesOfCSV(string Tags) Global
	return RolesOf(SplitCSV(Tags))
EndFunction

; Wraps one value in an array.
string[] Function One(string Value) Global
	string[] list = new string[1]
	list[0] = Value
	return list
EndFunction

string Function GetRandomScene(Actor[] Actors) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureScene(Actor[] Actors, string FurnitureType) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithSceneTag(Actor[] Actors, string Tag) Global
	string[] empty
	return FindRandomScene(Actors, "", One(Tag), empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAnySceneTag(Actor[] Actors, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", Tags, empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAnySceneTagCSV(Actor[] Actors, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", SplitCSV(Tags), empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAllSceneTags(Actor[] Actors, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, Tags, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAllSceneTagsCSV(Actor[] Actors, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, SplitCSV(Tags), empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithSingleActorTag(Actor[] Actors, int Position, string Tag) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, Position, One(Tag), empty)
EndFunction

string Function GetRandomSceneWithAnySingleActorTag(Actor[] Actors, int Position, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, Position, Tags, empty)
EndFunction

string Function GetRandomSceneWithAnySingleActorTagCSV(Actor[] Actors, int Position, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, Position, SplitCSV(Tags), empty)
EndFunction

string Function GetRandomSceneWithAllSingleActorTags(Actor[] Actors, int Position, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, Position, empty, Tags)
EndFunction

string Function GetRandomSceneWithAllSingleActorTagsCSV(Actor[] Actors, int Position, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, Position, empty, SplitCSV(Tags))
EndFunction

string Function GetRandomSceneWithMultiActorTagForAny(Actor[] Actors, string[] Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", RolesOf(Tags), false, false, "", 0)
EndFunction

string Function GetRandomSceneWithMultiActorTagForAnyCSV(Actor[] Actors, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", RolesOfCSV(Tags), false, false, "", 0)
EndFunction

string Function GetRandomSceneWithAction(Actor[] Actors, string Type) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, One(Type), empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAnyAction(Actor[] Actors, string[] Types) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, Types, empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAnyActionCSV(Actor[] Actors, string Types) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, SplitCSV(Types), empty, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAllActions(Actor[] Actors, string[] Types) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, Types, -1, empty, empty)
EndFunction

string Function GetRandomSceneWithAllActionsCSV(Actor[] Actors, string Types) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, SplitCSV(Types), -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithSceneTag(Actor[] Actors, string FurnitureType, string Tag) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, One(Tag), empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAnySceneTag(Actor[] Actors, string FurnitureType, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, Tags, empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAnySceneTagCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, SplitCSV(Tags), empty, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAllSceneTags(Actor[] Actors, string FurnitureType, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, Tags, empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAllSceneTagsCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, SplitCSV(Tags), empty, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithSingleActorTag(Actor[] Actors, string FurnitureType, int Position, string Tag) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, Position, One(Tag), empty)
EndFunction

string Function GetRandomFurnitureSceneWithAnySingleActorTag(Actor[] Actors, string FurnitureType, int Position, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, Position, Tags, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAnySingleActorTagCSV(Actor[] Actors, string FurnitureType, int Position, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, Position, SplitCSV(Tags), empty)
EndFunction

string Function GetRandomFurnitureSceneWithAllSingleActorTags(Actor[] Actors, string FurnitureType, int Position, string[] Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, Position, empty, Tags)
EndFunction

string Function GetRandomFurnitureSceneWithAllSingleActorTagsCSV(Actor[] Actors, string FurnitureType, int Position, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, Position, empty, SplitCSV(Tags))
EndFunction

string Function GetRandomFurnitureSceneWithMultiActorTagForAny(Actor[] Actors, string FurnitureType, string[] Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, RolesOf(Tags), false, false, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithMultiActorTagForAnyCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, RolesOfCSV(Tags), false, false, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithAction(Actor[] Actors, string FurnitureType, string Type) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, One(Type), empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAnyAction(Actor[] Actors, string FurnitureType, string[] Types) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, Types, empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionCSV(Actor[] Actors, string FurnitureType, string Types) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, SplitCSV(Types), empty, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAllActions(Actor[] Actors, string FurnitureType, string[] Types) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, Types, -1, empty, empty)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsCSV(Actor[] Actors, string FurnitureType, string Types) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, SplitCSV(Types), -1, empty, empty)
EndFunction

; ---- Multi-actor tags (OStim's ForAll and Any / All variants) ----
string Function GetRandomSceneWithMultiActorTagForAll(Actor[] Actors, string[] Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", RolesOf(Tags), false, true, "", 0)
EndFunction

string Function GetRandomSceneWithMultiActorTagForAllCSV(Actor[] Actors, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", RolesOfCSV(Tags), false, true, "", 0)
EndFunction

string Function GetRandomSceneWithAnyMultiActorTagForAnyCSV(Actor[] Actors, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", Tags, false, false, "", 0)
EndFunction

string Function GetRandomSceneWithAnyMultiActorTagForAllCSV(Actor[] Actors, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", Tags, false, true, "", 0)
EndFunction

string Function GetRandomSceneWithAllMultiActorTagsForAnyCSV(Actor[] Actors, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", Tags, true, false, "", 0)
EndFunction

string Function GetRandomSceneWithAllMultiActorTagsForAllCSV(Actor[] Actors, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, "", Tags, true, true, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithMultiActorTagForAll(Actor[] Actors, string FurnitureType, string[] Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, RolesOf(Tags), false, true, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithMultiActorTagForAlLCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, RolesOfCSV(Tags), false, true, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithAnyMultiActorTagForAnyCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, Tags, false, false, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithAnyMultiActorTagForAllCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, Tags, false, true, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithAllMultiActorTagsForAnyCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, Tags, true, false, "", 0)
EndFunction

string Function GetRandomFurnitureSceneWithAllMultiActorTagsForAllCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, Tags, true, true, "", 0)
EndFunction

; ---- Scene tags and multi-actor tags together ----
string Function GetRandomSceneWithAnySceneTagAndAnyMultiActorTagForAnyCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, false, false, SceneTags, 1)
EndFunction

string Function GetRandomSceneWithAllSceneTagsAndAnyMultiActorTagForAnyCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, false, false, SceneTags, 2)
EndFunction

string Function GetRandomSceneWithAnySceneTagAndAnyMultiActorTagForAllCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, false, true, SceneTags, 1)
EndFunction

string Function GetRandomSceneWithAllSceneTagsAndAnyMultiActorTagForAllCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, false, true, SceneTags, 2)
EndFunction

string Function GetRandomSceneWithAnySceneTagAndAllMultiActorTagsForAnyCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, true, false, SceneTags, 1)
EndFunction

string Function GetRandomSceneWithAllSceneTagsAndAllMultiActorTagsForAnyCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, true, false, SceneTags, 2)
EndFunction

string Function GetRandomSceneWithAnySceneTagAndAllMultiActorTagsForAllCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, true, true, SceneTags, 1)
EndFunction

string Function GetRandomSceneWithAllSceneTagsAndAllMultiActorTagsForAllCSV(Actor[] Actors, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, "", ActorTags, true, true, SceneTags, 2)
EndFunction

string Function GetRandomFurnitureSceneWithAnySceneTagAndAnyMultiActorTagForAnyCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, false, false, SceneTags, 1)
EndFunction

string Function GetRandomFurnitureSceneWithAllSceneTagsAndAnyMultiActorTagForAnyCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, false, false, SceneTags, 2)
EndFunction

string Function GetRandomFurnitureSceneWithAnySceneTagAndAnyMultiActorTagForAllCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, false, true, SceneTags, 1)
EndFunction

string Function GetRandomFurnitureSceneWithAllSceneTagsAndAnyMultiActorTagForAllCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, false, true, SceneTags, 2)
EndFunction

string Function GetRandomFurnitureSceneWithAnySceneTagAndAllMultiActorTagsForAnyCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, true, false, SceneTags, 1)
EndFunction

string Function GetRandomFurnitureSceneWithAllSceneTagsAndAllMultiActorTagsForAnyCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, true, false, SceneTags, 2)
EndFunction

string Function GetRandomFurnitureSceneWithAnySceneTagAndAllMultiActorTagsForAllCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, true, true, SceneTags, 1)
EndFunction

string Function GetRandomFurnitureSceneWithAllSceneTagsAndAllMultiActorTagsForAllCSV(Actor[] Actors, string FurnitureType, string SceneTags, string ActorTags) Global
	return FindRandomSceneByRoleTags(Actors, FurnitureType, ActorTags, true, true, SceneTags, 2)
EndFunction

; ---- Actions by role ----
string Function GetRandomSceneWithActionForActor(Actor[] Actors, int Position, string Type) Global
	return FindRandomSceneByActionRoles(Actors, "", One(Type), false, true, Position, false, 0)
EndFunction

string Function GetRandomSceneWithAnyActionForActor(Actor[] Actors, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, "", Types, false, true, Position, false, 0)
EndFunction

string Function GetRandomSceneWithAnyActionForActorCSV(Actor[] Actors, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, "", SplitCSV(Types), false, true, Position, false, 0)
EndFunction

string Function GetRandomSceneWithAllActionsForActor(Actor[] Actors, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, "", Types, true, true, Position, false, 0)
EndFunction

string Function GetRandomSceneWithAllActionsForActorCSV(Actor[] Actors, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, "", SplitCSV(Types), true, true, Position, false, 0)
EndFunction

string Function GetRandomFurnitureSceneWithActionForActor(Actor[] Actors, string FurnitureType, int Position, string Type) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, One(Type), false, true, Position, false, 0)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionForActor(Actor[] Actors, string FurnitureType, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, Types, false, true, Position, false, 0)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionForActorCSV(Actor[] Actors, string FurnitureType, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, SplitCSV(Types), false, true, Position, false, 0)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsForActor(Actor[] Actors, string FurnitureType, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, Types, true, true, Position, false, 0)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsForActorCSV(Actor[] Actors, string FurnitureType, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, SplitCSV(Types), true, true, Position, false, 0)
EndFunction

string Function GetRandomSceneWithActionForTarget(Actor[] Actors, int Position, string Type) Global
	return FindRandomSceneByActionRoles(Actors, "", One(Type), false, false, 0, true, Position)
EndFunction

string Function GetRandomSceneWithAnyActionForTarget(Actor[] Actors, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, "", Types, false, false, 0, true, Position)
EndFunction

string Function GetRandomSceneWithAnyActionForTargetCSV(Actor[] Actors, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, "", SplitCSV(Types), false, false, 0, true, Position)
EndFunction

string Function GetRandomSceneWithAllActionsForTarget(Actor[] Actors, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, "", Types, true, false, 0, true, Position)
EndFunction

string Function GetRandomSceneWithAllActionsForTargetCSV(Actor[] Actors, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, "", SplitCSV(Types), true, false, 0, true, Position)
EndFunction

string Function GetRandomFurnitureSceneWithActionForTarget(Actor[] Actors, string FurnitureType, int Position, string Type) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, One(Type), false, false, 0, true, Position)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionForTarget(Actor[] Actors, string FurnitureType, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, Types, false, false, 0, true, Position)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionForTargetCSV(Actor[] Actors, string FurnitureType, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, SplitCSV(Types), false, false, 0, true, Position)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsForTarget(Actor[] Actors, string FurnitureType, int Position, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, Types, true, false, 0, true, Position)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsForTargetCSV(Actor[] Actors, string FurnitureType, int Position, string Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, SplitCSV(Types), true, false, 0, true, Position)
EndFunction

string Function GetRandomSceneWithActionForActorAndTarget(Actor[] Actors, int ActorPosition, int TargetPosition, string Type) Global
	return FindRandomSceneByActionRoles(Actors, "", One(Type), false, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomSceneWithAnyActionForActorAndTarget(Actor[] Actors, int ActorPosition, int TargetPosition, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, "", Types, false, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomSceneWithAnyActionForActorAndTargetCSV(Actor[] Actors, int ActorPosition, int TargetPosition, string Types) Global
	return FindRandomSceneByActionRoles(Actors, "", SplitCSV(Types), false, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomSceneWithAllActionsForActorAndTarget(Actor[] Actors, int ActorPosition, int TargetPosition, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, "", Types, true, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomSceneWithAllActionsForActorAndTargetCSV(Actor[] Actors, int ActorPosition, int TargetPosition, string Types) Global
	return FindRandomSceneByActionRoles(Actors, "", SplitCSV(Types), true, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomFurnitureSceneWithActionForActorAndTarget(Actor[] Actors, string FurnitureType, int ActorPosition, int TargetPosition, string Type) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, One(Type), false, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionForActorAndTarget(Actor[] Actors, string FurnitureType, int ActorPosition, int TargetPosition, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, Types, false, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomFurnitureSceneWithAnyActionForActorAndTargetCSV(Actor[] Actors, string FurnitureType, int ActorPosition, int TargetPosition, string Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, SplitCSV(Types), false, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsForActorAndTarget(Actor[] Actors, string FurnitureType, int ActorPosition, int TargetPosition, string[] Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, Types, true, true, ActorPosition, true, TargetPosition)
EndFunction

string Function GetRandomFurnitureSceneWithAllActionsForActorAndTargetCSV(Actor[] Actors, string FurnitureType, int ActorPosition, int TargetPosition, string Types) Global
	return FindRandomSceneByActionRoles(Actors, FurnitureType, SplitCSV(Types), true, true, ActorPosition, true, TargetPosition)
EndFunction

; ---- Superload (OStim's: every filter at once; see OStim's OLibrary.psc for the formats) ----
string Function GetRandomSceneSuperloadCSV(Actor[] Actors, string FurnitureType = "", string AnySceneTag = "", string AllSceneTags = "", string SceneTagWhitelist = "", string SceneTagBlacklist = "", string AnyActorTagForAny = "", string AnyActorTagForAll = "", string AllActorTagsForAny = "", string AllActorTagsForAll = "", string ActorTagWhitelistForAny = "", string ActorTagWhitelistForAll = "", string ActorTagBlacklistForAny = "", string ActorTagBlacklistForAll = "", string AnyActionType = "", string AnyActionActor = "", string AnyActionTarget = "", string AnyActionPerformer = "", string AnyActionMatesAny = "", string AnyActionMatesAll = "", string AnyActionParticipantAny = "", string AnyActionParticipantAll = "", string AllActionTypes = "", string AllActionActors = "", string AllActionTargets = "", string AllActionPerformers = "", string AllActionMatesAny = "", string AllActionMatesAll = "", string AllActionParticipantsAny = "", string AllActionParticipantsAll = "", string ActionWhitelistTypes = "", string ActionWhitelistActors = "", string ActionWhitelistTargets = "", string ActionWhitelistPerformers = "", string ActionWhitelistMatesAny = "", string ActionWhitelistMatesAll = "", string ActionWhitelistParticipantsAny = "", string ActionWhitelistParticipantsAll = "", string ActionBlacklistTypes = "", string ActionBlacklistActors = "", string ActionBlacklistTargets = "", string ActionBlacklistPerformers = "", string ActionBlacklistMatesAny = "", string ActionBlacklistMatesAll = "", string ActionBlacklistParticipantsAny = "", string ActionBlacklistParticipantsAll = "") Global
	string[] filters = new string[44]
	filters[0] = AnySceneTag
	filters[1] = AllSceneTags
	filters[2] = SceneTagWhitelist
	filters[3] = SceneTagBlacklist
	filters[4] = AnyActorTagForAny
	filters[5] = AnyActorTagForAll
	filters[6] = AllActorTagsForAny
	filters[7] = AllActorTagsForAll
	filters[8] = ActorTagWhitelistForAny
	filters[9] = ActorTagWhitelistForAll
	filters[10] = ActorTagBlacklistForAny
	filters[11] = ActorTagBlacklistForAll
	filters[12] = AnyActionType
	filters[13] = AnyActionActor
	filters[14] = AnyActionTarget
	filters[15] = AnyActionPerformer
	filters[16] = AnyActionMatesAny
	filters[17] = AnyActionMatesAll
	filters[18] = AnyActionParticipantAny
	filters[19] = AnyActionParticipantAll
	filters[20] = AllActionTypes
	filters[21] = AllActionActors
	filters[22] = AllActionTargets
	filters[23] = AllActionPerformers
	filters[24] = AllActionMatesAny
	filters[25] = AllActionMatesAll
	filters[26] = AllActionParticipantsAny
	filters[27] = AllActionParticipantsAll
	filters[28] = ActionWhitelistTypes
	filters[29] = ActionWhitelistActors
	filters[30] = ActionWhitelistTargets
	filters[31] = ActionWhitelistPerformers
	filters[32] = ActionWhitelistMatesAny
	filters[33] = ActionWhitelistMatesAll
	filters[34] = ActionWhitelistParticipantsAny
	filters[35] = ActionWhitelistParticipantsAll
	filters[36] = ActionBlacklistTypes
	filters[37] = ActionBlacklistActors
	filters[38] = ActionBlacklistTargets
	filters[39] = ActionBlacklistPerformers
	filters[40] = ActionBlacklistMatesAny
	filters[41] = ActionBlacklistMatesAll
	filters[42] = ActionBlacklistParticipantsAny
	filters[43] = ActionBlacklistParticipantsAll
	return FindRandomSceneSuperload(Actors, FurnitureType, filters)
EndFunction
