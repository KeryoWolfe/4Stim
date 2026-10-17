# Peaks and events

As in OStim, scenes have **peaks** (the high point of an action's motion: a thrust, a stroke) and **events** (something that happens once: a spank). Peaks play the slaps and strokes you hear during sex; events play their own sound, give stimulation, make actors react, and shake the camera.

## Peaks

An action finds its peaks from its `peak` (in its action file, docs/ACTIONS.md), OStim's format:

```json
"peak": { "type": "bonedistance", "actorBone": "Pelvis", "targetBone": "Pelvis" }
```

- **`bonedistance`**: a peak each time the action actor's bones and the target's come closest (with `"inverse": true`, furthest apart). `actorBone` and `targetBone` are a bone name or a list of them; with lists, the closest pair counts. 4Stim learns each animation's range as it plays, so it works at any size and speed, and waits a second after a scene change before measuring, as OStim does.
- **4Stim's `minRange`** (bonedistance, default 3 units): the bones must move at least this far apart and back before there are any peaks, so actors who only sway give none. OStim has no such field: Skyrim's genital bones always move far enough.
- **`loop`**: a peak every `interval` milliseconds, divided by the speed's `displaySpeed` (scene files) when it has one.

On each peak, the action's `peak` sounds play (docs/SOUND.md, "Action sounds").

4Stim's default actions use Fallout 4's vanilla bones, which every body has: vaginal and anal sex the two pelvises (`Pelvis`); a handjob and male masturbation the hands (`RArm_Hand`, `LArm_Hand`) furthest from the pelvis; a footjob the toes (`RLeg_Toe1`, `LLeg_Toe1`). `4Stim.log` says how many peak finders and sounds each scene got (`Peaks:`). OStim's use Skyrim's genital bones. With an extended skeleton (ZeX), `Penis_00`, `Vagina_00` and `Anus_00` can be used too. The head is `HEAD`.

In a scene file, each action can say (OStim's fields):

| Field | Default | Meaning |
|---|---|---|
| `doPeaks` | `true` | `false`: this action has no peaks here. |
| `peaksAnnotated` | `false` | `true`: peaks come only from the animation's `4StimPeak` annotations (below), not from the action's `peak`. |
| `muted` | `false` | `true`: this action plays none of its sounds here. |

## Events

Events are in `Data\F4SE\Plugins\4Stim\Events\`, one per file, the id being the file name, in OStim's format (OStim: `SKSE\Plugins\OStim\events`). 4Stim comes with OStim's: `spank` (and `spankleft`, `spankright`), `wash` and its kinds (`washback`, `washchest`, `washface`, `washfeet`, `washhands`, `washhead`).

```json
{
	"actor": { "stimulation": 2.5, "maxStimulation": 50, "reactionDelay": 0.5 },
	"target": { "stimulation": 5.0, "maxStimulation": 75, "reactionDelay": 0.5 },
	"file": "fx/4Stim/effects/spank",
	"cameraShakeStrength": 0.5,
	"cameraShakeDuration": 0.1,
	"controllerRumbleStrength": 0.3,
	"controllerRumbleDuration": 0.1,
	"tags": [ "sensual" ]
}
```

- `actor`, `target`, `performer`: who did it, who it was done to, who's moving. For each: `stimulation` added to their excitement at once (with their multiplier), only while it's below `maxStimulation` (so spanking alone can't make anyone climax), and `reactionDelay` (seconds) before they react: their voice set's reaction to this event, if it has one (docs/SOUND.md).
- The sound plays on the event's actor: OStim's `sound` (a sound descriptor form), or 4Stim's `file` (a sound file or folder under `Data\Sound\`), at `soundVolume`.
- `cameraShakeStrength` / `Duration` (0-1, seconds) and `controllerRumbleStrength` / `Duration`: only in a scene you're in or watching (OStim shakes for any scene), and only with shakes on (`fClimaxShake` above 0) and rumble on (`SetUseRumble`).
- `supertype`: another event whose values this one uses where it has none (`spankleft` is a `spank`). Reactions look for the event, then its supertype.
- `tags`: kept for other mods.

### Firing events

- **Animations:** the `4StimEvent` annotation (OStim's `OStimEvent`): `4StimEvent.spank,1` (the actor playing this animation spanked role 1) or `4StimEvent.spank,0,1,0` (role 0 spanked role 1, role 0 moving). The animation converter should turn OStim's annotations into these.
- **Papyrus:** `FourStim.CallEvent(akInScene, "spank", aiActor, aiTarget, aiPerformer)` (roles of the scene akInScene is in; `aiActor` -1 = akInScene's own). `FourStim.GetEvents()` lists them.
- **Listening:** scripts registered with `FourStim.RegisterForSceneEvents` get `FourStim_OnEvent(String asEvent, Actor akActor, Actor akTarget, Actor akPerformer, Actor[] akActors, String asSceneID)`.

## Other annotations

| Annotation | OStim's | What it does |
|---|---|---|
| `4StimClimax` | `OStimClimax` | The actor climaxes (docs/EXCITEMENT.md). |
| `4StimEvent.<event>,...` | `OStimEvent` | Fires an event (above). |
| `4StimPeak.<action>` | `OStimPeak` | A peak of the scene's action number `<action>` (0 = the first), for actions with `peaksAnnotated`. |
| `4StimPlayExpression.<event>` | `OStimPlayExpression` | The actor makes that event expression (`moan`, `climax`...), with no sound (docs/EXPRESSIONS.md). |
| `4StimBendSoS.<steps>` / `4StimResetSoS` | `OStimBendSoS` / `OStimResetSoS` | The actor's penis bends that many SoS steps more, until reset or the next scene (docs/ALIGNMENT.md). |
| `4StimEquipObject.<type>` / `4StimUnequipObject.<type>` | `OStimEquipObject` / `OStimUnequipObject` | The actor puts on / takes off an equip object of that type (docs/ACTORS.md). |

Annotations count for as long as the actor is in a scene.

## Not in yet

OStim's sex toys (which also follow peaks), and muting actors from Papyrus (`muteWithActor` / `muteWithTarget` on action sounds are read but nothing mutes an actor yet).
