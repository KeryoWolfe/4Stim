# Decisions

Where 4Stim departs from OStim NG, and why (roadmap 1.8). The rule
(CLAUDE.md) is to do what OStim does; each entry here is either **forced by
Fallout 4** or **4Stim's own** (an addition the owner asked for, or one that
OStim's behaviour needs in Fallout 4). The owner's other decisions
(branches, dependency versions, animation permissions) are in
docs/PORTING_ROADMAP.md, "Decided by the owner".

Each entry: what OStim does, what 4Stim does, why, and where it's described.

## Playing animations

**Idle records instead of behavior patches** (forced). OStim's animations
are added to Skyrim's behavior graphs by a behavior patcher, and OStim plays
them by animation event. Fallout 4 has no behavior patcher for that, so
4Stim plays each role's animation through an Idle record in a plugin
(`<animation>_<role>`, on the human behavior graph), named after OStim's
animations so OStim's scene files work unchanged. docs/SCENES.md, "The
plugin: Idle records".

**`playbackSpeed` by animation update** (forced). OStim sets its
`OStimSpeed` behavior variable, which only its Skyrim behavior patch has.
4Stim speeds up or slows down the actors' animation updates instead
(`bUsePlaybackSpeed`). docs/SCENES.md, docs/API.md.

**Transitions played once** (forced). Converters write `dyn_ActivationLoop`
for every Idle, and a looping idle blends back from its first frames for a
moment; 4Stim plays a transition's Idles as `dyn_Activation`. docs/SCENES.md.

**First-play hitch kept** (owner, 13 November 2026). The game loads an
animation file the first time it plays (40-120 ms measured); preloading
needs reverse engineering CommonLibF4 doesn't have, and OStim doesn't
preload either. docs/TODO.md, Later.

## Holding actors in place

**The scene lock** is OStim's: each actor translated to their spot
(`TranslateTo`) for the whole scene. **4Stim's own around it:** the scene
guard puts back anyone more than 4 units or 3 degrees off their spot,
scene actors stop colliding with each other and with the scene's furniture
(beds stay solid), and a collision watch. Fallout 4's NPCs, furniture and
physics push actors out of paired animations that Skyrim's leave alone.
docs/ALIGNMENT.md.

**Locking actors through Papyrus** (forced). `SetRestrained`, `EnableAI`,
`BlockActivation` and `AllowPCDialogue` have no direct function in
CommonLibF4, and restraining has to happen on Papyrus's thread; 4Stim calls
them through the VM. `SetScale` and `EvaluatePackage` are called directly
(`bDirectNatives`, `iEvaluatePackage`). Roadmap 2b.6.

**Interaction blocked before an NPC is moved** (forced). A Fallout 4 NPC
moved next to the player can greet them within a second and open dialogue
on its own, so activation and dialogue are blocked before the move, not
after. `src/Core/Playback.cpp`, `SuppressInteraction`.

**Every scene actor's weapon sheathed** (forced). OStim sheathes the
player's (`sheatheWeaponry`, through Skyrim's `SheatheWeapon`). Fallout 4's
Papyrus has no `SheatheWeapon`, and an NPC whose weapon is only taken off
stays in the drawn pose with bare fists, over the scene's animations (the
owner's logs); 4Stim calls the game's own sheathe for every scene actor.
`src/Core/Scenes.cpp`, `SheatheNow`.

## Bodies

**Erections by each body's own means** (forced). OStim leaves erections to
Schlongs of Skyrim (`SOSBend<n>`, `SOSFlaccid`). Fallout 4 has no SoS; each
male body has its own way (physics swaps, LooksMenu morphs, the bodysuit's
meshes), and 4Stim uses whichever the actor has. OStim's events are kept:
the bend, flaccid at -10, flaccid after the scene. docs/ERECTION.md.

**Who has a penis, from the body** (forced). OStim asks SoS's or TNG's
factions when no actor property decides (`useSoSSex`, `useTNGSex`). Fallout
4 has neither; 4Stim looks at what the actor wears and their naked body for
a mesh on ZeX's `Penis_*` bones or named like a penis (`bPenisFromBody`,
`sPenisMeshNames`). The body can only add a penis: under clothes a man's
body isn't drawn. docs/ACTORS.md.

**`carried` actor property condition** (4Stim's own). Items in the
inventory, worn or not: a futa mod that takes its penis off while the actor
is clothed still leaves it there. docs/ACTORS.md.

**Physics swaps** (4Stim's own). SMP bodies get rigid (or receiving)
physics for the length of a scene through FSMP's `SwapPhysicsFile`, as
A-Body does for AAF scenes. OStim has no counterpart; without it Fallout 4's
SMP genitals stay floppy in scenes. docs/PHYSICS.md.

**OStim's properties with no Fallout 4 counterpart left out**: vampire,
beast race, tail and in-water actor properties; the vampire and tailjob
animations (their action types stay, for other packs). RaceMenu height isn't
read (Fallout 4 has none). docs/ACTORS.md, docs/SCALING.md.

## Faces and equipment

**Expressions as Fallout 4 face morphs** (forced). OStim's expression files
are read unchanged and turned into Fallout 4's face morphs instead of
Skyrim's. docs/EXPRESSIONS.md.

**Tongue face held while a tongue is on** (forced). Fallout 4 has no jaw
bone, so a tongue in a closed mouth can't be seen; 4Stim holds the mouth
open while the tongue is worn (OStim shows tongues only with the faces that
call for them). docs/ACTORS.md.

**Gear the game takes off is put back** (forced, 4Stim's). Giving an NPC
an equip object makes Fallout 4 choose their gear again, which takes off
things a script put on them; 4Stim puts them back a moment later.
docs/ACTORS.md.

## Interface

**4Stim's own HUD** (forced). OStim's scene UI is built for Skyrim's
interface; 4Stim's HUD is its own Scaleform movie (ActionScript 3) with a
fixed API, so it can be restyled or replaced, offering OStim's navigation,
options and meters. docs/HUD_API.md, docs/OPTIONS.md.

**Fallout 4's MCM** (forced). OStim's settings pages are SkyUI's; 4Stim's
are Fallout 4's Mod Configuration Menu, with OStim's pages, order, labels
and help texts, and 4Stim's own settings after them. Without the MCM,
`4Stim.ini` has everything. docs/MCM.md.

**Translations in Fallout 4's format** (forced). UTF-16 `$key<tab>text`
files, handed to the game's translator for the MCM; packs' `$` keys are
translated the way OStim's `LocaleManager` does. docs/TRANSLATIONS.md.

## Engine and API

**Runtime Database for other game versions** (forced). On 1.11.240 4Stim
uses the Address Library, as OStim does on Skyrim; other versions have
none, so CommonLibF4RD's resolver is built in (experimental).
docs/OLDGEN.md.

**The C++ API's names** (4Stim's own). Namespace `FourStimAPI` and message
type `'4ST'`, so a plugin can talk to both; scenes passed by ID, not as
OStim's `Graph::Node` objects. docs/API.md.

**"4Stim" for OStim's names** (owner, CLAUDE.md). Commands, events,
annotations, tags and labels OStim names "OStim" take "4Stim" (OStim's
`OStimClimax` is `4StimClimax`); OStim's own names and files stay as they
are.

**Scenes can resume after a load** (4Stim's own, off by default).
`bResumeScenes=0` ends scenes on load, as OStim; 1 resumes them.
docs/SETTINGS.md.
