# Futanari FEV: 4Stim patch

Futanari FEV (Raknar) makes its penis erect when an AAF scene starts and flaccid when it ends, and keeps it on through AAF's undressing. This patch does the first for 4Stim scenes, so AAF isn't needed:

- `Raknar_Futanari_AAFWatcher.psc` replaces the mod's script of that name (on `Raknar_Futanari_Quest`'s `PlayerRefAlias`). It registers for 4Stim's scene events (`FourStim.RegisterForSceneEvents`) when the alias starts and on every load, and equips the erect penis at a scene's start and the flaccid one at its end, for actors with `Raknar_Futanari_Effect`: the player's scenes and NPC scenes. The original's properties and erect / flaccid code are unchanged, so `FutaFEV.esp` needs no edit.
- Keeping the penis on through undressing needs nothing: 4Stim sees it as the actor's penis (its meshes are named "Schlong" and "Penis", `sPenisMeshNames`) and Undress leaves it on (docs/ACTORS.md, Futa). The mod's AAF `protectedEquipment` file can stay or go.

Needs 4Stim 0.5.0 from 10 November 2026 or later (scene events from a quest alias).

Install: a mod of its own below Futanari FEV, compile the script with Fallout 4's Papyrus compiler (FourStim.psc in the imports), and let the compiled `Raknar\Raknar_Futanari_AAFWatcher.pex` overwrite the mod's.

Behaviour difference: the original swapped the player for AAF's third argument (`akArgs[2]`) when the player was the futa; this patch acts on the player as on anyone else.
