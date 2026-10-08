# Adding animations to 4Stim

4Stim reads scenes from JSON files. To add your animations you need two things, and no scripting:

1. **A plugin with Idle records** for your animations (one Idle per actor role).
2. **A JSON file** describing your scenes, placed in:

```
Data\F4SE\Plugins\4Stim\Scenes\YourPack.json
```

Every `.json` file in that folder is loaded. Use a filename unique to your pack.

## Example

```json
{
	// Default plugin for every idle in this file (optional)
	"plugin": "MyAnimPack.esp",
	"scenes": [
		{
			"id": "MyPack_StandingHug",
			"name": "Standing hug",
			"actors": [ { "sex": "male" }, { "sex": "female" } ],
			"speeds": [
				{ "idles": [ "0x000801", "0x000802" ] },
				{ "idles": [ "0x000803", "0x000804" ] }
			],
			"navigations": [
				{ "to": "MyPack_StandingKiss", "label": "kiss {1}" }
			],
			"tags": [ "standing", "romantic" ]
		},
		{
			"id": "MyPack_Wave",
			"name": "Wave",
			"actors": [ { "idle": "0x000810" } ]
		}
	]
}
```

The first scene has two speeds and one navigation option. The second uses the single-speed shorthand: one `idle` on each actor and no `speeds` list.

## Fields

| Field | Where | Required | Meaning |
|---|---|---|---|
| `plugin` | file, speed or actor | yes, on one of them | Plugin file holding the Idle records. The closest one wins. |
| `scenes` | file | yes, unless the file only has `sequences` | List of scenes. |
| `sequences` | file | no | List of sequences (see below). |
| `id` | scene | yes | Unique scene ID. Case-insensitive. Prefix it with your pack name to avoid clashes. |
| `name` | scene | no | Display name. Defaults to the ID. |
| `actors` | scene | yes | One entry per role, **in role order**: the first entry is role 0, the second role 1, and so on. |
| `sex` | actor | no | `"male"`, `"female"` or `"any"` (default). Informational for now. |
| `speeds` | scene | one of `speeds` or actor `idle` | List of speeds, slowest first. Each has `idles`: one Idle form ID per role, in role order. |
| `idle` | actor | one of `speeds` or actor `idle` | Single-speed shorthand: the Idle form ID for this role. |
| `navigations` | scene | no | Scenes the player can move to from this one, in display order. Each has `to` (destination scene ID) and `label`. In a label, `{0}`, `{1}` and so on are replaced by the name of the actor in that role. |
| `tags` | scene | no | Free-form tags for future filtering. |
| `icon` | scene or navigation | no | HUD icon: a `.dds` (or `.swf`) under `Data\Interface\4Stim\Icons\`, extension optional, so `"4Stim/positional/standup_f"` is `Icons\4Stim\positional\standup_f.dds` (see `HUD_API.md`). On a navigation entry it overrides the destination scene's icon. |
| `length` | scene | for transitions | Seconds one play-through of the animation takes. A transition moves on after this long; a sequence uses it when an entry has no `duration`. |
| `destination` | scene | no | Makes the scene a **transition**: it plays once (for `length` seconds) and then moves on to this scene by itself. Must have the same number of actors. |

## Speeds and navigation in game

During a scene, the speed keys (`=` and `-` by default, set in `4Stim.ini`) step through the scene's speeds, and the 4Stim hotkey gives the in-scene HUD input focus: its Navigation tab lists the scene's navigation links, plus "End scene". The HUD's speed meter shows one segment per speed. Moving to another scene keeps everyone in their roles and keeps the current speed if the destination has that many. A navigation link only works between scenes with the same number of actors; links to scenes that don't exist or don't match are dropped with a warning in the log.

## Transitions

A transition is a short animation between two poses, like turning around from cowgirl to reverse cowgirl. Give it a `length` and a `destination`:

```json
{ "id": "MyPack_CowgirlToReverse", "name": "Turn around", "length": 1.95,
  "destination": "MyPack_ReverseCowgirl", "actors": [ ... ] }
```

Navigate to the transition as to any scene (the player picks "turn around"); when its time is up, the scene moves on to the destination on its own, at the same speed where the destination has it. Time only counts while the game isn't paused, and the move happens `fTransitionLead` seconds (0.4 by default, in `4Stim.ini`) before the length, so the animation doesn't restart first. During the transition, the HUD already lists the destination's options; picking one waits for the transition to finish and then goes there instead of the destination. A transition without a `length`, or whose destination doesn't exist, is logged and plays like an ordinary scene.

## Sequences

A sequence plays a fixed run of scenes, each for a set time, then stays on the last one (if that's a transition, it still moves on to its destination). Put them in a `sequences` list in any scene file:

```json
"sequences": [
	{
		"id": "MyPack_Ride",
		"name": "Ride",
		"tags": [ "2p", "mf" ],
		"scenes": [
			{ "id": "MyPack_CowgirlIdle", "duration": 4 },
			{ "id": "MyPack_Cowgirl", "duration": 8, "speed": 2 },
			{ "id": "MyPack_CowgirlToReverse" },
			{ "id": "MyPack_ReverseCowgirl", "duration": 10 }
		]
	}
]
```

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | Unique sequence ID, case-insensitive. |
| `name` | no | Display name. Defaults to the ID. |
| `tags` | no | Free-form tags. |
| `scenes` | yes | The scenes in order. All must have the same number of actors. |
| `scenes[].id` | yes | Scene ID. |
| `scenes[].duration` | unless the scene has a `length` | Seconds to stay in this scene. |
| `scenes[].speed` | no | Speed to play it at, 1 = slowest (default). |

A sequence with a missing scene, a mismatched actor count or an entry with no time is skipped with a warning in the log.

Starting one: the picker lists sequences first, as "Sequence: <name>", and in the HUD's Search tab picking one plays it on the running scene. From Papyrus, `FourStimScene.BeginPairSequence` / `BeginSequence` start one like `BeginPairScene` / `BeginScene`, and `FourStim.StartSequenceOnScene` plays one on a running scene. The player stays in control: navigating or picking another scene stops the sequence where it is (speed keys don't).

## Form IDs

Copy the Idle's form ID from xEdit or the Creation Kit. The load-order prefix (the first two hex digits, or `FE xxx` for light plugins) is ignored, so a full ID like `FE0EFAB8` and the local ID `0xAB8` both work. Your scenes keep working when the user's load order changes.

## Two-actor scenes

Author both roles around **one shared origin**, the way OStim does. 4Stim puts both actors on the same spot with the same heading, turns off collision between them, and starts both idles on the same frame. The distance between the actors must come from the animations themselves.

## Testing and troubleshooting

- Check each Idle on its own first: `player.playidle <EditorID>` in the console.
- After editing a JSON file, reload without restarting: `cgf "FourStim.ReloadScenes"`.
- Problems are logged in `Documents\My Games\Fallout4\F4SE\4Stim.log`. Every bad entry gets a line saying which file, scene and role failed and why (bad JSON, missing plugin, wrong ID, record isn't an Idle), and a summary line reports how many scenes loaded and how many were skipped. A broken entry is skipped; the rest of the file still loads.
- If two files define the same scene ID, the file that sorts later alphabetically wins, and the log says so.
