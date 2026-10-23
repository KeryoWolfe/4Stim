# Undressing

Scene actors take off their clothes and armor during a scene and put them back on when it ends, like in OStim. The settings are in the `[Undress]` section of `4Stim.ini`.

## When

- **Undress at start** (`SetAlwaysUndressAtStart`, off by default): everything comes off when a scene starts.
- **Partial** (`SetPartialUndressing`): each time the scene moves to another scene, each actor takes off what that scene's actions need for their side. A kiss takes the hat off, a handjob the giver's gloves (`undressSlots` on the action type, `ACTIONS.md`).
- **Full** (`SetUndressIfNeed`): actions marked `fullStrip`, such as intercourse, oral sex or a handjob's receiver, take everything off.
- **Never** in a scene marked `"noStrip": true` (`SCENES.md`).
- **Redress** (`bRedress`): when the scene ends, each actor puts back on what they took off.
- **By hand:** the HUD's Utility tab, Actors, _name_: **Undress** / **Dress** (OPTIONS.md). Undressing by hand takes off everything in the undress slots, the player too even with `bUndressPlayer=0`; dressing puts back on everything 4Stim took off.

Weapons are always put away when a scene starts.

## Staying undressed

Fallout 4's NPCs put their outfit back on by themselves, and other mods equip things too, so clothes could come back on mid-scene. Two things keep them off until the scene ends:

- An NPC's items are taken off with Papyrus's `UnequipItem(item, abPreventEquip = true)`, so their AI doesn't put them back on (redressing equips them by script, which it allows). The player's aren't locked this way: it would keep you from putting them on yourself, and the player doesn't re-equip on their own. (Departure from OStim, which doesn't lock them: Skyrim's NPCs don't re-equip that way.)
- Four times a second, the scene guard checks everyone in a scene, the player included, for anything 4Stim took off that's back on, and takes it off again (`Undress: ... back on mid-scene` in `4Stim.log`).

When something stays on through a full undress, `4Stim.log` names it and its slots (`Undress: ... keeps ... on (slots ...)`), once per actor and scene. That's usually an outfit mod using a slot outside `sUndressSlots`, often one of the accessory slots 54 to 58: add those slots to `sUndressSlots` if your outfits use them for clothes rather than body parts.

## One piece at a time

Clothes come off and go back on all at once, as in OStim, or one item at a time with `fUndressItemDelay` seconds between them (a 4Stim setting; 0 by default).

## Animated undressing and redressing

- **Undressing scenes:** a scene can take clothes off at a set moment of its animation with an `undress` list (`SCENES.md`): a partner pulling someone's top off takes the top off as the hands get there. The test pack has OStim's: from **Standing apart** (MF), "take off _her_ chest armor / outfit / gloves / hat / boots" and "_she_ takes off _his_ outfit", each playing once and going back to standing apart. OStim's are a top and bottoms; Fallout 4's clothes are mostly one-piece outfits, so its top animation takes off the chest armor (slot 41) and its bottoms animation the whole outfit with any armor on it (33, 36-45).
- **Redress animations** (`SetAnimateRedress`, off by default, as OStim): when a scene ends, NPCs get dressed a body part at a time, torso, feet, hands, then head, each with its own animation, and that part's clothes go on partway through it, like OStim. The animations are one-actor scenes tagged `redress` plus the part (`torso`, `feet`, `hands`, `head`) for the actor's sex, with `length` and `dressAt` (`SCENES.md`); the test pack has OStim's. Anything that isn't in one of those parts goes on afterwards. The player dresses without them.

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

## Weapons

Everyone's weapon comes off as their scene starts (a drawn weapon's arm pose would show over the animation; OStim leaves weapons alone, Skyrim's animations hide them). 4Stim remembers which weapon it was and puts it back on when the scene ends, for the player and NPCs alike, as long as they still have it and aren't holding another. A scene saved and loaded keeps it too. The weapon goes back on by its base item, so with several of the same weapon (different mods on each), the game may pick another of them.
