Scriptname FourEvent Native Hidden
{OStim NG's OEvent for 4Stim (docs\API.md): events (docs\EVENTS.md).}

; Whether SubType is SuperType or has it among its supertypes, as OStim's.
bool Function IsChildOf(string SuperType, string SubType) Global
	return FourStim.IsEventChildOf(SuperType, SubType)
EndFunction
