# Actions

An **action** says what the actors in a scene are doing to each other: kissing, a handjob, vaginal sex. Scenes list their actions (the `actions` field, see `SCENES.md`), and action **types** say what each action means. 4Stim uses them to decide who can take which role, how excited each actor gets (`EXCITEMENT.md`) and what they take off (`UNDRESS.md`). Sounds and expressions will use them later.

The types and their ids follow OStim's. A scene converted from OStim keeps its `actions` list as it is.

## In a scene

```json
"actions": [
	{ "type": "vaginalsex", "actor": 0, "target": 1, "performer": 1 },
	{ "type": "gropingbutt", "actor": 0, "target": 1 }
]
```

| Field | Required | Meaning |
|---|---|---|
| `type` | yes | An action type id or one of its aliases. Case-insensitive. |
| `actor` | no (0) | The role doing the action. |
| `target` | no (the actor) | The role it's done to. Leave it out for something an actor does to themselves (masturbation). |
| `performer` | no (the actor) | The role that's moving. |
| `muted`, `doPeaks`, `peaksAnnotated` | no | OStim's: no sounds from this action here / no peaks / peaks from annotations only (`EVENTS.md`). |

**Actor, target and performer** follow OStim's convention. Actor and target are fixed by what the action is, not by who's active. In vaginal sex the one with the penis is always the actor and the one with the vagina the target. In a blowjob the one with the mouth is the actor. Who's doing the moving goes in `performer`, so a cowgirl scene is `vaginalsex` with actor 0 (him), target 1 (her) and performer 1.

## Action type files

Action types are defined in JSON files in `Data\F4SE\Plugins\4Stim\Actions\`. All files in the folder are read in name order, and a later file replaces an earlier file's type with the same id. `Default.json` ships the types, adapted from OStim NG.

```json
{
	"actions": [
		{
			"id": "handjob",
			"name": "Handjob",
			"aliases": [ "hj" ],
			"tags": [ "penilestimulation", "sexual" ],
			"actor": { "stimulation": 0.1, "maxStimulation": 50, "requirements": [ "hand" ] },
			"target": { "stimulation": 0.9, "requirements": [ "penis" ] }
		}
	]
}
```

| Field | Meaning |
|---|---|
| `id` | The type's id, used in scene files. Case-insensitive. |
| `name` | Display name (picker, HUD). |
| `aliases` | Other ids scene files may use for this type. |
| `tags` | Action tags: `sexual`, `sensual`, `romantic`, `oral`, `intercourse`, `vaginalpenetration`... Papyrus can query them (`SceneHasActionTag`). |
| `actor`, `target`, `performer` | What the action means for that side. Each can have the three fields below. |
| `stimulation` | Excitement per second for that side's actor (`EXCITEMENT.md`). |
| `maxStimulation` | Past this excitement, the action stops adding any (default 100). For example, kissing alone won't bring anyone to climax. |
| `requirements` | Body parts that side needs (see below). |
| `fullStrip` | `true`: that side's actor takes everything off (UNDRESS.md). |
| `undressSlots` | Otherwise, the Fallout 4 biped slots (30–61) that side's actor takes off, e.g. `[34, 35]` for gloves. |
| `moan`, `talk`, `muffled`, `expressionOverride` | That side's actor moans, may talk (kept for dialogue), sounds muffled, makes this expression (`SOUND.md`, `EXPRESSIONS.md`). |
| `equipObject` | Equip object types that side's actor wears while doing it (OStim's field: a type or a list, e.g. `"tongue"`; `ACTORS.md`). |
| `toySlot` | Sex toy slots that side drives (OStim's field: a slot or a list, e.g. `"genital"`, `"anus"`; API.md). |
| `peak` | How the action's peaks are found (`EVENTS.md`). |
| `sounds` | Sounds it plays on peaks, as bones meet, or in a loop (`SOUND.md`). |

### OStim's layout

OStim's action files work too: one action per file in `Actions\`, the file name being the id (`Actions\vaginalsex.json`), with no `actions` list. Their `strippingSlots` are Skyrim's biped slots, and 4Stim maps them to Fallout 4's with the table in `UNDRESS.md` (an `undressSlots` on the same side wins). Their `moan`, `talk`, `muffled`, `expressionOverride`, `sounds` and `peak` are read too (sounds as sound descriptors, or 4Stim's `file`); OStim's factions, stats and toy slots are skipped until the features they belong to come. Such an action has no `name`, so it's shown as its id with a capital letter.

## Requirements

A role collects the requirements of every action it's in, and only an actor who has all of them can take it, as in OStim. What an actor has comes from their actor properties (`ACTORS.md`); with 4Stim's default ones:

| Requirement | Who has it |
|---|---|
| `hand`, `mouth`, `foot`, `nipple`, `anus`, `penis` | everyone (OStim's: a woman takes a role that needs a penis with a strap-on) |
| `testicles` | those with a penis: men, and futa |
| `vagina`, `breast` | women |
| `ghoul` | ghouls |
| `tail`, `vampire`, anything else | no one, unless an actor property gives it |

So a scene with an action that needs a tail or a vampire never shows up: Fallout 4 has neither, which hides a few test-pack scenes. With `SetOnlyGayAnimsInGayScenes=1` (the default), the role's sex counts too, so a woman takes a man's role only in scenes that leave it open.

## Papyrus

`FourStim.psc` has these functions:

- `SceneHasAction` and `SceneHasActionTag`
- `GetSceneActionCount`, `FindSceneAction`, `GetSceneActionType` and `GetSceneActionRole`
- `GetActionTypes`, `GetActionName`, `GetActionTags` and `ActionHasTag`

Scene events give the scene id, which these functions take.
