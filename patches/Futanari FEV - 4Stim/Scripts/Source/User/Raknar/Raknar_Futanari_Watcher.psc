Scriptname Raknar:Raknar_Futanari_Watcher extends ReferenceAlias
{Futanari FEV's polling watcher, patched for 4Stim (4Stim's patches folder);
nothing here is about 4Stim, but its errors filled the Papyrus log every 4
seconds. Two changes: futas whose 3D isn't loaded aren't cast on ("Ref is in
an unloaded cell, so it cannot cast spells"), and Raknar_Futanari_Polling_Total,
which FutaFEV.esp leaves empty, is only set when it's there ("Cannot call
GetValue() on a None object"). Otherwise unchanged.}

Formlist Property Raknar_Futanari_list Auto
Actor Property FutaChar Auto
Actor property PlayerRef Auto Const
Spell Property Raknar_Futanari_SpellNPC Auto Const
GlobalVariable Property Raknar_Futanari_Polling_Rate auto const
GlobalVariable Property Raknar_Futanari_Polling_Total Auto

Event OnInit()
StartTimer(Raknar_Futanari_Polling_Rate.getValue())
EndEvent

Event OnTimer(int aiTimerID)
;debug.Notification("pinging " + (Raknar_Futanari_list.Getsize()))
Int iIndex = 0
		While (iIndex < Raknar_Futanari_list.Getsize())
			Futachar = Raknar_Futanari_list.GetAt(iIndex) As Actor
			If (FutaChar && FutaChar.Is3DLoaded())
			 Raknar_Futanari_SpellNPC.cast(FutaChar)
			EndIf
			 ;debug.notification("Spell cast at " + FutaChar )
		iIndex += 1
		EndWhile
		if(Raknar_Futanari_Polling_Total && Raknar_Futanari_list.Getsize() != Raknar_Futanari_Polling_Total.getvalue() )
		Raknar_Futanari_Polling_Total.setvalue( Raknar_Futanari_list.Getsize())
		endif
StartTimer(4)
EndEvent
