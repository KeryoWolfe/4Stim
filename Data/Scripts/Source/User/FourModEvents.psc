Scriptname FourModEvents Native Hidden
{Mod events for Fallout 4 (docs\API.md, "Mod events"): SKSE's RegisterForModEvent
and SendModEvent, which OStim NG sends all its events through, kept by 4Stim's
plugin. 4Stim's events are OStim's with "ostim_" renamed "4stim_" and the same
arguments: 4stim_thread_start, 4stim_thread_scenechanged, 4stim_thread_end...}

; Receiver's scripts get Callback whenever EventName is sent (any case).
; Registering the same event again replaces the callback. The callback is a
; Function (Fallout 4 only allows the game's own events as Events) with OStim's
; four arguments:
;
;   Function OnThreadStart(string asEventName, string asStrArg, float afNumArg, Form akSender)
;
; except "4stim_event", which has OStim's ostim_event arguments:
;
;   Function OnEvent(int aiThreadID, string asType, Form akActor, Form akTarget, Form akPerformer)
;
; Every script attached to Receiver that has the function gets the call. As
; SKSE's, registrations last until a game is loaded: register again after each
; load (for example from the player's OnPlayerLoadGame, through
; RegisterForRemoteEvent). Receiver is any script (usually Self): one on a
; Quest, an ObjectReference or another form, or, as SKSE's, on a quest alias
; or an active magic effect.
bool Function RegisterForModEvent(ScriptObject Receiver, string EventName, string Callback) Global Native

; Receiver stops getting EventName.
Function UnregisterForModEvent(ScriptObject Receiver, string EventName) Global Native

; Receiver stops getting every mod event.
Function UnregisterForAllModEvents(ScriptObject Receiver) Global Native

; Sends EventName to everyone registered for it, as SKSE's SendModEvent: for
; 4Stim's add-ons to tell each other things. Sender may be None.
Function SendModEvent(string EventName, string StrArg = "", float NumArg = 0.0, Form Sender = None) Global Native
