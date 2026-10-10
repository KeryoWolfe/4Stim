# Adding animations to 4Stim

4Stim reads scenes from JSON files. To add your animations you need two things, and no scripting:

1. **A plugin with Idle records** for your animations (one Idle per actor role and speed; see "The plugin: Idle records").
2. **A JSON file** describing your scenes, placed in:

```
Data\F4SE\Plugins\4Stim\Scenes\YourPack.json
```

Every `.json` file in that folder is loaded. Use a filename unique to your pack.

## The plugin: Idle records

Each animation clip is played through an **Idle** record (IDLE) in your plugin: one Idle per role, per speed. A two-actor scene with three speeds needs six Idles. The Idles aren't part of any idle tree; 4Stim plays them directly by form ID, so they need no conditions and no parent.

| Field (xEdit / Creation Kit) | Value |
|---|---|
| Editor ID (`EDID`) | Anything unique, e.g. `MyPack_Cowgirl_S1_0` (scene, speed, role). It's what you type in the console to test: `player.playidle MyPack_Cowgirl_S1_0`. |
| Behavior Graph (`DNAM`) | `Actors\Character\Behaviors\RaiderRootBehavior.hkx` (human actors). |
| Animation Event (`ENAM`) | `dyn_ActivationLoop` for scenes that loop. **`dyn_Activation`** for transitions (see Transitions below). |
| Animation file (`GNAM`) | The clip's path relative to `Data\Meshes\`, e.g. `Actors\Character\Animations\MyPack\Cowgirl_S1_0_fo4.hkx`. |
| Related Idle Animations (`ANAM`) | None: parent and previous sibling both null. |
| Conditions | None. |

The clips themselves:

- **Fallout 4 format.** Havok 2014 (`hk_2014`) animations for the Fallout 4 skeleton. Skyrim (OStim) clips have to be converted first; the Skyrim-to-FO4 converter writes the clips and a plugin with these Idles for you (`pipeline.py`).
- **Put them under** `Data\Meshes\Actors\Character\Animations\<YourPack>\`, at the path the Idle's `GNAM` names.
- **One shared origin.** All roles of a scene are authored around the same point (see "Two-actor scenes"). The actors are placed on that point with the same heading.
- **Loops loop.** A clip for an ordinary scene should end in the pose it starts in.
- **Transitions are different:** they play once (`dyn_Activation`), end with about 0.5 s of held last pose, and end on the destination's first pose (see Transitions).

The plugin can be a light plugin (ESL-flagged). Scene files use the Idles' form IDs within your plugin, so load order doesn't matter (see "Form IDs").

**Converter note:** the converter currently writes every Idle with `dyn_ActivationLoop`. After converting, change the transition Idles' animation event to `dyn_Activation` in xEdit, and pad the transition clips with `hold_end.py`. Regenerating the plugin undoes the event change.

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
| `sex` | actor | no | Who may take this role: `"male"`, `"female"` or `"any"` (default). With `SetOnlyGayAnimsInGayScenes=1` (the default, in `4Stim.ini`), the picker, the HUD and navigation only offer scenes and sequences whose roles the actors fit, and a new scene gives each actor the role that fits (the player takes the first role unless only the other order fits). A sequence's role asks for whatever its scenes ask for; scenes that ask for different sexes in the same role can't share a sequence. OStim's `intendedSex` is read the same way. |
| `speeds` | scene | one of `speeds` or actor `idle` | List of speeds, slowest first. Each has `idles`: one Idle form ID per role, in role order. |
| `autoTransitions` | actor | no | Scenes to move to on an event for the actor in this role, OStim's format: `{"climax": "MyPack_MissionaryClimax", "pullout": "MyPack_MissionaryPullout"}`. `climax` plays when the actor climaxes (see `EXCITEMENT.md`), `pullout` when auto mode has a man pull out (`AUTOMODE.md`); scripts can play any other with `FourStim.AutoTransition`. Each must be a scene with the same number of actors, usually a transition back. `"climax": "..."` on the actor is short for the climax one. |
| `tags` | actor | no | What the actor in this role is doing: `standing`, `kneeling`, `sitting`, `lyingback`, `lyingfront`, `allfours`, `ontop`, `onbottom`, `facingaway`... (OStim's actor tags). Auto mode uses `standing` (`AUTOMODE.md`); scripts can query them (`SceneActorHasTag`). |
| `requirements` | actor | no | Body parts the actor in this role needs besides what the scene's actions ask for (`ACTIONS.md`, Requirements), e.g. `["penis"]`. |
| `idle` | actor | one of `speeds` or actor `idle` | Single-speed shorthand: the Idle form ID for this role. |
| `navigations` | scene | no | Scenes the player can move to from this one, in display order. Each has `to` (destination scene ID) and `label`. In a label, `{0}`, `{1}` and so on are replaced by the name of the actor in that role. |
| `actions` | scene | no | What the actors do to each other: a list of `{"type": "vaginalsex", "actor": 0, "target": 1}`. `type` is an action type id or alias (`ACTIONS.md`); `actor` is the role doing it, `target` the role it's done to (default: the actor, for things done to oneself), and `performer` the role moving (default: the actor; in cowgirl the target is the performer). Unknown types are skipped with a warning. With `SetOnlyGayAnimsInGayScenes=1` a role's actions must also fit its actor: a role that receives `vaginalsex` needs a vagina, so only women take it (`ACTIONS.md`, Requirements). The picker shows a scene's actions under its name. Excitement, undressing and sounds will build on them. |
| `tags` | scene | no | Free-form tags. One has a meaning: **`idle`** marks a scene a new scene can start with. After the player picks who and where, 4Stim starts a random `idle` scene that fits them (any fitting scene if none is tagged), like OStim. Tag your neutral standing, sitting or lying poses `idle`, not transitions. |
| `icon` | scene or navigation | no | HUD icon: a `.dds` (or `.swf`) under `Data\Interface\4Stim\Icons\`, extension optional, so `"4Stim/positional/standup_f"` is `Icons\4Stim\positional\standup_f.dds` (see `HUD_API.md`). On a navigation entry it overrides the destination scene's icon. |
| `length` | scene | for transitions | Seconds one play-through of the animation takes. A transition moves on after this long; a sequence uses it when an entry has no `duration`. |
| `undress` | scene | no | Animated undressing: a list of `{"actor": role, "slots": [biped slots], "at": seconds}`. At `at` seconds into the scene, the actor in that role takes off what they wear in those slots (within `sUndressSlots`, see `UNDRESS.md`). For scenes whose animation shows clothes coming off. |
| `dressAt` | scene | no | For redress animations (one-actor scenes tagged `redress` and `torso`, `feet`, `hands` or `head`): when, in seconds, that part's clothes go on. `length` is how long the animation plays in all. See `UNDRESS.md`. |
| `destination` | scene | no | Makes the scene a **transition**: it plays once (for `length` seconds) and then moves on to this scene by itself. Must have the same number of actors. |
| `furniture` | scene | no | The furniture type the scene is played on (`"bed"`, `"chair"`, `"table"`...; see `FURNITURE.md`). The picker only lists it when furniture of that type, or of a subtype, is near the player, and the actors are moved onto the nearest such piece. Navigation stays on the furniture: from a furniture scene only scenes for the same furniture are offered, and from a scene without `furniture` only scenes without it. The exception is furniture whose type has `"supertype": "none"` (beds, as in OStim): on it, scenes without `furniture` are offered too, and play on the furniture. All scenes of a sequence must have the same `furniture`. |
| `furnitureOffset` | scene | no | `[x, y, z]` or `[x, y, z, degrees]`: moves this scene from its furniture type's spot, in the spot's own frame (x to the right, y forward, z up), and turns it. For scenes whose animation starts somewhere other than the rest on that furniture. |
| `defaultSpeed` | scene | no | The speed auto mode starts this scene at, 0 = slowest (OStim's convention; sequences count from 1). Navigating by hand keeps the current speed. |
| `noRandomSelection` | scene | no | `true`: auto mode never picks this scene at random (it can still walk through it). For scenes that only make sense from a particular other one. |
| `noStrip` | scene | no | `true`: no one undresses in this scene, whatever its actions (UNDRESS.md). Scenes after it still undress as usual. |

## Speeds and navigation in game

During a scene, the speed keys (`=` and `-` by default, set in `4Stim.ini`) step through the scene's speeds, and the 4Stim hotkey gives the in-scene HUD input focus: its Navigation tab lists the scene's navigation links, plus "End scene". The HUD's speed meter shows one segment per speed. Moving to another scene keeps everyone in their roles and keeps the current speed if the destination has that many. A navigation link only works between scenes with the same number of actors; links to scenes that don't exist or don't match are dropped with a warning in the log.

## Transitions

A transition is a short animation between two poses, like turning around from cowgirl to reverse cowgirl. Give it a `length` and a `destination`:

```json
{ "id": "MyPack_CowgirlToReverse", "name": "Turn around", "length": 1.95,
  "destination": "MyPack_ReverseCowgirl", "actors": [ ... ] }
```

Navigate to the transition as to any scene (the player picks "turn around"); when its time is up, the scene moves on to the destination on its own, at the same speed where the destination has it. Time only counts while the game isn't paused. The scene moves on `fTransitionLead` seconds before the length (0 by default, in `4Stim.ini`); if the clip ends first (the game's "IdleStop"), it moves on the moment it does.

Three things about the transition idle and clip:

- **Play it once.** Its idle record's animation event must be `dyn_Activation`, not `dyn_ActivationLoop`. Leaving a looping idle makes the game blend from the clip's *first* frames for a moment, a bounce back.
- **Hold the last pose for about 0.5 s at its end.** The game starts blending a one-shot clip back to the base pose about 0.35 s before the clip's end, so motion in that stretch would never be seen, and the base pose would show through. Set the scene's `length` to the motion, not the padded clip. The converter's `hold_end.py` adds the held frames: `python hold_end.py --seconds=0.5 <clips or folder>`.
- **Its last pose should be the destination's first.** During the transition, the HUD already lists the destination's options; picking one waits for the transition to finish and then goes there instead of the destination. A transition without a `length`, or whose destination doesn't exist, is logged and plays like an ordinary scene.

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
| `scenes` | yes | The scenes in order. All must have the same number of actors and the same `furniture`. |
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
