# To do

## V1.0 cleanup (before release)

- **Short hitch when picking a transition.** Choosing a scene that starts a transition freezes the game very briefly. It's likely the game loading the transition's animation file on first play; worth measuring (time from the pick to `PlayIdle` returning in the log) and, if so, preloading the clips of a scene's transitions while it plays.
- **Converter: transitions should play once.** The converter writes every idle with `dyn_ActivationLoop`; transition idles need `dyn_Activation` (see SCENES.md). Regenerating the test pack's esp currently undoes this.
- **Converter: hold the last pose.** Offer `hold_end.py` (0.5 s held last pose for transition clips) as an option in the converter GUI and pipeline instead of a separate step.

## Later

- Excitement system
- PrismaUI
- NPC greetings
- Race filtering
- Claim registry
- Group scenes
- First-person camera
- A sequence-end event for other mods
