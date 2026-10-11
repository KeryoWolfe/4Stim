# Actor scaling and heels

Paired animations are made for actors of one size. As in OStim, 4Stim scales everyone in a scene to the size the animation was made for, and handles heels, so a tall NPC, a short one and someone in heels all line up.

## Scaling

When a scene starts, and whenever it moves to another scene, each actor is scaled to:

> the role's `scale` (scene file, 1 by default) × their alignment's scale (the Align tab) ÷ their own NPC height

So an NPC whose record makes them 5% taller is scaled down 5% for the scene, and everyone plays the animation at its own size. When they leave the scene, their own scale comes back. This is OStim's `ThreadActor::scale`.

`SetScaling=1` in `4Stim.ini` turns this off, as OStim's setting of that name does: actors then keep their own scale, times their alignment's.

The NPC height is the `height` on their NPC record. OStim also divides by the RaceMenu height; Fallout 4 has no equivalent read here yet (LooksMenu body scaling isn't taken into account).

## Heels

An actor's heel height is read from what they wear, the way **Fallout 4's High Heels System** (HHS) defines heights. The highest one among their worn armor counts, from the first of these that has one:

1. HHS `.json` files in `Data\F4SE\Plugins\HHS\`: an armor (or armor addon) by plugin and form ID, for one sex or both (`{"MyShoes.esp": [{"formid": "00800", "gender": 1, "value": 10}]}`), or a mesh by its path under `Meshes\` (`{"MyShoes": [{"key": "MyShoes\\MyShoes.nif", "value": 10}]}`).
2. An HHS `.txt` next to the shoe's mesh (`Data\Meshes\MyArmor\MyArmor.nif` -> `MyArmor.txt`), or with the mesh's file name in `Data\F4SE\Plugins\HHS\`, holding `Height=10.0`.

Heights set in HHS's MCM, or written into the mesh as extra data, aren't read. Heels are read again on every scene change, so taking shoes off mid-scene is noticed at the next one. The heights found are logged (`Heels:` in `4Stim.log`). The `.json` files are read once per game session (again with `FourStim.ReloadScenes()`).

As OStim does with its heel offsets:

- **Feet on the ground** (the role is tagged `standing` or `squatting`, or the scene file says `"feetOnGround": true`): the heels stay on, and the actor is scaled down so body and heels together match the animation: × `scaleHeight` ÷ (`scaleHeight` + heel height). `scaleHeight` is the height the animation was made for, 120.748 units by default (OStim's), settable per role.
- **Feet off the ground** (lying, kneeling, sitting...): the heels come off. OStim moves the actor's skeleton down by the heel height; 4Stim lowers their spot by it, which has the same effect while the scene lock holds them.

Alignments are kept separately for actors in heels (`alignmentGroupByHeels=1`, OStim's default; `ALIGNMENT.md`), only while their heels are on.

## Scene file fields

On an actor (`SCENES.md`), all OStim's:

| Field | Default | Meaning |
|---|---|---|
| `scale` | 1 | The scale this role is played at (after evening out their height). |
| `scaleHeight` | 120.748 | The height the animation was made for, for heels. |
| `feetOnGround` | `standing` / `squatting` tag | Whether heels stay on in this role. |

## Not yet

- Heights from HHS's MCM and from extra data in meshes, and from other heel mods (e.g. ones set by script properties on the armor).
- The furniture type's `multiplyScale` (OStim's furniture scale).
- LooksMenu height.
