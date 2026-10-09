# To do

## V1.0 cleanup (before release)

- **Short hitch when picking a transition.** Choosing a scene that starts a transition freezes the game very briefly. It's likely the game loading the transition's animation file on first play; worth measuring (time from the pick to `PlayIdle` returning in the log) and, if so, preloading the clips of a scene's transitions while it plays.
- **Converter: transitions should play once.** The converter writes every idle with `dyn_ActivationLoop`; transition idles need `dyn_Activation` (see SCENES.md). Regenerating the test pack's esp currently undoes this.
- **Converter: hold the last pose.** Offer `hold_end.py` (0.5 s held last pose for transition clips) as an option in the converter GUI and pipeline instead of a separate step.

- **Scene events' actor arrays.** Actors passed from native code to Papyrus arrived as a "type mismatch" in the physics swap (fixed there by passing form IDs). The scene events (`FourStim_OnSceneStart` and the rest) pass `Actor[]` the same way; check them with a listening script, and pass form IDs if they're broken too.

## Converter rebuild

- **Rebuild the animation converter both ways: Skyrim to Fallout 4 and Fallout 4 to Skyrim.** One tool with a proper bone map and retargeting between the two skeletons, and the right Havok output for each game (FO4's hk_2014 and Skyrim SE's 64-bit hk_2010 packfiles). It should keep annotations and root motion, and build each game's plugin (FO4 IDLE records: one-shot for transitions, looping otherwise). It should also write 4Stim scene files and hold the last pose of transition clips. The two converter items above fold into this.

## Later

- Excitement system
- PrismaUI
- NPC greetings
- Race filtering
- Claim registry
- Group scenes
- First-person camera
- A sequence-end event for other mods
