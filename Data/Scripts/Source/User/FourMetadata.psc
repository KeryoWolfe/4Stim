Scriptname FourMetadata Native Hidden
{OStim NG's OMetadata for 4Stim (docs\API.md): what a scene is, by its ID.
All 282 of OMetadata's functions, with OStim's names and arguments, on the
natives below; the scene file format is docs\SCENES.md. HasActionTag takes an
action index, as OStim's (HasActionTagOnAny is the whole scene).}

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
	return HasAnySceneTag(ID, FourLibrary.SplitCSV(Tags))
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
	return HasAllSceneTags(ID, FourLibrary.SplitCSV(Tags))
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

; ---- 4Stim's natives and helpers for the functions below ----
; What the role at Position needs (penis, vagina, mouth...), its actions' included.
string[] Function GetActorRequirements(string Id, int Position) Global Native
; Case-insensitive list checks; ListOverlap is what of Have is in Wanted,
; lowercase, once each, sorted.
bool Function ListHasAny(string[] Have, string[] Wanted) Global Native
bool Function ListHasAll(string[] Have, string[] Wanted) Global Native
string[] Function ListOverlap(string[] Have, string[] Wanted) Global Native
int[] Function IntsOfCSV(string CSV) Global Native
float[] Function FloatsOfCSV(string CSV) Global Native
; What of Have is in Wanted, in Have's order (OStim's custom list overlaps).
int[] Function IntListOverlap(int[] Have, int[] Wanted) Global Native
float[] Function FloatListOverlap(float[] Have, float[] Wanted) Global Native
string[] Function StringListOverlap(string[] Have, string[] Wanted) Global Native
; The indexes of the actions of any of Types (any type if none) whose roles
; fit Mode: 0 any; 1 actor in Positions; 2 target in; 3 performer in; 4 actor
; in Positions and target in Positions2; 5 actor or target in; 6 both in;
; 7 actor, target or performer in; 8 all three in.
int[] Function FindActionsBy(string Id, string[] Types, int Mode, int[] Positions, int[] Positions2) Global Native
; FindActionsSuperloadCSVv2's search: its 25 strings after Id, in OStim's order.
int[] Function FindActionsSuperload(string Id, string[] Filters) Global Native
; Custom action data. Role: 0 actor, 1 target, 2 performer. Kind: 0 int,
; 1 float, 2 string, 3 int list, 4 float list, 5 string list.
bool Function HasCustomAction(string Id, int Index, int Role, int Kind, string Record) Global Native
int Function GetCustomActionInt(string Id, int Index, int Role, string Record, int Fallback) Global Native
float Function GetCustomActionFloat(string Id, int Index, int Role, string Record, float Fallback) Global Native
string Function GetCustomActionString(string Id, int Index, int Role, string Record, string Fallback) Global Native
int[] Function GetCustomActionIntList(string Id, int Index, int Role, string Record) Global Native
float[] Function GetCustomActionFloatList(string Id, int Index, int Role, string Record) Global Native
string[] Function GetCustomActionStringList(string Id, int Index, int Role, string Record) Global Native
; A role's custom value over the scene's actions. Op: 0 min, 1 max (Start is
; the fallback), 2 sum, 3 product (Start is the starting value).
int Function GetCustomSceneActorInt(string Id, int Position, string Record, int Op, int Start) Global Native
float Function GetCustomSceneActorFloat(string Id, int Position, string Record, int Op, float Start) Global Native

int[] Function EmptyInts() Global
	int[] noInts
	return noInts
EndFunction
int[] Function OneInt(int Value) Global
	int[] list = new int[1]
	list[0] = Value
	return list
EndFunction
int Function FirstOf(int[] List) Global
	if List.Length > 0
		return List[0]
	endif
	return -1
EndFunction
bool Function IntsAny(int[] Have, int[] Wanted) Global
	int i = 0
	while i < Wanted.Length
		if Have.Find(Wanted[i]) >= 0
			return true
		endif
		i += 1
	endwhile
	return false
EndFunction
bool Function IntsAll(int[] Have, int[] Wanted) Global
	int i = 0
	while i < Wanted.Length
		if Have.Find(Wanted[i]) < 0
			return false
		endif
		i += 1
	endwhile
	return true
EndFunction
bool Function FloatsAny(float[] Have, float[] Wanted) Global
	int i = 0
	while i < Wanted.Length
		if Have.Find(Wanted[i]) >= 0
			return true
		endif
		i += 1
	endwhile
	return false
EndFunction
bool Function FloatsAll(float[] Have, float[] Wanted) Global
	int i = 0
	while i < Wanted.Length
		if Have.Find(Wanted[i]) < 0
			return false
		endif
		i += 1
	endwhile
	return true
EndFunction

; ---- The rest of OMetadata (generated from OStim's OMetadata.psc) ----
string[] Function ScenesToNames(string[] Ids) Global Native

string Function GetAnimationId(string Id, int Index) Global Native

bool Function HasRequirement(string Id, int Position, string Requirement) Global
	return GetActorRequirements(Id, Position).Find(Requirement) >= 0
EndFunction

bool Function HasAnyRequirement(string Id, int Position, string[] Requirements) Global
	return ListHasAny(GetActorRequirements(Id, Position), Requirements)
EndFunction

bool Function HasAnyRequirementCSV(string Id, int Position, string Requirements) Global
	return ListHasAny(GetActorRequirements(Id, Position), FourLibrary.SplitCSV(Requirements))
EndFunction

bool Function HasAllRequirements(string Id, int Position, string[] Requirements) Global
	return ListHasAll(GetActorRequirements(Id, Position), Requirements)
EndFunction

bool Function HasAllRequirementsCSV(string Id, int Position, string Requirements) Global
	return ListHasAll(GetActorRequirements(Id, Position), FourLibrary.SplitCSV(Requirements))
EndFunction

string[] Function GetSceneTagOverlap(string Id, string[] Tags) Global
	return ListOverlap(GetSceneTags(Id), Tags)
EndFunction

string[] Function GetSceneTagOverlapCSV(string Id, string Tags) Global
	return ListOverlap(GetSceneTags(Id), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasAnyActorTag(string Id, int Position, string[] Tags) Global
	return ListHasAny(GetActorTags(Id, Position), Tags)
EndFunction

bool Function HasAnyActorTagCSV(string Id, int Position, string Tags) Global
	return ListHasAny(GetActorTags(Id, Position), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasAllActorTags(string Id, int Position, string[] Tags) Global
	return ListHasAll(GetActorTags(Id, Position), Tags)
EndFunction

bool Function HasAllActorTagsCSV(string Id, int Position, string Tags) Global
	return ListHasAll(GetActorTags(Id, Position), FourLibrary.SplitCSV(Tags))
EndFunction

string[] Function GetActorTagOverlap(string Id, int Position, string[] Tags) Global
	return ListOverlap(GetActorTags(Id, Position), Tags)
EndFunction

string[] Function GetActorTagOverlapCSV(string Id, int Position, string Tags) Global
	return ListOverlap(GetActorTags(Id, Position), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasActions(string Id) Global
	return GetActionCount(Id) > 0
EndFunction

int Function FindAnyAction(string Id, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 0, EmptyInts(), EmptyInts()))
EndFunction

int Function FindAnyActionCSV(string Id, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 0, EmptyInts(), EmptyInts()))
EndFunction

int[] Function FindActions(string Id, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 0, EmptyInts(), EmptyInts())
EndFunction

int[] Function FindAllActions(string Id, string[] Types) Global
	return FindActionsBy(Id, Types, 0, EmptyInts(), EmptyInts())
EndFunction

int[] Function FindAllActionsCSV(string Id, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 0, EmptyInts(), EmptyInts())
EndFunction

int Function FindAnyActionForActor(string Id, int Position, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 1, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForActorCSV(string Id, int Position, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 1, OneInt(Position), EmptyInts()))
EndFunction

int[] Function FindActionsForActor(string Id, int Position, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 1, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForActor(string Id, int Position, string[] Types) Global
	return FindActionsBy(Id, Types, 1, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForActorCSV(string Id, int Position, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 1, OneInt(Position), EmptyInts())
EndFunction

int Function FindActionForActors(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 1, Positions, EmptyInts()))
EndFunction

int Function FindActionForActorsCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 1, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForActors(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 1, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForActorsCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 1, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForActors(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 1, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForActorsCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 1, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForActors(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 1, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForActorsCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 1, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindAnyActionForTarget(string Id, int Position, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 2, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForTargetCSV(string Id, int Position, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 2, OneInt(Position), EmptyInts()))
EndFunction

int[] Function FindActionsForTarget(string Id, int Position, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 2, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForTarget(string Id, int Position, string[] Types) Global
	return FindActionsBy(Id, Types, 2, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForTargetCSV(string Id, int Position, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 2, OneInt(Position), EmptyInts())
EndFunction

int Function FindActionForTargets(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 2, Positions, EmptyInts()))
EndFunction

int Function FindActionForTargetsCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 2, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForTargets(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 2, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForTargetsCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 2, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForTargets(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 2, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForTargetsCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 2, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForTargets(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 2, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForTargetsCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 2, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindActionForPerformer(string Id, int Position, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 3, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForPerformer(string Id, int Position, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 3, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForPerformerCSV(string Id, int Position, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 3, OneInt(Position), EmptyInts()))
EndFunction

int[] Function FindActionsForPerformer(string Id, int Position, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 3, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForPerformer(string Id, int Position, string[] Types) Global
	return FindActionsBy(Id, Types, 3, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForPerformerCSV(string Id, int Position, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 3, OneInt(Position), EmptyInts())
EndFunction

int Function FindActionForPerformers(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 3, Positions, EmptyInts()))
EndFunction

int Function FindActionForPerformersCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 3, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForPerformers(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 3, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForPerformersCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 3, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForPerformers(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 3, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForPerformersCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 3, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForPerformers(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 3, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForPerformersCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 3, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindActionForActorAndTarget(string Id, int ActorPosition, int TargetPosition, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 4, OneInt(ActorPosition), OneInt(TargetPosition)))
EndFunction

int Function FindAnyActionForActorAndTarget(string Id, int ActorPosition, int TargetPosition, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 4, OneInt(ActorPosition), OneInt(TargetPosition)))
EndFunction

int Function FindAnyActionForActorAndTargetCSV(string Id, int ActorPosition, int TargetPosition, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 4, OneInt(ActorPosition), OneInt(TargetPosition)))
EndFunction

int[] Function FindActionsForActorAndTarget(string Id, int ActorPosition, int TargetPosition, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 4, OneInt(ActorPosition), OneInt(TargetPosition))
EndFunction

int[] Function FindAllActionsForActorAndTarget(string Id, int ActorPosition, int TargetPosition, string[] Types) Global
	return FindActionsBy(Id, Types, 4, OneInt(ActorPosition), OneInt(TargetPosition))
EndFunction

int[] Function FindAllActionsForActorAndTargetCSV(string Id, int ActorPosition, int TargetPosition, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 4, OneInt(ActorPosition), OneInt(TargetPosition))
EndFunction

int Function FindActionForActorsAndTargets(string Id, int[] ActorPositions, int[] TargetPositions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 4, ActorPositions, TargetPositions))
EndFunction

int Function FindActionForActorsAndTargetsCSV(string Id, string ActorPositions, string TargetPositions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 4, IntsOfCSV(ActorPositions), IntsOfCSV(TargetPositions)))
EndFunction

int Function FindAnyActionForActorsAndTargets(string Id, int[] ActorPositions, int[] TargetPositions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 4, ActorPositions, TargetPositions))
EndFunction

int Function FindAnyActionForActorsAndTargetsCSV(string Id, string ActorPositions, string TargetPositions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 4, IntsOfCSV(ActorPositions), IntsOfCSV(TargetPositions)))
EndFunction

int[] Function FindActionsForActorsAndTargets(string Id, int[] ActorPositions, int[] TargetPositions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 4, ActorPositions, TargetPositions)
EndFunction

int[] Function FindActionsForActorsAndTargetsCSV(string Id, string ActorPositions, string TargetPositions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 4, IntsOfCSV(ActorPositions), IntsOfCSV(TargetPositions))
EndFunction

int[] Function FindAllActionsForActorsAndTargets(string Id, int[] ActorPositions, int[] TargetPositions, string[] Types) Global
	return FindActionsBy(Id, Types, 4, ActorPositions, TargetPositions)
EndFunction

int[] Function FindAllActionsForActorsAndTargetsCSV(string Id, string ActorPositions, string TargetPositions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 4, IntsOfCSV(ActorPositions), IntsOfCSV(TargetPositions))
EndFunction

int Function FindActionForMate(string Id, int Position, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 5, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForMate(string Id, int Position, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 5, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForMateCSV(string Id, int Position, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 5, OneInt(Position), EmptyInts()))
EndFunction

int[] Function FindActionsForMate(string Id, int Position, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 5, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForMate(string Id, int Position, string[] Types) Global
	return FindActionsBy(Id, Types, 5, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForMateCSV(string Id, int Position, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 5, OneInt(Position), EmptyInts())
EndFunction

int Function FindActionForMatesAny(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 5, Positions, EmptyInts()))
EndFunction

int Function FindActionForMatesAnyCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 5, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForMatesAny(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 5, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForMatesAnyCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 5, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForMatesAny(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 5, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForMatesAnyCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 5, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForMatesAny(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 5, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForMatesAnyCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 5, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindActionForMatesAll(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 6, Positions, EmptyInts()))
EndFunction

int Function FindActionForMatesAllCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 6, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForMatesAll(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 6, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForMatesAllCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 6, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForMatesAll(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 6, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForMatesAllCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 6, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForMatesAll(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 6, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForMatesAllCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 6, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindActionForParticipant(string Id, int Position, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 7, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForParticipant(string Id, int Position, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 7, OneInt(Position), EmptyInts()))
EndFunction

int Function FindAnyActionForParticipantCSV(string Id, int Position, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 7, OneInt(Position), EmptyInts()))
EndFunction

int[] Function FindActionsForParticipant(string Id, int Position, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 7, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForParticipant(string Id, int Position, string[] Types) Global
	return FindActionsBy(Id, Types, 7, OneInt(Position), EmptyInts())
EndFunction

int[] Function FindAllActionsForParticipantCSV(string Id, int Position, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 7, OneInt(Position), EmptyInts())
EndFunction

int Function FindActionForParticipantsAny(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 7, Positions, EmptyInts()))
EndFunction

int Function FindActionForParticipantsAnyCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 7, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForParticipantsAny(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 7, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForParticipantsAnyCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 7, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForParticipantsAny(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 7, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForParticipantsAnyCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 7, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForParticipantsAny(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 7, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForParticipantsAnyCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 7, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindActionForParticipantsAll(string Id, int[] Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 8, Positions, EmptyInts()))
EndFunction

int Function FindActionForParticipantsAllCSV(string Id, string Positions, string Type) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.One(Type), 8, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int Function FindAnyActionForParticipantsAll(string Id, int[] Positions, string[] Types) Global
	return FirstOf(FindActionsBy(Id, Types, 8, Positions, EmptyInts()))
EndFunction

int Function FindAnyActionForParticipantsAllCSV(string Id, string Positions, string Types) Global
	return FirstOf(FindActionsBy(Id, FourLibrary.SplitCSV(Types), 8, IntsOfCSV(Positions), EmptyInts()))
EndFunction

int[] Function FindActionsForParticipantsAll(string Id, int[] Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 8, Positions, EmptyInts())
EndFunction

int[] Function FindActionsForParticipantsAllCSV(string Id, string Positions, string Type) Global
	return FindActionsBy(Id, FourLibrary.One(Type), 8, IntsOfCSV(Positions), EmptyInts())
EndFunction

int[] Function FindAllActionsForParticipantsAll(string Id, int[] Positions, string[] Types) Global
	return FindActionsBy(Id, Types, 8, Positions, EmptyInts())
EndFunction

int[] Function FindAllActionsForParticipantsAllCSV(string Id, string Positions, string Types) Global
	return FindActionsBy(Id, FourLibrary.SplitCSV(Types), 8, IntsOfCSV(Positions), EmptyInts())
EndFunction

int Function FindActionSuperloadCSVv2(string Id, string ActorPositions = "", string TargetPositions = "", string PerformerPositions = "", string MatePositionsAny = "", string MatePositionsAll = "", string ParticipantPositionsAny = "", string ParticipantPositionsAll = "", string Types = "", string AnyActionTag = "", string AllActionTags = "", string ActionTagWhitelist = "", string ActionTagBlacklist = "", string AnyCustomIntRecord = "", string AllCustomIntRecords = "", string AnyCustomFloatRecord = "", string allCustomFloatRecords = "", string anyCustomStringRecord = "", string allCustomStringRecords = "", string AnyCustomIntListRecord = "", string AllCustomIntListRecords = "", string AnyCustomFloatListRecord = "", string AllCustomFloatListRecords = "", string AnyCustomStringListRecord = "", string AllCustomStringListRecords = "") Global
	string[] filters = new string[25]
	filters[0] = ActorPositions
	filters[1] = TargetPositions
	filters[2] = PerformerPositions
	filters[3] = MatePositionsAny
	filters[4] = MatePositionsAll
	filters[5] = ParticipantPositionsAny
	filters[6] = ParticipantPositionsAll
	filters[7] = Types
	filters[8] = AnyActionTag
	filters[9] = AllActionTags
	filters[10] = ActionTagWhitelist
	filters[11] = ActionTagBlacklist
	filters[12] = AnyCustomIntRecord
	filters[13] = AllCustomIntRecords
	filters[14] = AnyCustomFloatRecord
	filters[15] = allCustomFloatRecords
	filters[16] = anyCustomStringRecord
	filters[17] = allCustomStringRecords
	filters[18] = AnyCustomIntListRecord
	filters[19] = AllCustomIntListRecords
	filters[20] = AnyCustomFloatListRecord
	filters[21] = AllCustomFloatListRecords
	filters[22] = AnyCustomStringListRecord
	filters[23] = AllCustomStringListRecords
	return FirstOf(FindActionsSuperload(Id, filters))
EndFunction

int[] Function FindActionsSuperloadCSVv2(string Id, string ActorPositions = "", string TargetPositions = "", string PerformerPositions = "", string MatePositionsAny = "", string MatePositionsAll = "", string ParticipantPositionsAny = "", string ParticipantPositionsAll = "", string Types = "", string AnyActionTag = "", string AllActionTags = "", string ActionTagWhitelist = "", string ActionTagBlacklist = "", string AnyCustomIntRecord = "", string AllCustomIntRecords = "", string AnyCustomFloatRecord = "", string allCustomFloatRecords = "", string anyCustomStringRecord = "", string allCustomStringRecords = "", string AnyCustomIntListRecord = "", string AllCustomIntListRecords = "", string AnyCustomFloatListRecord = "", string AllCustomFloatListRecords = "", string AnyCustomStringListRecord = "", string AllCustomStringListRecords = "") Global
	string[] filters = new string[25]
	filters[0] = ActorPositions
	filters[1] = TargetPositions
	filters[2] = PerformerPositions
	filters[3] = MatePositionsAny
	filters[4] = MatePositionsAll
	filters[5] = ParticipantPositionsAny
	filters[6] = ParticipantPositionsAll
	filters[7] = Types
	filters[8] = AnyActionTag
	filters[9] = AllActionTags
	filters[10] = ActionTagWhitelist
	filters[11] = ActionTagBlacklist
	filters[12] = AnyCustomIntRecord
	filters[13] = AllCustomIntRecords
	filters[14] = AnyCustomFloatRecord
	filters[15] = allCustomFloatRecords
	filters[16] = anyCustomStringRecord
	filters[17] = allCustomStringRecords
	filters[18] = AnyCustomIntListRecord
	filters[19] = AllCustomIntListRecords
	filters[20] = AnyCustomFloatListRecord
	filters[21] = AllCustomFloatListRecords
	filters[22] = AnyCustomStringListRecord
	filters[23] = AllCustomStringListRecords
	return FindActionsSuperload(Id, filters)
EndFunction

string[] Function GetActionTypes(string Id) Global Native

int[] Function GetActionActors(string Id) Global Native

int[] Function GetActionTargets(string Id) Global Native

int[] Function GetActionPerformers(string Id) Global Native

string[] Function GetActionTags(string Id, int Index) Global Native

string[] Function GetAllActionsTags(string Id) Global Native

bool Function HasActionTag(string Id, int Index, string Tag) Global
	return GetActionTags(Id, Index).Find(Tag) >= 0
EndFunction

bool Function HasActionTagOnAny(string Id, string Tag) Global
	return GetAllActionsTags(Id).Find(Tag) >= 0
EndFunction

bool Function HasAnyActionTag(string Id, int Index, string[] Tags) Global
	return ListHasAny(GetActionTags(Id, Index), Tags)
EndFunction

bool Function HasAnyActionTagCSV(string Id, int Index, string Tags) Global
	return ListHasAny(GetActionTags(Id, Index), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasAnyActionTagOnAny(string Id, string[] Tags) Global
	return ListHasAny(GetAllActionsTags(Id), Tags)
EndFunction

bool Function HasAnyActionTagOnAnyCSV(string Id, string Tags) Global
	return ListHasAny(GetAllActionsTags(Id), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasAllActionTags(string Id, int Index, string[] Tags) Global
	return ListHasAll(GetActionTags(Id, Index), Tags)
EndFunction

bool Function HasAllActionTagsCSV(string Id, int Index, string Tags) Global
	return ListHasAll(GetActionTags(Id, Index), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasAllActionTagsOnAny(string Id, string[] Tags) Global
	string[] wanted = Tags
	int i = 0
	int count = GetActionCount(Id)
	while i < count
		if ListHasAll(GetActionTags(Id, i), wanted)
			return true
		endif
		i += 1
	endwhile
	return false
EndFunction

bool Function HasAllActionTagsOnAnyCSV(string Id, string Tags) Global
	string[] wanted = FourLibrary.SplitCSV(Tags)
	int i = 0
	int count = GetActionCount(Id)
	while i < count
		if ListHasAll(GetActionTags(Id, i), wanted)
			return true
		endif
		i += 1
	endwhile
	return false
EndFunction

bool Function HasAllActionTagsOverAll(string Id, string[] Tags) Global
	return ListHasAll(GetAllActionsTags(Id), Tags)
EndFunction

bool Function HasAllActionTagsOverAllCSV(string Id, string Tags) Global
	return ListHasAll(GetAllActionsTags(Id), FourLibrary.SplitCSV(Tags))
EndFunction

string[] Function GetActionTagOverlap(string Id, int Index, string[] Tags) Global
	return ListOverlap(GetActionTags(Id, Index), Tags)
EndFunction

string[] Function GetActionTagOverlapCSV(string Id, int Index, string Tags) Global
	return ListOverlap(GetActionTags(Id, Index), FourLibrary.SplitCSV(Tags))
EndFunction

string[] Function GetActionTagOverlapOverAll(string Id, string[] Tags) Global
	return ListOverlap(GetAllActionsTags(Id), Tags)
EndFunction

string[] Function GetActionTagOverlapOverAllCSV(string Id, string Tags) Global
	return ListOverlap(GetAllActionsTags(Id), FourLibrary.SplitCSV(Tags))
EndFunction

bool Function HasCustomActionActorInt(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 0, 0, Record)
EndFunction

int Function GetCustomActionActorInt(string Id, int Index, string Record, int Fallback = 0) Global
	return GetCustomActionInt(Id, Index, 0, Record, Fallback)
EndFunction

bool Function IsCustomActionActorInt(string Id, int Index, string Record, int Value) Global
	return HasCustomAction(Id, Index, 0, 0, Record) && GetCustomActionInt(Id, Index, 0, Record, 0) == Value
EndFunction

bool Function HasCustomActionActorFloat(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 0, 1, Record)
EndFunction

float Function GetCustomActionActorFloat(string Id, int Index, string Record, float Fallback = 0.0) Global
	return GetCustomActionFloat(Id, Index, 0, Record, Fallback)
EndFunction

bool Function IsCustomActionActorFloat(string Id, int Index, string Record, float Value) Global
	return HasCustomAction(Id, Index, 0, 1, Record) && GetCustomActionFloat(Id, Index, 0, Record, 0.0) == Value
EndFunction

bool Function HasCustomActionActorString(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 0, 2, Record)
EndFunction

string Function GetCustomActionActorString(string Id, int Index, string Record, string Fallback = "") Global
	return GetCustomActionString(Id, Index, 0, Record, Fallback)
EndFunction

bool Function IsCustomActionActorString(string Id, int Index, string Record, string Value) Global
	return HasCustomAction(Id, Index, 0, 2, Record) && GetCustomActionString(Id, Index, 0, Record, "") == Value
EndFunction

bool Function HasCustomActionActorIntList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 0, 3, Record)
EndFunction

int[] Function GetCustomActionActorIntList(string Id, int Index, string Record) Global
	return GetCustomActionIntList(Id, Index, 0, Record)
EndFunction

bool Function CustomActionActorIntListContains(string Id, int Index, string Record, int Value) Global
	return GetCustomActionIntList(Id, Index, 0, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionActorIntListContainsAny(string Id, int Index, string Record, int[] Values) Global
	return IntsAny(GetCustomActionIntList(Id, Index, 0, Record), Values)
EndFunction

bool Function CustomActionActorIntListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return IntsAny(GetCustomActionIntList(Id, Index, 0, Record), IntsOfCSV(Values))
EndFunction

bool Function CustomActionActorIntListContainsAll(string Id, int Index, string Record, int[] Values) Global
	return IntsAll(GetCustomActionIntList(Id, Index, 0, Record), Values)
EndFunction

bool Function CustomActionActorIntListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return IntsAll(GetCustomActionIntList(Id, Index, 0, Record), IntsOfCSV(Values))
EndFunction

int[] Function GetCustomActionActorIntListOverlap(string Id, int Index, string Record, int[] Values) Global
	return IntListOverlap(GetCustomActionIntList(Id, Index, 0, Record), Values)
EndFunction

int[] Function GetCustomActionActorIntListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return IntListOverlap(GetCustomActionIntList(Id, Index, 0, Record), IntsOfCSV(Values))
EndFunction

bool Function HasCustomActionActorFloatList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 0, 4, Record)
EndFunction

float[] Function GetCustomActionActorFloatList(string Id, int Index, string Record) Global
	return GetCustomActionFloatList(Id, Index, 0, Record)
EndFunction

bool Function CustomActionActorFloatListContains(string Id, int Index, string Record, float Value) Global
	return GetCustomActionFloatList(Id, Index, 0, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionActorFloatListContainsAny(string Id, int Index, string Record, float[] Values) Global
	return FloatsAny(GetCustomActionFloatList(Id, Index, 0, Record), Values)
EndFunction

bool Function CustomActionActorFloatListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return FloatsAny(GetCustomActionFloatList(Id, Index, 0, Record), FloatsOfCSV(Values))
EndFunction

bool Function CustomActionActorFloatListContainsAll(string Id, int Index, string Record, float[] Values) Global
	return FloatsAll(GetCustomActionFloatList(Id, Index, 0, Record), Values)
EndFunction

bool Function CustomActionActorFloatListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return FloatsAll(GetCustomActionFloatList(Id, Index, 0, Record), FloatsOfCSV(Values))
EndFunction

float[] Function GetCustomActionActorFloatListOverlap(string Id, int Index, string Record, float[] Values) Global
	return FloatListOverlap(GetCustomActionFloatList(Id, Index, 0, Record), Values)
EndFunction

float[] Function GetCustomActionActorFloatListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return FloatListOverlap(GetCustomActionFloatList(Id, Index, 0, Record), FloatsOfCSV(Values))
EndFunction

bool Function HasCustomActionActorStringList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 0, 5, Record)
EndFunction

string[] Function GetCustomActionActorStringList(string Id, int Index, string Record) Global
	return GetCustomActionStringList(Id, Index, 0, Record)
EndFunction

bool Function CustomActionActorStringListContains(string Id, int Index, string Record, string Value) Global
	return GetCustomActionStringList(Id, Index, 0, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionActorStringListContainsAny(string Id, int Index, string Record, string[] Values) Global
	return ListHasAny(GetCustomActionStringList(Id, Index, 0, Record), Values)
EndFunction

bool Function CustomActionActorStringListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return ListHasAny(GetCustomActionStringList(Id, Index, 0, Record), FourLibrary.SplitCSV(Values))
EndFunction

bool Function CustomActionActorStringListContainsAll(string Id, int Index, string Record, string[] Values) Global
	return ListHasAll(GetCustomActionStringList(Id, Index, 0, Record), Values)
EndFunction

bool Function CustomActionActorStringListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return ListHasAll(GetCustomActionStringList(Id, Index, 0, Record), FourLibrary.SplitCSV(Values))
EndFunction

string[] Function GetCustomActionActorStringListOverlap(string Id, int Index, string Record, string[] Values) Global
	return StringListOverlap(GetCustomActionStringList(Id, Index, 0, Record), Values)
EndFunction

string[] Function GetCustomActionActorStringListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return StringListOverlap(GetCustomActionStringList(Id, Index, 0, Record), FourLibrary.SplitCSV(Values))
EndFunction

bool Function HasCustomActionTargetInt(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 1, 0, Record)
EndFunction

int Function GetCustomActionTargetInt(string Id, int Index, string Record, int Fallback = 0) Global
	return GetCustomActionInt(Id, Index, 1, Record, Fallback)
EndFunction

bool Function IsCustomActionTargetInt(string Id, int Index, string Record, int Value) Global
	return HasCustomAction(Id, Index, 1, 0, Record) && GetCustomActionInt(Id, Index, 1, Record, 0) == Value
EndFunction

bool Function HasCustomActionTargetFloat(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 1, 1, Record)
EndFunction

float Function GetCustomActionTargetFloat(string Id, int Index, string Record, float Fallback = 0.0) Global
	return GetCustomActionFloat(Id, Index, 1, Record, Fallback)
EndFunction

bool Function IsCustomActionTargetFloat(string Id, int Index, string Record, float Value) Global
	return HasCustomAction(Id, Index, 1, 1, Record) && GetCustomActionFloat(Id, Index, 1, Record, 0.0) == Value
EndFunction

bool Function HasCustomActionTargetString(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 1, 2, Record)
EndFunction

string Function GetCustomActionTargetString(string Id, int Index, string Record, string Fallback = "") Global
	return GetCustomActionString(Id, Index, 1, Record, Fallback)
EndFunction

bool Function IsCustomActionTargetString(string Id, int Index, string Record, string Value) Global
	return HasCustomAction(Id, Index, 1, 2, Record) && GetCustomActionString(Id, Index, 1, Record, "") == Value
EndFunction

bool Function HasCustomActionTargetIntList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 1, 3, Record)
EndFunction

int[] Function GetCustomActionTargetIntList(string Id, int Index, string Record) Global
	return GetCustomActionIntList(Id, Index, 1, Record)
EndFunction

bool Function CustomActionTargetIntListContains(string Id, int Index, string Record, int Value) Global
	return GetCustomActionIntList(Id, Index, 1, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionTargetIntListContainsAny(string Id, int Index, string Record, int[] Values) Global
	return IntsAny(GetCustomActionIntList(Id, Index, 1, Record), Values)
EndFunction

bool Function CustomActionTargetIntListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return IntsAny(GetCustomActionIntList(Id, Index, 1, Record), IntsOfCSV(Values))
EndFunction

bool Function CustomActionTargetIntListContainsAll(string Id, int Index, string Record, int[] Values) Global
	return IntsAll(GetCustomActionIntList(Id, Index, 1, Record), Values)
EndFunction

bool Function CustomActionTargetIntListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return IntsAll(GetCustomActionIntList(Id, Index, 1, Record), IntsOfCSV(Values))
EndFunction

int[] Function GetCustomActionTargetIntListOverlap(string Id, int Index, string Record, int[] Values) Global
	return IntListOverlap(GetCustomActionIntList(Id, Index, 1, Record), Values)
EndFunction

int[] Function GetCustomActionTargetIntListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return IntListOverlap(GetCustomActionIntList(Id, Index, 1, Record), IntsOfCSV(Values))
EndFunction

bool Function HasCustomActionTargetFloatList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 1, 4, Record)
EndFunction

float[] Function GetCustomActionTargetFloatList(string Id, int Index, string Record) Global
	return GetCustomActionFloatList(Id, Index, 1, Record)
EndFunction

bool Function CustomActionTargetFloatListContains(string Id, int Index, string Record, float Value) Global
	return GetCustomActionFloatList(Id, Index, 1, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionTargetFloatListContainsAny(string Id, int Index, string Record, float[] Values) Global
	return FloatsAny(GetCustomActionFloatList(Id, Index, 1, Record), Values)
EndFunction

bool Function CustomActionTargetFloatListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return FloatsAny(GetCustomActionFloatList(Id, Index, 1, Record), FloatsOfCSV(Values))
EndFunction

bool Function CustomActionTargetFloatListContainsAll(string Id, int Index, string Record, float[] Values) Global
	return FloatsAll(GetCustomActionFloatList(Id, Index, 1, Record), Values)
EndFunction

bool Function CustomActionTargetFloatListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return FloatsAll(GetCustomActionFloatList(Id, Index, 1, Record), FloatsOfCSV(Values))
EndFunction

float[] Function GetCustomActionTargetFloatListOverlap(string Id, int Index, string Record, float[] Values) Global
	return FloatListOverlap(GetCustomActionFloatList(Id, Index, 1, Record), Values)
EndFunction

float[] Function GetCustomActionTargetFloatListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return FloatListOverlap(GetCustomActionFloatList(Id, Index, 1, Record), FloatsOfCSV(Values))
EndFunction

bool Function HasCustomActionTargetStringList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 1, 5, Record)
EndFunction

string[] Function GetCustomActionTargetStringList(string Id, int Index, string Record) Global
	return GetCustomActionStringList(Id, Index, 1, Record)
EndFunction

bool Function CustomActionTargetStringListContains(string Id, int Index, string Record, string Value) Global
	return GetCustomActionStringList(Id, Index, 1, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionTargetStringListContainsAny(string Id, int Index, string Record, string[] Values) Global
	return ListHasAny(GetCustomActionStringList(Id, Index, 1, Record), Values)
EndFunction

bool Function CustomActionTargetStringListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return ListHasAny(GetCustomActionStringList(Id, Index, 1, Record), FourLibrary.SplitCSV(Values))
EndFunction

bool Function CustomActionTargetStringListContainsAll(string Id, int Index, string Record, string[] Values) Global
	return ListHasAll(GetCustomActionStringList(Id, Index, 1, Record), Values)
EndFunction

bool Function CustomActionTargetStringListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return ListHasAll(GetCustomActionStringList(Id, Index, 1, Record), FourLibrary.SplitCSV(Values))
EndFunction

string[] Function GetCustomActionTargetStringListOverlap(string Id, int Index, string Record, string[] Values) Global
	return StringListOverlap(GetCustomActionStringList(Id, Index, 1, Record), Values)
EndFunction

string[] Function GetCustomActionTargetStringListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return StringListOverlap(GetCustomActionStringList(Id, Index, 1, Record), FourLibrary.SplitCSV(Values))
EndFunction

bool Function HasCustomActionPerformerInt(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 2, 0, Record)
EndFunction

int Function GetCustomActionPerformerInt(string Id, int Index, string Record, int Fallback = 0) Global
	return GetCustomActionInt(Id, Index, 2, Record, Fallback)
EndFunction

bool Function IsCustomActionPerformerInt(string Id, int Index, string Record, int Value) Global
	return HasCustomAction(Id, Index, 2, 0, Record) && GetCustomActionInt(Id, Index, 2, Record, 0) == Value
EndFunction

bool Function HasCustomActionPerformerFloat(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 2, 1, Record)
EndFunction

float Function GetCustomActionPerformerFloat(string Id, int Index, string Record, float Fallback = 0.0) Global
	return GetCustomActionFloat(Id, Index, 2, Record, Fallback)
EndFunction

bool Function IsCustomActionPerformerFloat(string Id, int Index, string Record, float Value) Global
	return HasCustomAction(Id, Index, 2, 1, Record) && GetCustomActionFloat(Id, Index, 2, Record, 0.0) == Value
EndFunction

bool Function HasCustomActionPerformerString(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 2, 2, Record)
EndFunction

string Function GetCustomActionPerformerString(string Id, int Index, string Record, string Fallback = "") Global
	return GetCustomActionString(Id, Index, 2, Record, Fallback)
EndFunction

bool Function IsCustomActionPerformerString(string Id, int Index, string Record, string Value) Global
	return HasCustomAction(Id, Index, 2, 2, Record) && GetCustomActionString(Id, Index, 2, Record, "") == Value
EndFunction

bool Function HasCustomActionPerformerIntList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 2, 3, Record)
EndFunction

int[] Function GetCustomActionPerformerIntList(string Id, int Index, string Record) Global
	return GetCustomActionIntList(Id, Index, 2, Record)
EndFunction

bool Function CustomActionPerformerIntListContains(string Id, int Index, string Record, int Value) Global
	return GetCustomActionIntList(Id, Index, 2, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionPerformerIntListContainsAny(string Id, int Index, string Record, int[] Values) Global
	return IntsAny(GetCustomActionIntList(Id, Index, 2, Record), Values)
EndFunction

bool Function CustomActionPerformerIntListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return IntsAny(GetCustomActionIntList(Id, Index, 2, Record), IntsOfCSV(Values))
EndFunction

bool Function CustomActionPerformerIntListContainsAll(string Id, int Index, string Record, int[] Values) Global
	return IntsAll(GetCustomActionIntList(Id, Index, 2, Record), Values)
EndFunction

bool Function CustomActionPerformerIntListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return IntsAll(GetCustomActionIntList(Id, Index, 2, Record), IntsOfCSV(Values))
EndFunction

int[] Function GetCustomActionPerformerIntListOverlap(string Id, int Index, string Record, int[] Values) Global
	return IntListOverlap(GetCustomActionIntList(Id, Index, 2, Record), Values)
EndFunction

int[] Function GetCustomActionPerformerIntListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return IntListOverlap(GetCustomActionIntList(Id, Index, 2, Record), IntsOfCSV(Values))
EndFunction

bool Function HasCustomActionPerformerFloatList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 2, 4, Record)
EndFunction

float[] Function GetCustomActionPerformerFloatList(string Id, int Index, string Record) Global
	return GetCustomActionFloatList(Id, Index, 2, Record)
EndFunction

bool Function CustomActionPerformerFloatListContains(string Id, int Index, string Record, float Value) Global
	return GetCustomActionFloatList(Id, Index, 2, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionPerformerFloatListContainsAny(string Id, int Index, string Record, float[] Values) Global
	return FloatsAny(GetCustomActionFloatList(Id, Index, 2, Record), Values)
EndFunction

bool Function CustomActionPerformerFloatListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return FloatsAny(GetCustomActionFloatList(Id, Index, 2, Record), FloatsOfCSV(Values))
EndFunction

bool Function CustomActionPerformerFloatListContainsAll(string Id, int Index, string Record, float[] Values) Global
	return FloatsAll(GetCustomActionFloatList(Id, Index, 2, Record), Values)
EndFunction

bool Function CustomActionPerformerFloatListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return FloatsAll(GetCustomActionFloatList(Id, Index, 2, Record), FloatsOfCSV(Values))
EndFunction

float[] Function GetCustomActionPerformerFloatListOverlap(string Id, int Index, string Record, float[] Values) Global
	return FloatListOverlap(GetCustomActionFloatList(Id, Index, 2, Record), Values)
EndFunction

float[] Function GetCustomActionPerformerFloatListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return FloatListOverlap(GetCustomActionFloatList(Id, Index, 2, Record), FloatsOfCSV(Values))
EndFunction

bool Function HasCustomActionPerformerStringList(string Id, int Index, string Record) Global
	return HasCustomAction(Id, Index, 2, 5, Record)
EndFunction

string[] Function GetCustomActionPerformerStringList(string Id, int Index, string Record) Global
	return GetCustomActionStringList(Id, Index, 2, Record)
EndFunction

bool Function CustomActionPerformerStringListContains(string Id, int Index, string Record, string Value) Global
	return GetCustomActionStringList(Id, Index, 2, Record).Find(Value) >= 0
EndFunction

bool Function CustomActionPerformerStringListContainsAny(string Id, int Index, string Record, string[] Values) Global
	return ListHasAny(GetCustomActionStringList(Id, Index, 2, Record), Values)
EndFunction

bool Function CustomActionPerformerStringListContainsAnyCSV(string Id, int Index, string Record, string Values) Global
	return ListHasAny(GetCustomActionStringList(Id, Index, 2, Record), FourLibrary.SplitCSV(Values))
EndFunction

bool Function CustomActionPerformerStringListContainsAll(string Id, int Index, string Record, string[] Values) Global
	return ListHasAll(GetCustomActionStringList(Id, Index, 2, Record), Values)
EndFunction

bool Function CustomActionPerformerStringListContainsAllCSV(string Id, int Index, string Record, string Values) Global
	return ListHasAll(GetCustomActionStringList(Id, Index, 2, Record), FourLibrary.SplitCSV(Values))
EndFunction

string[] Function GetCustomActionPerformerStringListOverlap(string Id, int Index, string Record, string[] Values) Global
	return StringListOverlap(GetCustomActionStringList(Id, Index, 2, Record), Values)
EndFunction

string[] Function GetCustomActionPerformerStringListOverlapCSV(string Id, int Index, string Record, string Values) Global
	return StringListOverlap(GetCustomActionStringList(Id, Index, 2, Record), FourLibrary.SplitCSV(Values))
EndFunction

int Function GetCustomSceneActorIntMin(string Id, int Position, string Record, int Fallback = 0) Global
	return GetCustomSceneActorInt(Id, Position, Record, 0, Fallback)
EndFunction

int Function GetCustomSceneActorIntMax(string Id, int Position, string Record, int Fallback = 0) Global
	return GetCustomSceneActorInt(Id, Position, Record, 1, Fallback)
EndFunction

int Function GetCustomSceneActorIntSum(string Id, int Position, string Record, int StartValue = 0) Global
	return GetCustomSceneActorInt(Id, Position, Record, 2, StartValue)
EndFunction

int Function GetCustomSceneActorIntProduct(string Id, int Position, string Record, int StartValue = 1) Global
	return GetCustomSceneActorInt(Id, Position, Record, 3, StartValue)
EndFunction

float Function GetCustomSceneActorFloatMin(string Id, int Position, string Record, float Fallback = 0.0) Global
	return GetCustomSceneActorFloat(Id, Position, Record, 0, Fallback)
EndFunction

float Function GetCustomSceneActorFloatMax(string Id, int Position, string Record, float Fallback = 0.0) Global
	return GetCustomSceneActorFloat(Id, Position, Record, 1, Fallback)
EndFunction

float Function GetCustomSceneActorFloatSum(string Id, int Position, string Record, float StartValue = 0.0) Global
	return GetCustomSceneActorFloat(Id, Position, Record, 2, StartValue)
EndFunction

float Function GetCustomSceneActorFloatProduct(string Id, int Position, string Record, float StartValue = 1.0) Global
	return GetCustomSceneActorFloat(Id, Position, Record, 3, StartValue)
EndFunction

; Kept by OStim for old addons (declared there without Global; Global here, so they can be called).
int Function FindActionSuperloadCSV(string Id, string ActorPositions = "", string TargetPositions = "", string PerformerPositions = "", string MatePositionsAny = "", string MatePositionsAll = "", string ParticipantPositionsAny = "", string ParticipantPositionsAll = "", string Types = "") Global
	return FindActionSuperloadCSVv2(Id, ActorPositions, TargetPositions, PerformerPositions, MatePositionsAny, MatePositionsAll, ParticipantPositionsAny, ParticipantPositionsAll, Types)
EndFunction
int[] Function FindActionsSuperloadCSV(string Id, string ActorPositions = "", string TargetPositions = "", string PerformerPositions = "", string MatePositionsAny = "", string MatePositionsAll = "", string ParticipantPositionsAny = "", string ParticipantPositionsAll = "", string Types = "") Global
	return FindActionsSuperloadCSVv2(Id, ActorPositions, TargetPositions, PerformerPositions, MatePositionsAny, MatePositionsAll, ParticipantPositionsAny, ParticipantPositionsAll, Types)
EndFunction
