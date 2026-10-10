# Alignment

Paired animations only line up if every actor is exactly where the animation expects them. 4Stim does two things about it: it **locks** everyone in place for the whole scene, and it lets you **align** each actor by hand, per scene, from the HUD. Alignments are saved, so a scene you fix once stays fixed.

## The scene lock

Every scene has one spot, where its actors were placed, and the way they face. When the scene starts, each actor (the player too) is held on that spot plus their alignment, until the scene ends. Nothing can move them: not other NPCs walking into them, furniture, walls, slopes, gravity, or their own AI.

It works the way OStim's does: each actor is put into a "translation" (Papyrus `TranslateTo`) to their own spot, at a huge speed and next to no turning speed, and the game keeps a translating reference exactly where the translation puts it every frame until it's stopped. The scene guard checks four times a second, and anyone more than 4 units or 3 degrees off their spot is put back (`Scene lock:` in `4Stim.log`).

The spot is where the actors were placed for the scene (the player's own spot and facing when the player is in it), and every actor is turned to face the scene's way before they're locked: NPCs by their reference angle, as OStim does, since the game's heading setter only works on the player.

When the scene ends, everyone but one (the player, else the first role) steps 70 units off the spot, each in a different clear direction, before their collision with each other comes back, so no one is left stuck inside someone else.

`bLockScenes=0` in `4Stim.ini` turns the lock off.

## The Align tab

In a scene, give the HUD the keys (the hotkey), then go to the **Align** tab (Left / Right on the tab rows, or LB / RB):

| Row | Left / Right | Enter / A |
|---|---|---|
| **< Name (1/2) >** | the actor to adjust | the next actor |
| **Left / right** | moves them sideways (their right is +) | |
| **Back / forward** | moves them forward (+) or back | |
| **Down / up** | moves them up (+) or down | |
| **Turn** | turns them (degrees, clockwise +) | |
| **Size** | scales them, by the step / 100 (0.5 to 2) | |
| **Step < 1 >** | the step: 0.5, 1, 2, 5 or 10 units (or degrees) | the next step |
| **Reset _name_** | | back to no offset |

Hold Left / Right on a value to keep changing it. Every change moves the actor at once and is saved.

Directions are the scene's: "forward" is the way the scene faces, so an offset means the same thing wherever the scene is played.

## What's saved

`Data\F4SE\Plugins\4Stim\Alignment.json` (with Mod Organizer, it lands in `overwrite`), per scene id and role:

```json
{
	"scenes": {
		"4stimtest_ostim2pmissionarymf": {
			"0": { "x": 0, "y": -3.5, "z": 1, "rot": 0, "scale": 1 }
		}
	}
}
```

- An alignment belongs to a **scene and role**, for everyone who plays it.
- A **transition** uses the alignment of the scene it goes to, so no one jumps when it arrives.
- **Size** is on top of the actor's own scale, and is undone when they leave the scene (also when a save made mid-scene is loaded).
- The file can be shipped with an animation pack, or shared: it's plain JSON, re-read with `FourStim.ReloadScenes()`.

## Still to come

- **Penis bend** per scene and role (OStim's `sosBend`, `penisBend` in 4Stim), once bendable bodies are supported.
- Alignments **keyed by body or height** (OStim keys them by the actors' heights and heels), so A-Body / TWB characters and CBBE ones can have different values; with Automatic Mesh Detection (TODO, Later).
- Actor scaling to the animation's intended height, and heel compensation.
