Scriptname Raknar:Raknar_Futanari_Make extends activemagiceffect
{Futanari FEV's, patched for 4Stim (4Stim's patches folder): AAF taken out.
The original marked the actor as male for AAF here (its gender override
keyword), which failed to link without AAF ("Unable to link types
associated with function LoadAAF"). 4Stim needs no marking: it sees the
penis on the body (docs/ACTORS.md, Futa). Otherwise unchanged.}


Armor Property Raknar_Futanari_Flaccid Auto Const
Armor Property Raknar_Futanari_Erect Auto Const

ObjectMod Property Raknar_Futanari0 Auto Const
ObjectMod Property Raknar_FutanariE0 Auto Const
Keyword Property Raknar_Futanari_Effect Auto Const
Spell Property Raknar_Futanari_Spell Auto Const
Formlist Property Raknar_Futanari_list Auto
Actor Property PlayerRef Auto Const

Event OnEffectStart(Actor FutaChar, Actor akCaster)
;Debug.notification("Magic effect was started on " + FutaChar)
If (FutaChar.HasKeyword(Raknar_Futanari_Effect))
	Debug.Notification("That Stim won't have any effect.")
Elseif(Futachar.HasKeyword(Raknar_Futanari_Effect) != True)
	;Debug.MessageBox("I feel a intense burning around my groin... What is going on? I should take off my clothes and see.")
 	FutaChar.AddKeyword(Raknar_Futanari_Effect)
	FutaChar.AddItem(Raknar_Futanari_Flaccid, 1, True)
	FutaChar.AddItem(Raknar_Futanari_Erect, 1, True)
	Utility.wait(0.5)
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari0) 
	FutaChar.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE0)
	FutaChar.Unequipitemslot(3)
	FutaChar.EquipItem(Raknar_Futanari_Flaccid,false,true)
	If(FutaChar == PlayerRef)
	FutaChar.AddSpell(Raknar_Futanari_Spell, False)
	else
	Raknar_Futanari_list.Addform(FutaChar as Form)
	endif
	Debug.notification("Target made Futanari")

EndIf
endevent
