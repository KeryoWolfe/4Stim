# Alignment

Paired animations only line up if every actor is exactly where the animation expects them. 4Stim does two things about it: it **locks** everyone in place for the whole scene, and it lets you **align** each actor by hand, per scene, from the HUD. Alignments are saved, so a scene you fix once stays fixed.

## The scene lock

Every scene has one spot, where its actors were placed, and the way they face. When the scene starts, each actor (the player too) is held on that spot plus their alignment, until the scene ends. Nothing can move them: not other NPCs walking into them, furniture, walls, slopes, gravity, or their own AI.

It works the way OStim's does: each actor is put into a "translation" (Papyrus `TranslateTo`) to their own spot, at a huge speed and next to no turning speed, and the game keeps a translating reference exactly where the translation puts it every frame until it's stopped. The scene guard checks four times a second, and anyone more than 4 units or 3 degrees off their spot is put back (`Scene lock:` in `4Stim.log`).

The spot is where the actors were placed for the scene (the player's own spot and facing when the player is in it), and every actor is turned to face the scene's way before they're locked: NPCs by their reference angle, as OStim does, since the game's heading setter only works on the player.

When the scene ends, as with OStim's "Reset position" (`SetResetPosition`, on by default), each NPC goes back to where they stood before the scene, facing the way they faced; the player isn't moved, since the scene was built on the player's own spot. That also takes everyone out of each other before their collision with each other comes back. NPCs with nowhere recorded (a scene started again from a save), or all of them with `SetResetPosition=0`, step 70 units off the spot instead, each in a different clear direction, so no one is left stuck inside someone else.

`bLockScenes=0` in `4Stim.ini` turns the lock off.

## The Align tab

In a scene, give the HUD the keys (the hotkey), then go to the **Align** tab (Left / Right on the tab rows; on a controller, B for the tab buttons):

| Row | Left / Right | Enter / A |
|---|---|---|
| **< Name (1/2) >** | the actor to adjust | the next actor |
| **Left / right** | moves them sideways (their right is +) | |
| **Back / forward** | moves them forward (+) or back | |
| **Down / up** | moves them up (+) or down | |
| **Turn** | turns them (degrees, clockwise +) | |
| **Size** | scales them, by the step / 100 (0.5 to 2) | |
| **Penis bend** | bends their penis up (+) or down, one step at a time (-9 to +9, and -10 for flaccid; below) | |
| **Step < 1 >** | the step: 0.5, 1, 2, 5 or 10 units (or degrees) | the next step |
| **Reset _name_** | | back to no offset, and the scene's own penis bend |

Hold Left / Right on a value to keep changing it. Every change moves the actor at once and is saved.

Directions are the scene's: "forward" is the way the scene faces, so an offset means the same thing wherever the scene is played.

## What's saved

As in OStim, alignments are saved per **set of actors**, scene and role, to `Documents\My Games\Fallout4\4Stim\alignment.json` (OStim keeps its `alignment.json` in `Documents\My Games\Skyrim Special Edition\OStim`), in OStim's layout:

```json
{
  "M100x0&F100x0": {
    "4StimTest_OStim2PMissionaryMF": {
      "0": { "offsetX": 0, "offsetY": -3.5, "offsetZ": 1, "rotation": 0, "scale": 1, "sosBend": 0 }
    }
  }
}
```

- **The actor set** is OStim's key: one `<sex><height>x<heels>` per role, joined by `&`. By default actors are grouped by sex only, as OStim's defaults (`alignmentGroupBySex=1`, `alignmentGroupByHeight=0`, `alignmentGroupByHeels=1` in `4Stim.ini`), so a man and a woman in missionary share one alignment, and two women in the same scene have their own. With `alignmentGroupByHeight=1`, actors of different scales get their own too (height = scale x 100). Heels are read from the High Heels System's files and mesh extra data (`SCALING.md`), and count only while an actor's heels are on (feet on the ground).
- **A transition** uses the alignment of the scene it goes to, so no one jumps when it arrives.
- **Size** is on top of the actor's own scale, and is undone when they leave the scene (also when a save made mid-scene is loaded).
- **`sosBend`** is the penis bend (below). With no alignment saved for a role, the scene's own `penisBend` is used, as OStim's default alignment.
- A scene file's own `offset` (OStim's, `SCENES.md`) is added on top, as in OStim.
- **From older 4Stim builds:** the first time this build runs with no `alignment.json` yet, it converts `Data\F4SE\Plugins\4Stim\Alignment.json` (one value for everyone): each value goes to every actor set its scene's roles can have. The old file isn't read again.

## Penis bend

As OStim's schlong bending: each role can have the penis bent up or down, in Schlongs of Skyrim's steps from -9 to +9, so it lines up with the partner. The bend comes from the Align tab (saved as `sosBend`), else the scene file (`penisBend`, or OStim's `sosBend` / `tngBend`), and an animation can add to it for a moment with the `4StimBendSoS.<steps>` annotation (OStim's `OStimBendSoS`; `4StimResetSoS` ends it, and so does the next scene).

OStim plays SoS's bend animations; Fallout 4 has none, so 4Stim turns a node of the skeleton: by default ZeX's `Penis_01_Offset`, which bends the shaft from its base. ZeX's `_Offset` nodes aren't animated, so the animation doesn't undo the bend, and A-Body's SMP genitals follow the skeleton's penis bones (rigidly with `MaleBodyHard.xml`, springing with the others). In `4Stim.ini`, `[Alignment]`:

| Setting | Default | Meaning |
|---|---|---|
| `SetSchlongBending` | 0 | 1 = no bending (OStim's setting, the same way round). |
| `fPenisBendStep` | 5 | Degrees per step. Negative if + bends it down. |
| `sPenisBendNode` | `Penis_01_Offset` | The skeleton node turned (another skeleton's, or ZeX's `Penis_Offset` to turn the whole package). |
| `iPenisBendAxis` | 2 | The node's own axis it turns around: 0 x, 1 y, 2 z (ZeX's shaft runs along x and bends around z). |

The bend goes back to none when the scene ends. Actors without the node (women, other skeletons) are left alone.

**-10 is flaccid**, as OStim's (it sends SoS's `SOSFlaccid` there): a body that can go flaccid (morphs, a bodysuit: ERECTION.md) does, and the node goes back to its own angle. A body that can't (A-Body, rigid while its physics are off for the scene) is bent as far down as it goes, -9. Erect again from -9 up.

## Still to come

- **Penis offset** (moving it rather than bending it), and bends per segment.
- Scaling and heels: see `SCALING.md`.
