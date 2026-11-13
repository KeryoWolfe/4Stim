Scriptname Raknar:Raknar_Futanari_AAFWatcher extends ReferenceAlias
{Futanari FEV's AAF watcher, patched for 4Stim (4Stim's patches folder):
the penis goes erect when a 4Stim scene starts and flaccid when it ends, as
the original did on AAF's OnSceneInit / OnAnimationStop. AAF isn't needed.
Keeping it on through undressing, which the original did with AAF's
protectedEquipment file, 4Stim does by itself (docs/ACTORS.md, Futa).}

; The original's properties, unchanged: FutaFEV.esp fills them.
Actor Property FutaChar Auto
Actor property PlayerRef Auto Const
Keyword Property Raknar_Futanari_Effect Auto Const
Armor Property Raknar_Futanari_Erect Auto Const
ActorValue Property Raknar_Futanari_Size Auto const
Armor Property Raknar_Futanari_Flaccid Auto Const

ObjectMod Property Raknar_Futanari0 Auto Const
ObjectMod Property Raknar_Futanari1 Auto Const
ObjectMod Property Raknar_Futanari2 Auto Const
ObjectMod Property Raknar_Futanari3 Auto Const
ObjectMod Property Raknar_Futanari4 Auto Const
ObjectMod Property Raknar_Futanari5 Auto Const
ObjectMod Property Raknar_Futanari6 Auto Const

ObjectMod Property Raknar_FutanariE0 Auto Const
ObjectMod Property Raknar_FutanariE1 Auto Const
ObjectMod Property Raknar_FutanariE2 Auto Const
ObjectMod Property Raknar_FutanariE3 Auto Const
ObjectMod Property Raknar_FutanariE4 Auto Const
ObjectMod Property Raknar_FutanariE5 Auto Const
ObjectMod Property Raknar_FutanariE6 Auto Const

; 4Stim's scene events aren't kept in the save: registered when the alias
; starts and again on every load (this alias is the player, so it gets
; OnPlayerLoadGame; a save that already ran the AAF version registers here).
Event OnInit()
	RegisterFor4Stim()
EndEvent

Event OnPlayerLoadGame()
	RegisterFor4Stim()
EndEvent

Function RegisterFor4Stim()
	FourStim.RegisterForSceneEvents(self)
EndFunction

; 4Stim calls these on registered scripts (docs/API.md, "Scene events"):
; the player's scenes and scenes without the player.
Function FourStim_OnSceneStart(Actor[] akActors, String asSceneID)
	Erect(akActors)
EndFunction

Function FourStim_OnNPCSceneStart(Actor[] akActors, String asSceneID)
	Erect(akActors)
EndFunction

Function FourStim_OnSceneEnd(Actor[] akActors, String asSceneID)
	Flaccid(akActors)
EndFunction

Function FourStim_OnNPCSceneEnd(Actor[] akActors, String asSceneID)
	Flaccid(akActors)
EndFunction

Function Erect(Actor[] akActors)
	Int i = 0
	While i < akActors.Length
		If IsFuta(akActors[i])
			RestoreFutaErect(akActors[i])
		EndIf
		i += 1
	EndWhile
EndFunction

Function Flaccid(Actor[] akActors)
	Int i = 0
	While i < akActors.Length
		If IsFuta(akActors[i])
			RestoreFutaFlaccid(akActors[i])
		EndIf
		i += 1
	EndWhile
EndFunction

; The patch's: a futa by the mod's keyword, or by its own records of who is
; one (its list of futa NPCs, the player's futa ability), for a futa whose
; keyword is missing (the watcher found none on a futa with the penis in her
; inventory). The keyword is given back then, so the Cure works on them.
bool Function IsFuta(Actor akActor)
	if !akActor
		return false
	elseif akActor.HasKeyword(Raknar_Futanari_Effect)
		return true
	endif
	FormList futas = Game.GetFormFromFile(0x6AFB, "FutaFEV.esp") as FormList
	Spell ability = Game.GetFormFromFile(0x2CA8C, "FutaFEV.esp") as Spell
	if (futas && futas.HasForm(akActor)) || (ability && akActor.HasSpell(ability))
		akActor.AddKeyword(Raknar_Futanari_Effect)
		return true
	endif
	return false
EndFunction

; The original's, with one change: their parameter was named FutaChar, the
; same as the script's FutaChar property, and Papyrus took the property (None,
; never filled): "Cannot call GetItemCount() on a None object", and no
; erection. The original only worked because AAF's handler set the property
; first. Now akFuta.
Function RestoreFutaErect (Actor akFuta)

if (akFuta.GetItemCount(Raknar_Futanari_Erect) == 0)
	akFuta.Additem(Raknar_Futanari_Erect)
endif

int Currentsize = akFuta.GetValue(Raknar_Futanari_Size) as int
akFuta.RemoveAllModsFromInventoryItem (Raknar_Futanari_Erect)
 If(Currentsize == 0)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE0)
ElseIf (Currentsize == 1)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE1)
Elseif (Currentsize == 2)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE2)
Elseif (Currentsize == 3)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE3)
Elseif (Currentsize == 4)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE4)
Elseif (Currentsize == 5)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE5)
Elseif (Currentsize >= 6)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Erect, Raknar_FutanariE6)
EndIf
akFuta.EquipItem(Raknar_Futanari_Erect,false,true)
EndFunction

Function RestoreFutaFlaccid (Actor akFuta)

if (akFuta.GetItemCount(Raknar_Futanari_Flaccid) == 0)
	akFuta.Additem(Raknar_Futanari_Flaccid)
endif

int Currentsize = akFuta.GetValue(Raknar_Futanari_Size) as int
akFuta.RemoveAllModsFromInventoryItem (Raknar_Futanari_Flaccid)
 If(Currentsize == 0)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari0)
ElseIf (Currentsize == 1)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari1)
Elseif (Currentsize == 2)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari2)
Elseif (Currentsize == 3)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari3)
Elseif (Currentsize == 4)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari4)
Elseif (Currentsize == 5)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari5)
Elseif (Currentsize >= 6)
	akFuta.AttachModToInventoryItem (Raknar_Futanari_Flaccid, Raknar_Futanari6)
EndIf
akFuta.EquipItem(Raknar_Futanari_Flaccid,false,true)
EndFunction
