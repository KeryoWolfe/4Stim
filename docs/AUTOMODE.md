# Auto mode

Auto mode moves a scene on by itself, the way OStim's auto mode does. Scenes without the player start in it (`bAutoModeNPC`; OStim's always do). Scenes with the player start in it with `SetAIControl`, or, without that, solo scenes with `SetForceAIForMasturbation` and scenes with partners with `SetForceAIInConsensualScenes` (all off by default, as OStim's). A thread built with `NoAutoMode` never runs it, and `NoPlayerControl` doesn't turn it on (both as OStim's). The auto mode key (`SetControlToggle`, U by default) turns it on or off for the scene you're in or watching. Scripts can do the same with `FourStim.SetAutoMode`. OStim's pull-out key (`SetPullOut`, unbound by default) pulls out by hand, the same way auto mode's pull-out stage does.

## What it does

- **Stages:** a two-or-more-actor scene in auto mode goes through up to three stages:
  - **Foreplay** (in `autoModeForeplayChance` percent of scenes): sexual scenes without intercourse (no `vaginalsex`, `analsex` or `tribbing` action), until someone's excitement passes a random point between `autoModeForeplayThresholdMin` and `autoModeForeplayThresholdMax`.
  - **Intercourse:** scenes with one of those actions.
  - **Pull-out** (in `autoModePulloutChance` percent of scenes): when a man's excitement passes a random point between `autoModePulloutThresholdMin` and `autoModePulloutThresholdMax` during intercourse, the scene goes to his role's `pullout` auto transition (4Stim only pulls out of intercourse scenes; OStim's pull-out goes by his excitement in any scene of the main stage). Without one, it goes to a nearby scene where he finishes by hand (`malemasturbation`, no intercourse). It then waits for the climax and goes back to intercourse.
- **Solo scenes:** a one-actor scene picks masturbation scenes for the actor's sex.
- **Timing:** every `autoModeAnimDurationMin` to `autoModeAnimDurationMax` milliseconds (7.5 to 15 s) it picks a random scene for the stage. The pick must fit the actors in their roles (`SetOnlyGayAnimsInGayScenes`), the furniture they're on and the scene's actions. It skips transitions and scenes marked `noRandomSelection`. Off furniture it prefers scenes where someone stands, and on a bed scenes where no one does (`bAutoModeStandingOnFloor`). A difference: OStim requires it, and doesn't move when no scene passes; 4Stim drops the rule then, which keeps auto mode going with small packs.
- **Getting there:** if the picked scene is within `navigationDistanceMax` navigations, auto mode walks there through the navigation links. Transitions play in full, and other scenes on the way play for half a second. Further scenes are played directly. Once a scene is sexual, it only picks scenes it can walk to (`autoModeLimitToNavigationDistance`).
- **Speed** (OStim's speed control: it runs in every NPC scene, and in your scenes with `SetActorSpeedControl`, in auto mode or not; not while a sequence, a route or a transition moves the scene): every `autoSpeedControlIntervalMin` to `autoSpeedControlIntervalMax` milliseconds (2.5 to 7.5 s) it may speed up one step. The chance is 0 at `autoSpeedControlExcitementMin` excitement and certain at `autoSpeedControlExcitementMax` (`SetActorSpeedControl`). Any scene starts at its `defaultSpeed`; a scene moved to keeps the pace, as OStim's: the speed's place among the old scene's speeds (speed / number of speeds) is put on the new one's, rounded, so a transition's destination, a navigation or an auto transition goes on about as fast. Auto mode's own moves end at the scene's `defaultSpeed`.
- **Picking by hand:** a scene you pick yourself while auto mode is on stays for at least `autoModeAnimDurationMin`.

Auto mode waits while the scene is in a transition, a sequence, or ending.

**Fades:** in a scene with the player, with `SetUseAutoFades=1` (off by default, as OStim), or when the scene it goes to is marked `fadeOnEntry`, the screen fades to black as auto mode moves on, as OStim's does.

## Scene files

These scene fields matter to auto mode (`SCENES.md`):

- `actions`: what auto mode picks by (`ACTIONS.md`).
- `noRandomSelection`: auto mode never picks the scene at random.
- `defaultSpeed`: the speed the scene starts at, and auto mode moves to it at.
- Actor `tags`: `standing` is used by `bAutoModeStandingOnFloor`.
- Actor `autoTransitions`: `pullout`.

## Papyrus

`FourStim.psc` has `SetAutoMode`, `IsAutoMode` and `AutoTransition`.
