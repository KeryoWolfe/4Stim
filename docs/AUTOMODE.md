# Auto mode

Auto mode moves a scene on by itself, the way OStim's auto mode does. Scenes without the player start in it, and scenes with the player don't, unless `4Stim.ini` says otherwise (`[AutoMode]`). The auto mode key (`iAutoModeKey`, U by default) turns it on or off for the scene you're in or watching. Scripts can do the same with `FourStim.SetAutoMode`.

## What it does

- **Stages:** a two-or-more-actor scene in auto mode goes through up to three stages:
  - **Foreplay** (in `iForeplayChance` percent of scenes): sexual scenes without intercourse (no `vaginalsex`, `analsex` or `tribbing` action), until someone's excitement passes a random point between `fForeplayEndMin` and `fForeplayEndMax`.
  - **Intercourse:** scenes with one of those actions.
  - **Pull-out** (in `iPulloutChance` percent of scenes): when a man's excitement passes a random point between `fPulloutMin` and `fPulloutMax` during intercourse, the scene goes to his role's `pullout` auto transition. Without one, it goes to a nearby scene where he finishes by hand (`malemasturbation`, no intercourse). It then waits for the climax and goes back to intercourse.
- **Solo scenes:** a one-actor scene picks masturbation scenes for the actor's sex.
- **Timing:** every `fAutoModeSceneMin` to `fAutoModeSceneMax` seconds it picks a random scene for the stage. The pick must fit the actors in their roles (`bMatchSex`), the furniture they're on and the scene's actions. It skips transitions and scenes marked `noRandomSelection`. Off furniture it prefers scenes where someone stands, and on a bed scenes where no one does, when there are any (`bAutoModeStandingOnFloor`, as OStim).
- **Getting there:** if the picked scene is within `iAutoModeMaxSteps` navigations, auto mode walks there through the navigation links. Transitions play in full, and other scenes on the way play for half a second. Further scenes are played directly. Once a scene is sexual, it only picks scenes it can walk to (`bAutoModeLimitToNavigation`).
- **Speed:** every `fAutoSpeedIntervalMin` to `fAutoSpeedIntervalMax` seconds it may speed up one step. The chance is 0 at `fAutoSpeedExcitementMin` excitement and certain at `fAutoSpeedExcitementMax` (`bAutoSpeed`). A scene auto mode moves to starts at its `defaultSpeed`.
- **Picking by hand:** a scene you pick yourself while auto mode is on stays for at least `fAutoModeSceneMin` seconds.

Auto mode waits while the scene is in a transition, a sequence, or ending.

## Scene files

These scene fields matter to auto mode (`SCENES.md`):

- `actions`: what auto mode picks by (`ACTIONS.md`).
- `noRandomSelection`: auto mode never picks the scene at random.
- `defaultSpeed`: the speed it starts the scene at.
- Actor `tags`: `standing` is used by `bAutoModeStandingOnFloor`.
- Actor `autoTransitions`: `pullout`.

## Papyrus

`FourStim.psc` has `SetAutoMode`, `IsAutoMode` and `AutoTransition`.
