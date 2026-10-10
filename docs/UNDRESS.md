# Undressing

Scene actors take off their clothes and armor during a scene and put them back on when it ends, like in OStim. The settings are in the `[Undress]` section of `4Stim.ini`.

## When

- **Undress at start** (`bUndressAtStart`, off by default): everything comes off when a scene starts.
- **Partial** (`bPartialUndress`): each time the scene moves to another scene, each actor takes off what that scene's actions need for their side. A kiss takes the hat off, a handjob the giver's gloves (`undressSlots` on the action type, `ACTIONS.md`).
- **Full** (`bFullUndressMidScene`): actions marked `fullStrip`, such as intercourse, oral sex or a handjob's receiver, take everything off.
- **Never** in a scene marked `"noStrip": true` (`SCENES.md`).
- **Redress** (`bRedress`): when the scene ends, each actor puts back on what they took off.
- **By hand:** the HUD's Utility tab has **Undress _name_** / **Dress _name_** for each actor in the scene (shown while `bUndress` is on). Undressing by hand takes off everything in the undress slots, the player too even with `bUndressPlayer=0`; dressing puts back on everything 4Stim took off.

Weapons are always put away when a scene starts.

## One piece at a time

Clothes come off and go back on one item at a time, `fUndressItemDelay` seconds apart (0.3 by default; 0 = all at once).

## Animated undressing and redressing

- **Undressing scenes:** a scene can take clothes off at a set moment of its animation with an `undress` list (`SCENES.md`): a partner pulling someone's top off takes the top off as the hands get there. The test pack has OStim's: from **Standing apart** (MF), "take off _her_ top / bottoms / gloves / hat / boots" and "_she_ takes off _his_ bottoms", each playing once and going back to standing apart.
- **Redress animations** (`bAnimateRedress`, on by default): when a scene ends, NPCs get dressed a body part at a time, torso, feet, hands, then head, each with its own animation, and that part's clothes go on partway through it, like OStim. The animations are one-actor scenes tagged `redress` plus the part (`torso`, `feet`, `hands`, `head`) for the actor's sex, with `length` and `dressAt` (`SCENES.md`); the test pack has OStim's. Anything that isn't in one of those parts goes on afterwards. The player dresses without them.

The slots each part covers:

| Part | Slots |
|---|---|
| torso | 33, 36–38, 41–43, 50 |
| feet | 39, 40, 44, 45 (Fallout 4 has no feet slot: shoes are part of the legs) |
| hands | 34, 35 |
| head | 46, 47 |

## What

Only what's worn in the slots listed in `sUndressSlots` comes off. These are Fallout 4's biped slots, 30 to 61:

| Slots | |
|---|---|
| 33 | Body: outfits, jumpsuits |
| 34, 35 | Left and right hand |
| 36–40 | Under-armor: torso, arms, legs |
| 41–45 | Armor pieces: torso, arms, legs |
| 46 | Headband: hats, helmets |
| 47 | Eyes: glasses, goggles |
| 50 | Neck: scarves, bandanas |

Hair (30, 31), the head (32), mouth (49), scalp (52), the accessory slots (54–58) and the Pip-Boy (60) are left alone by default. Body mods use some of these for parts of the body itself. Add any slot your outfits use to the list.

The action types' `undressSlots` were mapped from OStim's Skyrim `strippingSlots`:

| Skyrim slot | Fallout 4 slots |
|---|---|
| head / hair / circlet | 46 |
| body | 33, 36, 39–41, 44, 45 |
| hands | 34, 35 |
| forearms | 37, 38, 42, 43 |
| amulet / neck | 50 |
| ring | 51 |
| face | 49 |
| pelvis | 33, 36, 39, 40, 44, 45 |

Any slot outside `sUndressSlots` is skipped anyway.

## Papyrus

- `FourStim.UndressActor(akActor)` takes everything in the undress slots off an actor (like the HUD's Undress, so the player too).
- `FourStim.RedressActor(akActor)` puts back on what 4Stim took off.

What's been taken off is kept in the save with the scene: a scene that starts again on load still dresses everyone at the end, and one that doesn't dresses them on load (`bResumeScenes`, `4Stim.ini`).

The plugin finds what each actor wears in which slots; `FourStimUndress.psc` (vanilla Papyrus) takes it off and puts it back on.
