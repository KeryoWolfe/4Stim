# Actor scaling and heels

Paired animations are made for actors of one size. As in OStim, 4Stim scales everyone in a scene to the size the animation was made for, and handles heels, so a tall NPC, a short one and someone in heels all line up.

## Scaling

When a scene starts, and whenever it moves to another scene, each actor is scaled to:

> the role's `scale` (scene file, 1 by default) × their alignment's scale (the Align tab) ÷ their own NPC height

So an NPC whose record makes them 5% taller is scaled down 5% for the scene, and everyone plays the animation at its own size. When they leave the scene, their own scale comes back. This is OStim's `ThreadActor::scale`.

`SetScaling=1` in `4Stim.ini` turns this off, as OStim's setting of that name does: actors then keep their own scale, times their alignment's.

The NPC height is the `height` on their NPC record. OStim also divides by the RaceMenu height; Fallout 4 has no equivalent read here yet (LooksMenu body scaling isn't taken into account).

## Heels

An actor's heel height is read from what they wear, from **Fallout 4's High Heels System** files: a `.txt` next to the shoe's mesh (`Data\Meshes\...\Shoes.nif` → `Shoes.txt`) with `Height=<units>`. The highest one among their worn armor counts. Heels are read again on every scene change, so taking shoes off mid-scene is noticed at the next one. The heights found are logged (`Heels:` in `4Stim.log`).

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

- Heel heights from other heel mods (e.g. ones set by script properties on the armor), and Skyrim-style `HH_OFFSET` data in meshes.
- The furniture type's `multiplyScale` (OStim's furniture scale).
- LooksMenu height.
