Scriptname FourJSON Native Hidden
{OStim NG's OJSON for 4Stim (docs\API.md, "Mod events"): reads the JSON the end
events (4stim_thread_end, 4stim_end) carry in their string argument.}

; The actors that were in the thread, in role order (None where one no longer
; exists).
Actor[] Function GetActors(string Json) Global Native

; The scene the thread was in when it ended.
string Function GetScene(string Json) Global Native

; The metadata the thread had.
string[] Function GetMetadata(string Json) Global Native
