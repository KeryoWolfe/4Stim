# Erections

Actors with a penis are erect during their scenes and flaccid again when they leave them. A penis bend of -10 (ALIGNMENT.md) makes them flaccid mid-scene; -9 and up, erect again.

## OStim and Fallout 4

OStim leaves erections to Schlongs of Skyrim: it plays SoS's `SOSBend<steps>` animation event for the penis bend, which also erects it, `SOSFlaccid` at a bend of -10, and `SOSFlaccid` when an actor leaves a scene (OStim NG, `ThreadActor::bendSchlong` and `Core.cpp`). It has no setting of its own for them.

Fallout 4 has no Schlongs of Skyrim. Each male body has its own way to be erect, so 4Stim works out which one each scene actor has and uses it. This is a departure from OStim, forced by Fallout 4: the events are kept (the bend, -10 flaccid, flaccid after the scene) and the means are the bodies' own.

| Body | How it gets erect | Flaccid |
|---|---|---|
| A body whose `.tri` has an `Erection` morph: **BodyTalk**, **Atomic Muscle** installed with `.tri` morphs | LooksMenu's BodyGen sets `Erection` to 1 (as AAF's morph sets do). Without the penis bend's skeleton node (no ZeX), `Erection Up` / `Erection Down` follow the bend. | The morphs are removed. |
| An **SMP** body 4Stim swaps to rigid physics: **A-Body** (PHYSICS.md) | Nothing to do: A-Body's penis is always rigid in its mesh, and `MaleBody.xml`'s physics are what make it hang. The scene's swap to `MaleBodyHard.xml` / `MaleBodyHardBottom.xml` turns them off, so it's erect for the scene; 4Stim only bends it, as for every body. | A bend of -10 bends it as far down as it goes (-9). After the scene, the swap back. |
| A separate erect body armor: **Atomic Muscle** installed with the bodysuit | The bodysuit (`AMBodyErect`) goes on once the actor is naked (nothing else in the body slot, 33), and back on if it comes off. Undressing leaves it alone. | Taken off, and away if 4Stim gave it (one the actor already had stays theirs; one they wore before the scene is left as it was). |
| Anything else | The penis bend only. | -10 is the furthest bend down (-9). |

The order: a physics swap first, then a body with the morph (the meshes' `.tri`, else the files' `triFiles`), then a bodysuit once the actor is naked. A dressed actor whose body can't be seen yet is worked out again every half second, until their clothes come off. The log says what each actor got: `Erection: 00000014's body: LooksMenu morphs`.

Morph bodies need **LooksMenu** (its `BodyGen` script). Without it, 4Stim says so once in its log (`Erection: LooksMenu's BodyGen not found`) and those bodies get the bend only.

## Files

`Data\F4SE\Plugins\4Stim\Erection\*.json`, read in name order. The single values are the last file's that has them; bodysuits and `.tri` files add up. 4Stim's own is `Default.json`:

```json
{
	"keyword": { "mod": "Fallout4.esm", "formid": "035E9B" },
	"morphs": {
		"detect": "Erection",
		"erection": "Erection",
		"up": "Erection Up",
		"down": "Erection Down",
		"value": 1.0,
		"triFiles": [ "actors\\character\\characterassets\\MaleBody.tri" ]
	},
	"bodysuits": [
		{ "id": "Atomic Muscle", "mod": "Atomic Muscle.esp", "formid": "000803", "sex": "male" }
	]
}
```

| Field | Meaning |
|---|---|
| `keyword` | The keyword the morphs are set under, so removing them leaves the actor's other morphs (LooksMenu's sliders, BodyGen's) alone. 4Stim has no plugin of its own, so it uses a test keyword of Fallout 4's that nothing else uses (`ma_TestMod`). |
| `morphs.detect` | A body whose `.tri` has a morph of this name is a morph body. |
| `morphs.erection`, `up`, `down` | The morphs set. `""` leaves one out. |
| `morphs.value` | The erection morph's value (0 to 2). |
| `morphs.triFiles` | `.tri` files (under `Meshes\`) tried when the actor's meshes name none (LooksMenu's `BODYTRI`). |
| `bodysuits` | Erect body armors: `mod` and `formid` (the plugin's own ID), `sex` (`male`, `female` or `any`). Only used when the plugin is loaded. |

## Settings

`bErections` in `4Stim.ini` `[Alignment]` and the MCM's Alignment page: 1 (default) on, 0 off (the penis bend only).

## Saves

A save made mid-scene keeps LooksMenu's morphs (LooksMenu saves them) and a bodysuit 4Stim gave. When that save is loaded, 4Stim removes both before the scene starts again (or doesn't). The physics swap is FSMP's to keep (PHYSICS.md).

## Not tested yet

Everything on this page is built, not yet seen in the game: BodyTalk 4 (the owner's `MaleBody.tri` has `Erection`, `Erection Up`, `Erection Down`), Atomic Muscle both ways, A-Body's bend at -10, a save made mid-scene.
