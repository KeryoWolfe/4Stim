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

When something stays on through a full undress, `4Stim.log` names it and its slots (`Undress: ... keeps ... on (slots ...)`), once per actor and scene. That's an item with a NoStrip keyword, or one only in slots that aren't checked: check those slots on the MCM's Undressing slots page if your outfits use them for clothes.

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

As in OStim: a set of biped slots (OStim's `SetUndressingSlotMask`), and any armor worn that covers one of them comes off whole, unless it has a keyword with "NoStrip" in its name (OStim's `OStimNoStrip`, SexLab's `SexLabNoStrip`...: mods can mark items that must stay on). The slots are set on the MCM's **Undressing slots** page, a checkbox per slot as OStim's, or in `4Stim.ini`'s `[UndressSlots]` (`bUndressSlot30` to `bUndressSlot61`; OStim's `SetUndressingSlotMask`, a mask with bit 0 for slot 30, and the older `sUndressSlots` list are read too).

**Skyrim and Fallout 4 dress differently.** Skyrim's armor is one piece per region (body 32, head 30, hands 33, feet 37), and modders took the unnamed slots (44–49, 52–61) for cloaks, backpacks and the like, and for parts of the body (SOS's schlong is 52). OStim's default undresses Skyrim's clothing and modders' slots and leaves hair, ears, tail, amulet, ring, circlet and the schlong on. Fallout 4's clothes come in layers: an outfit in 33 (usually also claiming the under-armor slots 36–40), armor pieces on top in 41–45, and mods' extra pieces (jackets, belts, bandoliers, backpacks) in the unnamed 54–58. 4Stim's default is OStim's, mapped to that:

| Slot | | Default |
|---|---|---|
| 30, 31 | Hair top, hair long | stays on |
| 32 | FaceGen head | stays on |
| 33 | BODY: outfits, jumpsuits | **comes off** |
| 34, 35 | Left and right hand | **comes off** |
| 36–40 | Under-armor: torso, arms, legs | **comes off** |
| 41–45 | Armor pieces: torso, arms, legs | **comes off** |
| 46 | Headband: hats, helmets | **comes off** |
| 47 | Eyes: glasses, goggles | **comes off** |
| 48 | Beard | stays on |
| 49 | Mouth: masks, bandanas | **comes off** |
| 50 | Neck: scarves, collars | stays on (as OStim's amulet slot) |
| 51 | Ring | stays on (as OStim) |
| 52 | Scalp | stays on |
| 53 | Decapitation | stays on |
| 54–58 | Unnamed: mods' extra clothing | **comes off** (as OStim's modders' slots) |
| 59 | Shield | stays on |
| 60 | Pip-Boy | stays on (the Pip-Boy itself never comes off) |
| 61 | FX | stays on |

If a body mod puts part of the body (genitals, pubic hair) in one of 54–58, uncheck that slot, or give the item a NoStrip keyword. 4Stim's own equip objects (strap-ons, tongues) always stay on.

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

A slot that isn't checked is skipped anyway.

## Papyrus

- `FourStim.UndressActor(akActor)` takes everything in the undress slots off an actor (like the HUD's Undress, so the player too).
- `FourStim.RedressActor(akActor)` puts back on what 4Stim took off.

What's been taken off is kept in the save with the scene: a scene that starts again on load still dresses everyone at the end, and one that doesn't dresses them on load (`bResumeScenes`, `4Stim.ini`).

The plugin finds what each actor wears in which slots; `FourStimUndress.psc` (vanilla Papyrus) takes it off and puts it back on.

## Weapons

Everyone's weapon comes off as their scene starts (a drawn weapon's arm pose would show over the animation; OStim leaves weapons alone, Skyrim's animations hide them). 4Stim remembers which weapon it was and puts it back on when the scene ends, for the player and NPCs alike, as long as they still have it and aren't holding another. A scene saved and loaded keeps it too. The weapon goes back on by its base item, so with several of the same weapon (different mods on each), the game may pick another of them.
