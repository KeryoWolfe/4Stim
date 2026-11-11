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
| `keywords` / `notKeywords` | They, their race or their base have (don't have) one of these keywords: one a script gave them (`AddKeyword`) counts, as with Papyrus's `HasKeyword`. |
| `factions` / `notFactions` | They're (not) in one of these factions. |
| `worn` / `notWorn` | They're (not) wearing one of these armors: an equippable penis, a gag... |
| `wornKeywords` / `notWornKeywords` | Something they wear has (none has) one of these keywords. |
| `player` | `true`: the player; `false`: anyone else. |
| `child` | `false`: not a child. |
| `penis` | They have a penis or not (by the `sex` property above, else their body: Futa, below). |

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

In Fallout 4, futa usually comes from an equippable penis. With `bPenisFromBody=1` (the default; `[Roles]` in `4Stim.ini`, OStim's `useSoSSex` / `useTNGSex`) 4Stim sees it by itself: when no actor property says, anything the actor wears, their naked body included, that has a mesh skinned to ZeX's `Penis_*` bones, or whose name or material file (`.bgsm`) is like a penis's (`sPenisMeshNames`, `penis,schlong`; the game names worn meshes after their armor, so the material is often what tells: older penis mods such as Bag of Dicks and Futanari FEV skin theirs to the pelvis, from before ZeX's bones), gives them a penis (`Penis from body:` in `4Stim.log` says which mesh and which item; `Actor properties: … a penis,` says, for each actor the picker looks at, whether a property file or the body decided). Undress leaves that item on, as it would a body. 4Stim's own equip objects don't count, so a strap-on never does. OStim reads Schlongs of Skyrim's or TNG's factions for the same thing; Fallout 4 has neither, so 4Stim reads the meshes. Without the 3D loaded, or with `bPenisFromBody=0`, it's the base sex.

Some futa mods hide the penis while their futa are dressed and show it only once they're naked (Futanari FEV); as roles and scenes are picked before undressing, an actor property on the mod's keyword, or on its penis items being worn, says it instead (4Stim's patch for Futanari FEV ships both). An actor property still decides first, as in OStim: to give a penis to those wearing something without penis bones, to override the body, or for mods like that:

```json
{
	"condition": { "sex": "female", "worn": [ { "mod": "SomeFutaMod.esp", "formid": "0x801" } ] },
	"priority": 30,
	"sex": "male"
}
```

A property's `"sex"` is `"male"` (a penis), `"female"` (none) or, as OStim's, anything else (`"agender"`): that one still wins over lower priorities, but whether they have a penis goes by the body (above).

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

**Scenes without you** (4Stim's): with the same settings (two of the same sex: `playerSelectRoleGay`, on by default), the picker asks "Who leads?": the NPC picked takes the first role (usually the one giving), or either. To have a woman wear a strap-on with another woman, set `SetOnlyGayAnimsInGayScenes=0`: with it on (OStim's default too), two women only get scenes whose roles are for women or either sex, and those don't need a penis.

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

4Stim's optional **4Stim Equip Objects** add-on (`Optional\4Stim Equip Objects` in the repository) has OStim's own strap-on (Calyps', refit to True Wasteland Body): build "4Stim OStim Strap-On (TWB)" in BodySlide with your preset and enable `4Stim - Equip Objects.esp`. Its harness follows TWB's sliders and weights; its dildo is weighted to ZeX's penis bones, so it bends with the scene's penis bend (`ALIGNMENT.md`). Without the add-on there's none: fill the folder from a strap-on mod's armors. It also has OStim's tongues (Egilvar's: normal, long and snake, plain, pierced and double-pierced, each with a "cum" variant), moved to Fallout 4's head: they come out with the facial expressions that show a tongue (`phonemeObjects`, `EXPRESSIONS.md`), as in OStim. The normal tongue is the default. They have no physics yet (OStim's SMP bones aren't kept). Humans and ghouls wear them. A tongue put on by hand (the HUD's Tongue page, or `FourStim.EquipObject(actor, "tongue")`) also holds the `tongue` face, mouth open, until it's taken off: Fallout 4 has no jaw bone, so a tongue in a closed mouth can't be seen (4Stim's; OStim only shows tongues with the faces that call for them).

An actor wears one:

- **Strap-ons:** someone without a penis, in a role that needs one. It comes off in a role that needs a vagina, or in any role that doesn't need a penis with `unequipStrapOnIfNotNeeded=1`.
- **From actions:** a side's `equipObject` (`ACTIONS.md`), while the scene has that action.
- **From actor properties:** `equipObjects`, from the start.
- **From animations:** the `4StimEquipObject.<type>` / `4StimUnequipObject.<type>` annotations (OStim's `OStimEquipObject`).
- **From scripts:** `FourStim.EquipObject` and the rest (below).

Everything put on is taken off and away when the scene ends (also from a save loaded with the scene running). Undressing leaves equip objects on.

Lights are equip objects of type `light` as OStim's, chosen and put on through the same functions, but placed as game lights rather than worn (`LIGHTS.md`). Which object of a type an actor wears follows OStim's order: what's chosen for the actor in the HUD's options (each actor's page, OPTIONS.md, or `FourStim.SetEquipObjectID`; 4Stim's, OStim has no per-reference choice), then for their base, their race, their sex, then the type's `default` file, then any. A choice can be an object, `random` or `default` (the next step down). The steps above the actor are OStim's `OData` functions on `FourStim` (`GetEquipObjectPairs`, `GetEquipObjectName`, and `SetFormEquipObjectID`, OStim's `SetEquipObjectID`), by form ID: OStim's `0x0` for men and `0x1` for women, the player's base `0x7`. All are saved with the game. OStim's MCM choices are in 4Stim.ini, where the save has none: `sDefaultStrapOn` (women's), `sPlayerStrapOn`, `sDefaultTongueMale`, `sDefaultTongueFemale`, `sPlayerTongue` (an object's file name, `random` or `default`). They aren't on the MCM page: Fallout 4's MCM can't list choices that depend on what's installed.

## NPCs in a scene

As in OStim, an NPC in a scene does nothing but the scene. OStim hooks Skyrim's alias-package check and answers it with its own do-nothing package (OStim.esp's `OStimScenePackage`); 4Stim hooks Fallout 4's (`TESObjectREFR::CheckForCurrentAliasPackage`, Actor's vtable slot 0x5B) the same way. The game asks that before the NPC's own packages, and before starting dialogue or follow packages, so nothing their AI would do (a settler's job, sandboxing, a work spot, a chair) takes them out of the scene. When the scene ends they're handed back their own packages (`EvaluatePackage`). The player isn't touched.

4Stim has no plugin file, so the package isn't a record: it's made the way the game makes its own do-nothing package (`Actor::InitiateDoNothingPackage`: procedure type 0x1B, must complete). Where the game version's functions for that aren't found (1.10.163, possibly), it's Fallout4.esm's `MQ101VaultTecRepDoNothingScene` (a "Wait" with StopMovement, as OStim's). The package doesn't ignore combat.

## When a scene ends by itself

As OStim's (`Thread::loop`): a scene ends when anyone in it dies or is in combat, and a scene without the player ends when its first actor isn't in the player's cell (the player went elsewhere). `4Stim.log` says why: `Scene "...": ending, 0001A2B3 is in combat`. Like OStim's, the cell check is the game's cell: in the open world, walking a few cells away from an NPC scene ends it.

Actors in a scene don't start fights themselves (the scene holds them on the spot, on its own package), so a scene went on while its actors were shot. 4Stim's own addition, `bEndOnDamage` (1 by default; MCM, General page): a scene also ends when someone in it loses health (`ending, ... was hurt (... health lost)`). Shooting near them doesn't count.

When a scene ends, OStim's "Reset position" (`SetResetPosition`) sends NPCs back where they stood before it. A scene broken off by a death, a fight or harm doesn't, with 4Stim's `bStayWhenInterrupted` (1 by default): they carry on from the scene's spot, stepping off it so no one is stuck inside someone else, so a raider whose partner was shot turns on you from there.

A scene that couldn't start (no such scene, or the game refused every actor's animation) lets its actors go again a moment later, camera and controls included, as if it had ended (`Start failed (...): letting ... go again`). OStim applies nothing to actors until its thread exists; 4Stim's start places and locks them first, so it undoes that.

Loading a game while the player is in a scene puts back the player's controls, the camera speed, the FOV and the HUD (`Player scene state reset`); they aren't saved with the game and didn't end with the old one.

`4Stim.log` says which package is used (`Scene package: ...` at load) and who gets it. The scene guard (`bGuardScenes`) still takes anyone out of furniture or an idle marker as a fallback.

## Papyrus

In `FourStim.psc` (OStim's `OActor` functions):

- `HasPenis(Actor)` (OStim's `HasSchlong`), `GetActorType(Actor)`
- `EquipObject(Actor, String asType)`, `UnequipObject`, `IsObjectEquipped`, `SetObjectVariant(Actor, String asType, String asVariant, float afDuration = 0.0)`, `UnsetObjectVariant`, for actors in a scene
- `GetEquipObjectTypes()`
