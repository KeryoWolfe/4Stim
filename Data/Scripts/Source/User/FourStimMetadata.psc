Scriptname FourStimMetadata Native Hidden
{OStim NG's OMetadata for 4Stim (docs\API.md): what a scene is, by its ID.
The common functions, with OStim's names; the scene file format is
docs\SCENES.md.}

string Function GetName(string ID) Global Native
int Function GetActorCount(string ID) Global
	return FourStim.GetSceneActorCount(ID)
EndFunction
bool Function IsTransition(string ID) Global Native
; A transition's destination ("" for other scenes).
string Function GetDestination(string ID) Global Native
; The furniture type it's played on ("" = none).
string Function GetFurnitureType(string ID) Global Native
string Function GetModpack(string ID) Global Native
bool Function IsNoRandomSelection(string ID) Global Native

; Speeds, 0-based.
int Function GetMaxSpeed(string ID) Global Native
int Function GetDefaultSpeed(string ID) Global Native

; The scenes its navigations lead to.
string[] Function GetNavigations(string ID) Global Native
string Function GetAutoTransition(string ID, string Type) Global Native
string Function GetAutoTransitionForActor(string ID, int Position, string Type) Global Native

; Scene tags.
string[] Function GetSceneTags(string ID) Global Native
bool Function HasSceneTag(string ID, string Tag) Global
	return GetSceneTags(ID).Find(Tag) >= 0
EndFunction
bool Function HasAnySceneTag(string ID, string[] Tags) Global
	string[] have = GetSceneTags(ID)
	int i = 0
	while i < Tags.Length
		if have.Find(Tags[i]) >= 0
			return true
		endif
		i += 1
	endwhile
	return false
EndFunction
bool Function HasAnySceneTagCSV(string ID, string Tags) Global
	return HasAnySceneTag(ID, FourStimLibrary.SplitCSV(Tags))
EndFunction
bool Function HasAllSceneTags(string ID, string[] Tags) Global
	string[] have = GetSceneTags(ID)
	int i = 0
	while i < Tags.Length
		if have.Find(Tags[i]) < 0
			return false
		endif
		i += 1
	endwhile
	return true
EndFunction
bool Function HasAllSceneTagsCSV(string ID, string Tags) Global
	return HasAllSceneTags(ID, FourStimLibrary.SplitCSV(Tags))
EndFunction

; Actor tags of the role at Position.
string[] Function GetActorTags(string ID, int Position) Global
	return FourStim.GetSceneActorTags(ID, Position)
EndFunction
bool Function HasActorTag(string ID, int Position, string Tag) Global
	return FourStim.SceneActorHasTag(ID, Position, Tag)
EndFunction

; Actions (docs\ACTIONS.md).
int Function GetActionCount(string ID) Global
	return FourStim.GetSceneActionCount(ID)
EndFunction
bool Function HasAction(string ID, string Type) Global
	return FourStim.SceneHasAction(ID, Type)
EndFunction
bool Function HasActionTag(string ID, string Tag) Global
	return FourStim.SceneHasActionTag(ID, Tag)
EndFunction
; The index of the first action of Type (with these actor / target positions,
; -1 = any), or -1.
int Function FindAction(string ID, string Type) Global
	return FourStim.FindSceneAction(ID, Type)
EndFunction
int Function FindActionForActor(string ID, int Position, string Type) Global
	return FourStim.FindSceneAction(ID, Type, Position)
EndFunction
int Function FindActionForTarget(string ID, int Position, string Type) Global
	return FourStim.FindSceneAction(ID, Type, -1, Position)
EndFunction
string Function GetActionType(string ID, int Index) Global
	return FourStim.GetSceneActionType(ID, Index)
EndFunction
int Function GetActionActor(string ID, int Index) Global
	return FourStim.GetSceneActionRole(ID, Index, 0)
EndFunction
int Function GetActionTarget(string ID, int Index) Global
	return FourStim.GetSceneActionRole(ID, Index, 1)
EndFunction
int Function GetActionPerformer(string ID, int Index) Global
	return FourStim.GetSceneActionRole(ID, Index, 2)
EndFunction
