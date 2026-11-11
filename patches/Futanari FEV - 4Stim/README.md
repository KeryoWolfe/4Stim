# Futanari FEV: 4Stim patch

Futanari FEV (Raknar) makes its penis erect when an AAF scene starts and flaccid when it ends, and keeps it on through AAF's undressing. This patch does the first for 4Stim scenes, so AAF isn't needed:

- `Raknar_Futanari_AAFWatcher.psc` replaces the mod's script of that name (on `Raknar_Futanari_Quest`'s `PlayerRefAlias`). It registers for 4Stim's scene events (`FourStim.RegisterForSceneEvents`) when the alias starts and on every load, and equips the erect penis at a scene's start and the flaccid one at its end, for actors with `Raknar_Futanari_Effect`: the player's scenes and NPC scenes. The original's properties and erect / flaccid code are unchanged, so `FutaFEV.esp` needs no edit.
- `Raknar_Futanari_Make.psc` and `Raknar_Futanari_Cure.psc` replace the stim's scripts with AAF taken out: they marked the futa as male for AAF (its gender override keyword), which without AAF failed to link ("Unable to link types associated with function LoadAAF") and told you "AAF not installed, No Futasex for you." 4Stim needs no marking.
- `F4SE\Plugins\4Stim\Actor Properties\Futanari FEV.json`: an actor with the mod's `Raknar_Futanari_effect` keyword (its stim gives it) has a penis, dressed or not. Needed because the mod hides the penis while its futa are clothed (its Hider effect) and shows it only once they're naked, and 4Stim picks roles and scenes before undressing: by the body alone, a dressed futa looked like a woman and got lesbian scenes. Needs 4Stim from 10 November 2026 or later (keywords a script gave an actor count).
- `F4SE\Plugins\4Stim\Actor Properties\Futanari FEV - worn.json`: wearing the mod's penis (`Raknar_Futanari_Flaccid` or `_Erect`) means a penis too, whichever way it was put on.
- Keeping the penis on through undressing needs nothing: 4Stim sees it as the actor's penis (its materials are `Materials\Futa\penis.bgsm` and `Schlong.BGSM`, `sPenisMeshNames`) and Undress leaves it on (docs/ACTORS.md, Futa). The mod's AAF `protectedEquipment` file can stay or go.

Needs 4Stim 0.5.0 from 10 November 2026 or later (scene events from a quest alias).

Install: a mod of its own below Futanari FEV, compile the three scripts with Fallout 4's Papyrus compiler (FourStim.psc in the imports), and let the compiled `.pex` files overwrite the mod's.

The erect / flaccid swap is for actors Futanari FEV made futa itself (its stim: the `Raknar_Futanari_Effect` keyword and its own penis items), as in the original. Another mod's wearable penis (Bag of Dicks' `[Schlong]` items) has no such mechanics: 4Stim still sees it and keeps it on, but nothing swaps it.

Behaviour difference: the original swapped the player for AAF's third argument (`akArgs[2]`) when the player was the futa; this patch acts on the player as on anyone else.
