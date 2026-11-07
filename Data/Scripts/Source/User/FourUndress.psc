;/* FourUndress (OStim's OUndress)
* * Papyrus undressing: an add-on that wants to do all undressing and
* * dressing itself overrides this script (its own copy of FourUndress.pex)
* * and makes UsePapyrusUndressing return true. 4Stim then calls these
* * functions instead of undressing anyone (docs/UNDRESS.md).
* *
* * Departures from OStim's OUndress, forced by Fallout 4: actors are passed
* * by form ID (an Actor passed from the plugin doesn't arrive in Papyrus),
* * slot masks are Fallout 4's (bit 0 = slot 30), there's no thread ID, and
* * 4Stim doesn't wait for a return value: the add-on keeps track of what
* * it took off. OStim's AnimateRedress has no counterpart (4Stim plays its
* * own redress animations, bAnimateRedress).
*/;
ScriptName FourUndress

;/* UsePapyrusUndressing
* * true: 4Stim calls the functions below instead of undressing anyone.
* * Asked at every game load (as OStim's), not saved.
*/;
bool Function UsePapyrusUndressing() Global
	return false
EndFunction

;/* UndressPartial
* * Take off what aiActorID wears in aiSlotMask's slots (Fallout 4's slots,
* * bit 0 = slot 30). Called as a scene starts or its actions need it, and
* * by OActor-style UndressPartial calls.
*/;
Function UndressPartial(int aiActorID, int aiSlotMask) Global
EndFunction

;/* RedressPartial
* * Put back what was taken off in aiSlotMask's slots.
*/;
Function RedressPartial(int aiActorID, int aiSlotMask) Global
EndFunction

;/* Redress
* * Put back everything that was taken off (a scene ended, or redress was
* * asked for).
*/;
Function Redress(int aiActorID) Global
EndFunction
