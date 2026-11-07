# Alignment

Paired animations only line up if every actor is exactly where the animation expects them. 4Stim does two things about it: it **locks** everyone in place for the whole scene, and it lets you **align** each actor by hand, per scene, from the HUD. Alignments are saved, so a scene you fix once stays fixed.

## The scene lock

Every scene has one spot, where its actors were placed, and the way they face. When the scene starts, each actor (the player too) is held on that spot plus their alignment, until the scene ends. Nothing can move them: not other NPCs walking into them, furniture, walls, slopes, gravity, or their own AI.

It works the way OStim's does: each actor is put into a "translation" (Papyrus `TranslateTo`) to their own spot, at a huge speed and next to no turning speed, and the game keeps a translating reference exactly where the translation puts it every frame until it's stopped. The scene guard checks four times a second, and anyone more than 4 units or 3 degrees off their spot is put back (`Scene lock:` in `4Stim.log`).

The spot is where the actors were placed for the scene (the player's own spot and facing when the player is in it), and every actor is turned to face the scene's way before they're locked: NPCs by their reference angle, as OStim does, since the game's heading setter only works on the player.

When the scene ends, as with OStim's "Reset position" (`SetResetPosition`, on by default), each NPC goes back to where they stood before the scene, facing the way they faced. The player goes back too, as OStim's, when the scene moved them onto furniture (a bed, a table, a wall): to where they stood before, facing the way they faced. A scene off furniture is built on the player's own spot, so there's nothing to undo. That also takes everyone out of each other before their collision with each other comes back. NPCs with nowhere recorded (a scene started again from a save), or all of them with `SetResetPosition=0`, step 70 units off the spot instead, each in a different clear direction, so no one is left stuck inside someone else.

`bLockScenes=0` in `4Stim.ini` turns the lock off.

## The Align tab

In a scene, give the HUD the keys (the hotkey), then go to the **Align** tab (Left / Right on the tab rows; on a controller, B for the tab buttons):

| Row | Left / Right | Enter / A |
|---|---|---|
| **< Name >  Role 0/1  F** | the actor to adjust (role, and sex: M, F or *) | the next actor |
| **Left / right** | moves them sideways (their right is +) | |
| **Back / forward** | moves them forward (+) or back | |
| **Down / up** | moves them up (+) or down | |
| **Size** | scales them by the step (0.1 to 10) | |
| **Turn** | turns them (degrees, clockwise +) | |
| **Penis bend** | bends their penis up (+) or down, one step at a time (-9 to +9, and -10 for flaccid; below) | |
| **Step < 1.000 >** | the step, as OStim's: 10, 1, 0.1, 0.01 or 0.001 units (or degrees, or times the size) | the next step |
| **Reset _name_** | | back to no offset, and the scene's own penis bend |

The rows are in OStim's align menu's order, and as there the step is the same for every value, the size too (4Stim used to move the size by a hundredth of it). Above the rows, OStim's readout: the scene's id and the actor set's alignment key (the key in `alignment.json`, below), with the scene's pack at the header's end.

OStim's keys work too: **Toggle** (numpad 9) picks the next actor, and the **pull-out key** takes the next step size (OStim's No key in its align menu; elsewhere it still pulls out). Hold Left / Right on a value to keep changing it. Every change moves the actor at once and is saved.

Directions are the scene's: "forward" is the way the scene faces, so an offset means the same thing wherever the scene is played.

## What's saved

As in OStim, alignments are saved per **set of actors**, scene and role, to `Documents\My Games\Fallout4\4Stim\alignment.json` (OStim keeps its `alignment.json` in `Documents\My Games\Skyrim Special Edition\OStim`), in OStim's layout:

```json
{
  "M100x0&F100x0": {
    "OStim2PMissionaryMF": {
      "0": { "offsetX": 0, "offsetY": -3.5, "offsetZ": 1, "rotation": 0, "scale": 1, "sosBend": 0 }
    }
  }
}
```

- **The actor set** is OStim's key: one `<sex><height>x<heels>` per role, joined by `&`. By default actors are grouped by sex only, as OStim's defaults (`alignmentGroupBySex=1`, `alignmentGroupByHeight=0`, `alignmentGroupByHeels=1` in `4Stim.ini`), so a man and a woman in missionary share one alignment, and two women in the same scene have their own. With `alignmentGroupByHeight=1`, actors of different scales get their own too (height = scale x 100). Heels are read from the High Heels System's files and mesh extra data (`SCALING.md`), and count only while an actor's heels are on (feet on the ground).
- **A transition** uses the alignment and per-role values (offset, scale, heels, look direction, penis bend) of the scene it goes to, so no one jumps when it starts or arrives. OStim uses the transition's own; in Fallout 4 that shows as a bounce at both ends of every transition, so 4Stim keeps the destination's.
- **Size** is on top of the actor's own scale, and is undone when they leave the scene (also when a save made mid-scene is loaded).
- **`sosBend`** is the penis bend (below). With no alignment saved for a role, the scene's own `penisBend` is used, as OStim's default alignment.
- A scene file's own `offset` (OStim's, `SCENES.md`) is added on top, as in OStim.
- **From older 4Stim builds:** the first time this build runs with no `alignment.json` yet, it converts `Data\F4SE\Plugins\4Stim\Alignment.json` (one value for everyone): each value goes to every actor set its scene's roles can have. The old file isn't read again.

## Penis bend

As OStim's schlong bending: each role can have the penis bent up or down, in Schlongs of Skyrim's steps from -9 to +9, so it lines up with the partner. The bend comes from the Align tab (saved as `sosBend`), else the scene file (`penisBend`, or OStim's `sosBend` / `tngBend`), and an animation can add to it for a moment with the `4StimBendSoS.<steps>` annotation (OStim's `OStimBendSoS`; `4StimResetSoS` ends it, and so does the next scene).

**Aimed animations.** AnimBridge can aim the penis when it converts an OStim pack: for every frame of a sex action it bends the whole `Penis_00` to `Penis_05` chain toward the partner's target (`Vagina_00`, `Anus_00`, or the mouth: `Tongue_02`), most at the base and a little at each joint, as TNG's poses curve the shaft. Those animations carry the `4StimPenisAim` annotation, and AnimBridge leaves out their `OStimBendSoS` / `OStimResetSoS` ones. On such an animation the scene file's bend (`penisBend`, `sosBend`, `tngBend`) is left out, since it was written for SoS's straight schlong and would only bend the aimed shaft off target; a bend you set in the Align tab still counts, as do `4StimBendSoS` annotations. It lasts until the next scene.

OStim plays SoS's bend animations (`SOSBend<n>`); TNG answers them with its own poses, `TngBendUp` and `TngBendDown` blended with `TngErect`, which curve the whole shaft rather than hinge it at the base. Read from TNG's clips: up, `Genitals01` 43 degrees, `02` 9, `03` 9, `04` 4; down, `01` 53, `02` 3, `04` 3.5, `05` 4. Fallout 4 has neither, so 4Stim turns ZeX's matching segment pivots, `Penis_01_Offset` to `Penis_05_Offset`, each its share of the bend (TNG's proportions). As in SoS's and TNG's behavior graphs, step n is n/9 of the full pose (their bend clips are additive, blended at weights 0.111 to 1.0). Schlongs of Skyrim's own poses, if you'd rather have them: up 50 degrees at `Genitals01` and 5 on each of `02`-`06`, down 55 and 5 each, flaccid 60/10/5/5/5; as shares over ZeX's five pivots, `sPenisBendUp=0.667,0.067,0.067,0.067,0.133` and `sPenisBendDown=0.688,0.063,0.063,0.063,0.125` (the tip's 5 goes on the last pivot), with `fPenisBendStep=8.5`. ZeX's `_Offset` nodes aren't animated, so the animation doesn't undo the bend, and A-Body's SMP genitals follow the skeleton's penis bones (rigidly with `MaleBodyHard.xml`, springing with the others). In `4Stim.ini`, `[Alignment]`:

| Setting | Default | Meaning |
|---|---|---|
| `SetSchlongBending` | 0 | 1 = no bending (OStim's setting, the same way round). |
| `fPenisBendStep` | 7.2 | The whole bend per step, in degrees (TNG's full bend is about 65 degrees at 9). Negative if + bends it down. |
| `sPenisBendNode` | `Penis_01_Offset` to `Penis_05_Offset` | The skeleton nodes turned, base first, as a comma list (another skeleton's; one node, such as ZeX's `Penis_Offset` for the whole package, takes the whole bend). |
| `sPenisBendUp` / `sPenisBendDown` | `0.657,0.139,0.136,0.067,0` / `0.834,0.049,0,0.055,0.062` | Each node's share of a bend up / down. |
| `iPenisBendAxis` | 2 | The node's own axis it turns around: 0 x, 1 y, 2 z (ZeX's shaft runs along x and bends around z). |

The bend goes back to none when the scene ends. Actors without the node (women, other skeletons) are left alone.

**-10 is flaccid**, as OStim's (it sends SoS's `SOSFlaccid` there): a body that can go flaccid (morphs, a bodysuit: ERECTION.md) does, and the node goes back to its own angle. A body that can't (A-Body, rigid while its physics are off for the scene) takes TNG's flaccid pose instead: 50 degrees down at the base and 15 at the next segment (`sPenisFlaccid`). Erect again from -9 up.

## Still to come

- **Penis offset** (moving it rather than bending it), and bends per segment.
- Scaling and heels: see `SCALING.md`.
