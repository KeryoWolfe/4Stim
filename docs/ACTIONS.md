# Actions

An **action** says what the actors in a scene are doing to each other: kissing, a handjob, vaginal sex. Scenes list their actions (the `actions` field, see `SCENES.md`), and action **types** say what each action means. 4Stim uses them to decide who can take which role. Excitement, undressing, sounds and expressions will use them later.

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
| `stimulation` | Excitement per second for that side's actor. Not used yet; it's for the excitement system. |
| `maxStimulation` | Past this excitement, the action stops adding any (default 100). For example, kissing alone won't bring anyone to climax. |
| `requirements` | Body parts that side needs (see below). |

## Requirements

A role collects the requirements of every action it's in. With `bMatchSex=1`, an actor can only take the role if they have everything it needs:

| Requirement | Who has it |
|---|---|
| `hand`, `mouth`, `foot`, `nipple`, `anus` | everyone |
| `penis`, `testicles` | men (strap-ons will add a penis for women; see TODO) |
| `vagina`, `breast` | women |
| `tail`, `vampire`, anything else | no one yet |

So a scene with an action that needs a tail or a vampire never shows up. Fallout 4 has neither, which hides a few test-pack scenes.

## Papyrus

`FourStim.psc` has these functions:

- `SceneHasAction` and `SceneHasActionTag`
- `GetSceneActionCount`, `FindSceneAction`, `GetSceneActionType` and `GetSceneActionRole`
- `GetActionTypes`, `GetActionName`, `GetActionTags` and `ActionHasTag`

Scene events give the scene id, which these functions take.
