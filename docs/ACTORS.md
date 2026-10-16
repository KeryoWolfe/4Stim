# Actors and roles

As in OStim, who can take which role in a scene depends on three things: the actor's **type**, what they **have** for the role's requirements, and (with `SetOnlyGayAnimsInGayScenes`) their **sex**. All three come from **actor properties**. **Equip objects** (strap-ons, tongues...) are put on and taken off as actors move through scenes.

## Actor properties

Files in `Data\F4SE\Plugins\4Stim\Actor Properties\` (OStim: `SKSE\Plugins\OStim\actor properties`), one per file, in any subfolder. Each says what is true of the actors its condition holds for:

```json
{
	"condition": { "keywords": [ "ActorTypeNPC" ], "child": false },
	"priority": 10,
	"type": "npc",
	"requirements": { "anus": true, "foot": true, "hand": true, "mouth": true, "nipple": true, "penis": true }
}
```

| Field | Meaning |
|---|---|
| `condition` | Who this file is for (below). |
| `priority` | Where several files set the same thing for an actor, the highest wins (OStim: the perk's level). Default 0, or the perk's level. |
| `type` | The actor type (OStim's): `"npc"` for people. A role takes only actors of its own `type` (scene files, default `"npc"`). An actor with no type can't be in scenes at all, so this is also who the picker lists. |
| `sex` | `"male"`: the actor has a penis (OStim's: for futa); `"female"`: they don't. Otherwise, by the body. Who has a penis goes first in a scene, gets no strap-on, and climaxes like a man (below). |
| `requirements` | What they have for scene roles' requirements (`ACTIONS.md`): `true` gives it, `false` takes it away (a higher priority's `false` overrides a lower one's `true`). |
| `equipObjects` | Equip object types they wear from the start of every scene (`true`), or not (`false`). |
| `expression` | A facial expression set kept the whole scene, over the scene's own (OStim's: a gag keeps the mouth open; `EXPRESSIONS.md`). |
| `mute` / `muffle` | `true`: no sounds at all / only muffled ones, the whole scene (a gag). |

### Conditions

OStim's actor properties borrow a perk's conditions from OStim.esp. 4Stim has no plugin, so its conditions are written in the file; every field given must hold (in a list, any one of it will do):

| Field | Holds when |
|---|---|
| `sex` | `"male"` / `"female"`: the body's sex. |
| `races` / `notRaces` | Their race is (isn't) one of these. |
| `keywords` / `notKeywords` | Their race or base has (doesn't have) one of these keywords. |
| `factions` / `notFactions` | They're (not) in one of these factions. |
| `worn` / `notWorn` | They're (not) wearing one of these armors: an equippable penis, a gag... |
| `wornKeywords` / `notWornKeywords` | Something they wear has (none has) one of these keywords. |
| `player` | `true`: the player; `false`: anyone else. |
| `child` | `false`: not a child. |
| `penis` | They have a penis or not (by the `sex` property above, else their body). |

A form is OStim's `{ "mod": "Fallout4.esm", "formid": "0x13746" }`, or for keywords and races, an editor ID (`"ActorTypeNPC"`, `"HumanRace"`).

OStim's own condition works too, for a mod with its own plugin: give the condition a form (`{ "mod": "MyMod.esp", "formid": "0x800" }`) and it's a perk whose condition functions must hold (the actor doesn't need the perk). The perk's level is the priority unless the file gives one.

### The ones 4Stim comes with

| File | For | Gives |
|---|---|---|
| `4StimNPC` | People (keyword `ActorTypeNPC`): humans and ghouls, not children | type `npc`; hand, mouth, foot, nipple, anus, penis |
| `4StimFemale` | Women | breast, vagina |
| `4StimPenis` | Those with a penis | testicles |
| `4StimGhoul` | Ghouls (`ActorTypeGhoul`) | ghoul |
| `4StimSuperMutant` | Super mutants (`ActorTypeSuperMutant`) | type `supermutant` (only scenes made for them), and the body parts |

As OStim's `OStimNPC`, everyone "has" a penis: a woman can take a role that needs one, and wears a strap-on in it. With `SetOnlyGayAnimsInGayScenes=1` (the default) she only does in scenes that leave the role's sex open. OStim's vampire, beast race, tail and in-water properties have no Fallout 4 counterpart and aren't included. If the folder is missing, 4Stim uses the first three built in.

### Futa

In Fallout 4, futa comes from an equippable penis, not from the body. Give those who wear one a penis with an actor property:

```json
{
	"condition": { "sex": "female", "worn": [ { "mod": "SomeFutaMod.esp", "formid": "0x801" } ] },
	"priority": 30,
	"sex": "male"
}
```

They then go first, wear no strap-on, and, with OStim's settings (`4Stim.ini`, `[Roles]`): take men's roles too (`futaUseMaleRole`, default on), gain excitement at the male rate (`futaUseMaleExcitement`, off), and climax as men (`futaUseMaleClimax`, on: for `SetEndOnOrgasm`).

## Roles

When a scene starts, the actors are put in OStim's order: those with a penis first (otherwise as picked), then the player moved to their place. The scene gives each actor the role in that order if it fits, and otherwise the first other order that does (4Stim's: OStim only offers scenes that fit its order).

The player's place, in `[Roles]` (OStim's settings):

| Setting | Default | Meaning |
|---|---|---|
| `PlayerAlwaysDomStraight` / `PlayerAlwaysSubStraight` | 0 | In a scene for a man and a woman, you always take the first role / the second. |
| `PlayerAlwaysDomGay` / `PlayerAlwaysSubGay` | 0 | The same, with two of the same sex. |
| `playerSelectRoleStraight` / `Gay` / `Threesome` | 0 / 1 / 0 | The picker asks "Your role?" after you pick who's in it: role 1 (usually the one giving), role 2..., or any. |
| `unrestrictedNavigation` | 0 | Only the actor type counts: any actor takes any role (OStim's). |

With either, only scenes where you can have that place are offered.

## Equip objects

Armor items 4Stim puts on actors during scenes and takes away at the end (OStim's equip objects). One per file in `Data\F4SE\Plugins\4Stim\Equip Objects\`, the id being the file name, in OStim's format:

```json
{
	"type": "strapon",
	"name": "My strap-on",
	"default": true,
	"mod": "MyStrapOns.esp",
	"formid": "0x800",
	"variants": {
		"wet": { "mod": "MyStrapOns.esp", "formid": "0x801" }
	}
}
```

| Field | Meaning |
|---|---|
| `type` | What it is: `strapon` (4Stim puts these on itself), `tongue`, or any type an addon or animation uses. |
| `name` | Its name. |
| `default` | The one of its type actors get; without one, a random one. |
| `mod`, `formid` | The armor. Make it non-playable and use it for nothing else. |
| `variants` | Other armors for it (`"cum"`...), switched to by scripts. |

4Stim comes with none: Fallout 4 has no strap-on or tongue of its own. Fill the folder from a strap-on mod's armors.

An actor wears one:

- **Strap-ons:** someone without a penis, in a role that needs one. It comes off in a role that needs a vagina, or in any role that doesn't need a penis with `unequipStrapOnIfNotNeeded=1`.
- **From actions:** a side's `equipObject` (`ACTIONS.md`), while the scene has that action.
- **From actor properties:** `equipObjects`, from the start.
- **From animations:** the `4StimEquipObject.<type>` / `4StimUnequipObject.<type>` annotations (OStim's `OStimEquipObject`).
- **From scripts:** `FourStim.EquipObject` and the rest (below).

Everything put on is taken off and away when the scene ends (also from a save loaded with the scene running). Undressing leaves equip objects on.

Departures from OStim: lights are 4Stim's own (`LIGHTS.md`), not equip objects, since they'd need armors with lights in their meshes. There's no MCM to choose an equip object per actor, race or sex: it's the default one, or random.

## Papyrus

In `FourStim.psc` (OStim's `OActor` functions):

- `HasPenis(Actor)` (OStim's `HasSchlong`), `GetActorType(Actor)`
- `EquipObject(Actor, String asType)`, `UnequipObject`, `IsObjectEquipped`, `SetObjectVariant(Actor, String asType, String asVariant, float afDuration = 0.0)`, `UnsetObjectVariant`, for actors in a scene
- `GetEquipObjectTypes()`
