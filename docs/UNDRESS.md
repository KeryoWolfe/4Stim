# Undressing

Scene actors take off their clothes and armor during a scene and put them back on when it ends, like in OStim. The settings are in the `[Undress]` section of `4Stim.ini`.

## When

- **Undress at start** (`bUndressAtStart`, off by default): everything comes off when a scene starts.
- **Partial** (`bPartialUndress`): each time the scene moves to another scene, each actor takes off what that scene's actions need for their side. A kiss takes the hat off, a handjob the giver's gloves (`undressSlots` on the action type, `ACTIONS.md`).
- **Full** (`bFullUndressMidScene`): actions marked `fullStrip`, such as intercourse, oral sex or a handjob's receiver, take everything off.
- **Never** in a scene marked `"noStrip": true` (`SCENES.md`).
- **Redress** (`bRedress`): when the scene ends, each actor puts back on what they took off.

Weapons are always put away when a scene starts.

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

- `FourStim.UndressActor(akActor)` takes everything in the undress slots off an actor.
- `FourStim.RedressActor(akActor)` puts back on what 4Stim took off.

The work is done by `FourStimUndress.psc`, using F4SE's `GetWornItem`.
