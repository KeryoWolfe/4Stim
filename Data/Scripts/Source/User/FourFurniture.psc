Scriptname FourFurniture Native Hidden
{OStim NG's OFurniture for 4Stim (docs\API.md): furniture types and finding
furniture.}


; The furniture around CenterRef a scene for ActorCount actors could be played
; on, nearest first (0 = any count). Radius 0 = fFurnitureRadius, SameFloor 0 =
; fFurnitureHeight (how far up or down it may be). Furniture in use or blocked
; is left out.
ObjectReference[] Function FindFurniture(int ActorCount, ObjectReference CenterRef, float Radius = 0.0, float SameFloor = 0.0) Global Native

; The nearest furniture of Type (or a kind of it) around CenterRef, or None.
ObjectReference Function FindFurnitureOfType(string Type, ObjectReference CenterRef, float Radius = 0.0, float SameFloor = 0.0) Global Native

string Function GetFurnitureType(ObjectReference FurnitureRef) Global
	return FourStim.GetFurnitureType(FurnitureRef)
EndFunction

; Whether SubType is SuperType or a kind of it.
bool Function IsChildOf(string SuperType, string SubType) Global
	return FourStim.IsFurnitureChildOf(SuperType, SubType)
EndFunction

; Where a scene goes on the piece, in its own frame, as OStim's
; [x, y, z, rotation (radians), scale]. Empty if no scene type knows it.
float[] Function GetOffset(ObjectReference FurnitureRef) Global Native
; The thread using the piece, -1 if none.
int Function GetSceneID(ObjectReference FurnitureRef) Global Native
; OStim's ResetClutter: kept for add-ons, but it does nothing, as in OStim
; (its body is commented out there). 4Stim puts clutter back by itself after
; a scene (bResetClutter).
Function ResetClutter(ObjectReference CenterRef, float Radius) Global
EndFunction
