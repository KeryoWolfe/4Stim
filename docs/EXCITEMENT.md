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
- **When:** as OStim's, the climax waits until the moan the actor is making has ended (the count, the event, the speed drop, the effects and the end timer all wait); no new moan starts meanwhile.
- **Counted:** `GetTimesClimaxed` goes up by one, and a man's excitement starts over at 0. A woman's starts at `postOrgasmExcitement` times her climaxes so far, up to `postOrgasmExcitementMax`.
- **Speed:** a man's climax drops the scene to its slowest speed (after a climax scene, the scene it goes back to starts slowest). The climax scene (`SCENES.md`) is usually a transition back to the scene. Set `SetAutoClimaxAnims=0` to turn climax scenes off.
- **Event:** registered scripts get `FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)` (see `SceneEvents.h` / `FourStim.RegisterForSceneEvents`).
- **Effects:** if the player is in the scene or watching it, the moment the climax happens: with OStim's `SetUseScreenShake` on (off by default, as OStim's) the camera shakes very slightly (the free camera is shaken by the plugin, since the game's own shake doesn't move it; OStim shakes only a first-person camera, which scenes don't use here), the game goes into slow motion for 2.5 seconds, the screen blurs briefly (only for the player's own climax, as OStim's), a faint white glow rises along the screen's edges and fades (the HUD's `PlayClimax`), and the controller rumbles. `SetSlowMoOrgasms`, `fClimaxShake`, `SetBlurOrgasms` (and `fClimaxBlur`, its strength), `fClimaxFlash` and `SetUseRumble` set or turn off each one. Slow motion is OStim's (0.3x speed for 2.5 s, through the game timer's time multiplier, Fallout 4's `sgtm`); the shake strength and the glow are 4Stim's.
- **Tired:** a climax takes 250 Action Points (OStim's 250 Stamina; Fallout 4's nearest is AP).
- **Auto mode** moves on to its main stage after any climax, from foreplay too, as OStim's.
- **Ending:** the scene ends `fClimaxEndDelay` seconds later if the settings say so, as OStim's: every scene uses `endOnPlayerOrgasm`, `SetEndOnOrgasm` and `SetEndOnSubOrgasm` (or with `SetEndOnBothOrgasm=1`, it ends once everyone has climaxed), and scenes without the player also end on any climax with `endNPCSceneOnOrgasm`. Each climax that counts starts the delay again.

A **stalled** actor (`StallClimax`) stays at 100 without climaxing until released. `Climax(akActor)` makes an actor climax right away.

## Papyrus

`FourStim.psc` has these functions:

- `GetExcitement`, `SetExcitement` and `AddExcitement`
- `GetTimesClimaxed`, `Climax`, `StallClimax` and `IsClimaxStalled`
- `GetExcitementMultiplier` and `SetExcitementMultiplier`
- `GetTimeUntilClimax`

Excitement lives only as long as the actor's scene. It starts at 0 in every new scene. It's saved with the game, so a scene started again from a save goes on from where it was (OStim's threads end on load, so OStim has nothing to save); times climaxed and climax stalls aren't saved.

## Per-actor stimulation (OStim's OData)

An actor can have their own stimulation and max stimulation for one side of an action or event, in place of the action's or event's own: OStim's per-actor action and event stimulation. They're kept by the actor's base form, saved with the game, and set from Papyrus (`FourStim.SetActionStimulation(aiRole, aiFormID, asAction, afValue)`, `SetActionMaxStimulation`, `Get...`, `Reset...`, the same for events; OStim's `OData` functions, `aiRole` 1 actor, 2 target, 4 performer). A scene picks them up from the next scene change. OStim sets them from its MCM's excitement page, per actor; 4Stim's MCM has no per-actor pages, so for now it's Papyrus only.

