Scriptname FourStimLibrary Native Hidden
{OStim NG's OLibrary for 4Stim (docs\API.md): random scenes for actors, by
scene tags, actor tags and actions. Same names and arguments as OStim's; the
scenes are ones these actors can play (any role order that fits), not
transitions or scenes marked noRandomSelection. "" if none fits. OStim's
multi-actor "ForAll", action actor / target and combined variants aren't here
yet: FindRandomScene covers most of them.}

; Every scene ID.
string[] Function GetAllScenes() Global Native

; The one search the functions below use: FurnitureType ("" = off furniture),
; scene tags (any of / all of), action types (any of / all of), and actor tags
; (any of / all of) for the role at Position (-1 = any role). None or an empty
; array asks nothing.
string Function FindRandomScene(Actor[] Actors, string FurnitureType, string[] AnySceneTags, string[] AllSceneTags, string[] AnyActions, string[] AllActions, int Position, string[] AnyActorTags, string[] AllActorTags) Global Native

; "a,b,c" -> ["a", "b", "c"].
string[] Function SplitCSV(string CSV) Global Native

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
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, -1, Tags, empty)
EndFunction

string Function GetRandomSceneWithMultiActorTagForAnyCSV(Actor[] Actors, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, "", empty, empty, empty, empty, -1, SplitCSV(Tags), empty)
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
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, -1, Tags, empty)
EndFunction

string Function GetRandomFurnitureSceneWithMultiActorTagForAnyCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	string[] empty
	return FindRandomScene(Actors, FurnitureType, empty, empty, empty, empty, -1, SplitCSV(Tags), empty)
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

