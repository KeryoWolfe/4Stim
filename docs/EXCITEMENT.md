# Excitement and climax

Every actor in a running scene has an **excitement** from 0 to 100, shown on the HUD's actor meters. At 100 the actor **climaxes**. It works like OStim's. The settings are in the `[Excitement]` section of `4Stim.ini`.

## How excitement rises

A scene's actions (`ACTIONS.md`) give each side a `stimulation`, in excitement per second. For each actor:

- **Rate:** the strongest stimulation they get from any action in full, plus a tenth of each other one. An actor who is both actor and target of an action (masturbation) gets both sides.
- **Speed:** the rate is multiplied by `1 + speed / number of speeds`, so the fastest speed of a three-speed scene adds two thirds.
- **Multipliers:** then by `SetsexExcitementMult` or `SetFemaleSexExcitementMult`, and by the actor's own multiplier (`SetExcitementMultiplier`).
- **Ceiling:** excitement only rises to the highest `maxStimulation` of the actions giving them anything. Kissing stops at 50, so a kissing scene alone never brings anyone to climax. A scene that gives an actor nothing has a ceiling of 0.
- **Falling back:** above the ceiling, excitement falls by `excitementDecayRate` per second, after `excitementDecayGracePeriod` milliseconds (OStim's units).

## Climax

At 100 the actor climaxes:

- **Waiting for the animation:** if the actor's role has a `climax` scene and `SetAutoClimaxAnims` is on, it plays first, and the climax itself (everything below) happens at the animation's **`4StimClimax`** annotation (an animation event; OStim's is `OStimClimax`), or when the scene moves on from it if the animation has none. Without a climax scene, the climax happens at once.
- **Counted:** `GetTimesClimaxed` goes up by one, and a man's excitement starts over at 0. A woman's starts at `postOrgasmExcitement` times her climaxes so far, up to `postOrgasmExcitementMax`.
- **Speed:** a man's climax drops the scene to its slowest speed (after a climax scene, the scene it goes back to starts slowest). The climax scene (`SCENES.md`) is usually a transition back to the scene. Set `SetAutoClimaxAnims=0` to turn climax scenes off.
- **Event:** registered scripts get `FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)` (see `SceneEvents.h` / `FourStim.RegisterForSceneEvents`).
- **Effects:** if the player is in the scene or watching it, the moment the climax happens: the camera shakes very slightly (the free camera is shaken by the plugin, since the game's own shake doesn't move it), the game goes into slow motion for 2.5 seconds, the screen blurs briefly, a faint white glow rises along the screen's edges and fades (the HUD's `PlayClimax`), and the controller rumbles. `SetSlowMoOrgasms`, `fClimaxShake`, `SetBlurOrgasms` (and `fClimaxBlur`, its strength), `fClimaxFlash` and `SetUseRumble` set or turn off each one. Slow motion is OStim's (0.3x speed for 2.5 s, through the game timer's time multiplier, Fallout 4's `sgtm`); the shake strength and the glow are 4Stim's.
- **Ending:** the scene ends `fClimaxEndDelay` seconds later if the settings say so. Scenes with the player use `endOnPlayerOrgasm`, `SetEndOnOrgasm` and `SetEndOnSubOrgasm`, or with `SetEndOnBothOrgasm=1`, they end once everyone has climaxed. Scenes without the player use `endNPCSceneOnOrgasm`.

A **stalled** actor (`StallClimax`) stays at 100 without climaxing until released. `Climax(akActor)` makes an actor climax right away.

## Papyrus

`FourStim.psc` has these functions:

- `GetExcitement`, `SetExcitement` and `AddExcitement`
- `GetTimesClimaxed`, `Climax`, `StallClimax` and `IsClimaxStalled`
- `GetExcitementMultiplier` and `SetExcitementMultiplier`
- `GetTimeUntilClimax`

Excitement lives only as long as the actor's scene. It starts at 0 in every new scene, and it isn't saved.
