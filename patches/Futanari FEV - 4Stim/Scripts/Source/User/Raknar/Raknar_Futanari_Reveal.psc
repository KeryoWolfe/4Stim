Scriptname Raknar:Raknar_Futanari_Reveal extends activemagiceffect
{Futanari FEV's, patched for 4Stim (4Stim's patches folder): does nothing for
an actor without Raknar_Futanari_Effect. The mod casts its reveal spell on
every futa every 4 seconds; one still on its way when the Cure ran started
after it and gave the penis back ("Target cured", then the penis returned),
because the original adds the penis when the actor has none. Otherwise
unchanged.}

Armor Property Raknar_Futanari_Flaccid Auto Const
Armor Property Raknar_Futanari_Erect Auto Const

ObjectMod Property Raknar_Futanari0 Auto Const
ObjectMod Property Raknar_Futanari1 Auto Const
ObjectMod Property Raknar_Futanari2 Auto Const
ObjectMod Property Raknar_Futanari3 Auto Const
ObjectMod Property Raknar_Futanari4 Auto Const
ObjectMod Property Raknar_Futanari5 Auto Const
ObjectMod Property Raknar_Futanari6 Auto Const

ActorValue Property Raknar_Futanari_Size Auto const

; Added by the patch: filled from FutaFEV.esp when the property is empty
; (the plugin has no value for it, as the original script had none).
Keyword Property Raknar_Futanari_Effect Auto Const

Event OnEffectStart(Actor FutaChar, Actor akCaster)
if !FutaChar || !FutaChar.HasKeyword(GetFutaEffect())
	return ; cured (or never made futa): nothing to reveal
endif
if (Futachar.GetItemCount(Raknar_Futanari_Flaccid) >= 1)
	FutaChar.EquipItem(Raknar_Futanari_Flaccid,false,true)
	;Debug.Notification("My dick flops out.")
	else
	RestoreFutaFlaccid(FutaChar)
EndIf
endevent

Function RestoreFutaFlaccid (Actor FutaChar)

if (Futachar.GetItemCount(Raknar_Futanari_Flaccid) == 0)
	Futachar.Additem(Raknar_Futanari_Flaccid)
endif

int Currentsize = FutaChar.GetValue(Raknar_Futanari_Size) as int
FutaChar.RemoveAllModsFromInventoryItem (Raknar_Futanari_Flaccid)
 If(Currentsize == 0)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari0) 
ElseIf (Currentsize == 1)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari1) 
Elseif (Currentsize == 2)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari2) 
Elseif (Currentsize == 3)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari3) 
Elseif (Currentsize == 4)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari4) 
Elseif (Currentsize == 5)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari5) 
Elseif (Currentsize >= 6)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari6) 
EndIf
FutaChar.EquipItem(Raknar_Futanari_Flaccid,false,true)
EndFunction

; The patch's: the keyword, from the property if the plugin filled it, else
; from FutaFEV.esp (0x22AF5, Raknar_Futanari_effect).
Keyword Function GetFutaEffect()
	if Raknar_Futanari_Effect
		return Raknar_Futanari_Effect
	endif
	return Game.GetFormFromFile(0x22AF5, "FutaFEV.esp") as Keyword
EndFunction
