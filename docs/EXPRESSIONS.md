# Facial expressions

Actors in a scene make faces as in OStim: a calm or excited look picked from what they're doing, a moan face while they moan, a climax face, and an open mouth or tongue where an action calls for it. 4Stim reads **OStim's own expression files**, unchanged, and turns them into Fallout 4 face morphs.

`noFacialExpressions=1` in `4Stim.ini` turns them off (OStim's setting).

## Where they come from

Expression files are in `Data\F4SE\Plugins\4Stim\Facial Expressions\` (subfolders too), one per file, in OStim's format (OStim: `SKSE\Plugins\OStim\facial expressions`). The 80 that come with 4Stim are OStim's. Each one says when it's used and what the face does:

```json
{
  "sets": ["default"],
  "actionActors": ["blowjob"],
  "actionTargets": ["cunnilingus"],
  "events": ["moan"],
  "female": {
    "duration": 1.5,
    "expression": { "type": 10, "baseValue": 60, "variance": 20, "excitementMultiplier": 0.3 },
    "modifiers": [ { "type": 0, "baseValue": 30 } ],
    "phonemes": [ { "type": 1, "baseValue": 40, "speedMultiplier": 10 } ]
  },
  "male": { ... }
}
```

- `sets`: named sets it belongs to (`default`, `openmouth`, `tongue`...). `actionActors` / `actionTargets`: actions it's used for, by the actor doing them or the one they're done to. `events`: `moan`, `climax`.
- `female` / `male`: the face for each sex. `expression` (Skyrim's emotion, one), `modifiers` (eyes and brows), `phonemes` (the mouth). Each value is `baseValue` (0-100) plus a random 0-`variance`, plus `speedMultiplier` × the scene's relative speed and `excitementMultiplier` × the actor's excitement, as in OStim. `delay` is read but not used yet.
- **4Stim adds `morphs`:** a list like `modifiers`, but its `type` is a Fallout 4 morph index (below), for files made for Fallout 4.

## Layers

As in OStim, each actor's face is built from up to three layers. A higher layer replaces the lower ones only in the parts of the face it sets (the emotion, the mouth, the eyelids, the brows, and 4Stim's morphs):

1. **Underlying:** picked from the scene's actions (the role's `underlyingExpression` set or `expressionAction` if the scene names one, else files whose `actionActors` / `actionTargets` match the role's actions), else from the `default` set. Picked again every `SetExpressionDurationMin` to `SetExpressionDurationMax` milliseconds (1000 / 3000, OStim's).
2. **Event:** `moan` while the actor moans (with sound, docs/SOUND.md), `climax` at a climax, for the file's `duration`.
3. **Override:** an action's `expressionOverride` (`openmouth` for oral, `tongue` for licking, in `Actions\Default.json`), while that action is in the scene.

Faces move smoothly toward what the layers give, faster for events and overrides, as OStim's expression updater does. When the actor leaves the scene, their face goes back to normal.

## Fallout 4's face

Skyrim has expressions, phonemes and modifiers; Fallout 4 has 50 face morphs instead. `Data\F4SE\Plugins\4Stim\Expression Morphs.json` maps each Skyrim type onto morphs, as `[morph, weight]` pairs: a type at strength 60 with `[17, 1.0], [4, 0.5]` sets morph 17 to 60 and morph 4 to 30. Where several types set one morph, the highest wins. Skyrim's looking modifiers (8-11) have no morph and do nothing. Tune the map freely; it's read again with `FourStim.ReloadScenes()`.

Morphs are set with the console command `mfg morphs <morph> <0-100>`, only when a value changes, at most 20 times a second.

| # | Morph | # | Morph | # | Morph | # | Morph | # | Morph |
|---|---|---|---|---|---|---|---|---|---|
| 0 | BrowSqueeze | 10 | LeftLowerEyeLidUp | 20 | LeftUpperLipDown | 30 | RightLipCornerIn | 40 | RightSmile |
| 1 | JawForward | 11 | LeftLowerLipDown | 21 | LeftUpperLipUp | 31 | RightLipCornerOut | 41 | RightUpperEyeLidDown |
| 2 | JawOpen | 12 | LeftLowerLipUp | 22 | LowerLipFunnel | 32 | RightLowerEyeLidDown | 42 | RightUpperEyeLidUp |
| 3 | LeftBrowOuterUp | 13 | LeftMiddleBrowDown | 23 | LowerLipRollIn | 33 | RightLowerEyeLidUp | 43 | RightUpperLipDown |
| 4 | LeftCheekUp | 14 | LeftMiddleBrowUp | 24 | LowerLipRollOut | 34 | RightLowerLipDown | 44 | RightUpperLipUp |
| 5 | LeftFrown | 15 | LeftNoseUp | 25 | Pucker | 35 | RightLowerLipUp | 45 | StickyLips |
| 6 | LeftJaw | 16 | LeftOuterBrowDown | 26 | RightBrowOuterUp | 36 | RightMiddleBrowDown | 46 | UpperLipFunnel |
| 7 | LeftLipCornerIn | 17 | LeftSmile | 27 | RightCheekUp | 37 | RightMiddleBrowUp | 47 | UpperLipRollIn |
| 8 | LeftLipCornerOut | 18 | LeftUpperEyeLidDown | 28 | RightFrown | 38 | RightNoseUp | 48 | UpperLipRollOut |
| 9 | LeftLowerEyeLidDown | 19 | LeftUpperEyeLidUp | 29 | RightJaw | 39 | RightOuterBrowDown | 49 | TongueLift |

(The names are Screen Archer Menu's.)

## Not in yet

OStim's eye and head `look` directions, expression `delay`, and expressions outside scenes.
