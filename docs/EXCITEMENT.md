# Excitement and climax

Every actor in a running scene has an **excitement** from 0 to 100, shown on the HUD's actor meters. At 100 the actor **climaxes**. It works like OStim's. The settings are in the `[Excitement]` section of `4Stim.ini`.

## How excitement rises

A scene's actions (`ACTIONS.md`) give each side a `stimulation`, in excitement per second. For each actor:

- **Rate:** the strongest stimulation they get from any action in full, plus a tenth of each other one. An actor who is both actor and target of an action (masturbation) gets both sides.
- **Speed:** the rate is multiplied by `1 + speed / number of speeds`, so the fastest speed of a three-speed scene adds two thirds.
- **Multipliers:** then by `fMaleExcitementMult` or `fFemaleExcitementMult`, and by the actor's own multiplier (`SetExcitementMultiplier`).
- **Ceiling:** excitement only rises to the highest `maxStimulation` of the actions giving them anything. Kissing stops at 50, so a kissing scene alone never brings anyone to climax. A scene that gives an actor nothing has a ceiling of 0.
- **Falling back:** above the ceiling, excitement falls by `fExcitementDecayRate` per second, after `fExcitementDecayGrace` seconds.

## Climax

At 100 the actor climaxes:

- **Counted:** `GetTimesClimaxed` goes up by one, and a man's excitement starts over at 0. A woman's starts at `fPostClimaxExcitement` times her climaxes so far, up to `fPostClimaxExcitementMax`.
- **Animation:** if the actor's role in the scene has a `climax` scene (`SCENES.md`), it plays. That's usually a transition back to the scene, at the slowest speed for a man. Without one, a man's climax drops the scene to its slowest speed. Set `bClimaxScenes=0` to turn climax scenes off.
- **Event:** registered scripts get `FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)` (see `SceneEvents.h` / `FourStim.RegisterForSceneEvents`).
- **Effects:** if the player is in the scene or watching it, the moment the climax happens: the camera shakes very slightly (the free camera is shaken by the plugin, since the game's own shake doesn't move it), the screen blurs briefly, a faint white glow rises along the screen's edges and fades (the HUD's `PlayClimax`), and the controller rumbles. `fClimaxShake`, `fClimaxBlur`, `fClimaxFlash` and `bClimaxRumble` set or turn off each one.
- **Ending:** the scene ends `fClimaxEndDelay` seconds later if the settings say so. Scenes with the player use `bEndOnPlayerClimax`, `bEndOnMaleClimax` and `bEndOnFemaleClimax`, or with `bEndOnAllClimax=1`, they end once everyone has climaxed. Scenes without the player use `bEndNPCScenesOnClimax`.

A **stalled** actor (`StallClimax`) stays at 100 without climaxing until released. `Climax(akActor)` makes an actor climax right away.

## Papyrus

`FourStim.psc` has these functions:

- `GetExcitement`, `SetExcitement` and `AddExcitement`
- `GetTimesClimaxed`, `Climax`, `StallClimax` and `IsClimaxStalled`
- `GetExcitementMultiplier` and `SetExcitementMultiplier`
- `GetTimeUntilClimax`

Excitement lives only as long as the actor's scene. It starts at 0 in every new scene, and it isn't saved.
