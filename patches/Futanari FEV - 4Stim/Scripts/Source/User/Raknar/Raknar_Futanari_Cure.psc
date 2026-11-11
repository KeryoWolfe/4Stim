Scriptname Raknar:Raknar_Futanari_Cure extends activemagiceffect
{Futanari FEV's, patched for 4Stim (4Stim's patches folder): AAF taken out.
The original marked the actor as male for AAF here (its gender override
keyword), which failed to link without AAF ("Unable to link types
associated with function LoadAAF"). 4Stim needs no marking: it sees the
penis on the body (docs/ACTORS.md, Futa). Otherwise unchanged.}


Armor Property Raknar_Futanari_Flaccid Auto Const
Armor Property Raknar_Futanari_Erect Auto Const

Keyword Property Raknar_Futanari_Effect Auto Const
Spell Property Raknar_Futanari_Spell Auto Const
Actor Property PlayerRef Auto Const
Formlist Property Raknar_Futanari_list Auto

Event OnEffectStart(Actor FutaChar, Actor akCaster)
If (FutaChar.HasKeyword(Raknar_Futanari_Effect))
;Debug.MessageBox("I feel a intense burning around my groin... What is going on? I should take off my clothes and see.")
 	FutaChar.RemoveKeyword(Raknar_Futanari_Effect)
	FutaChar.RemoveItem(Raknar_Futanari_Flaccid, 1, True)
	FutaChar.RemoveItem(Raknar_Futanari_Erect, 1, True)
	FutaChar.RemoveSpell(Raknar_Futanari_Spell)
	If(FutaChar != PlayerRef)
	Raknar_Futanari_list.RemoveAddedform(FutaChar as Form)
	endif
	Utility.wait(0.5)
	Debug.notification("Target cured of Futanari")

Elseif(Futachar.HasKeyword(Raknar_Futanari_Effect) != True)
		Debug.Notification("Not mutated.. that Stim won't have any further effect.")
EndIf
endevent
