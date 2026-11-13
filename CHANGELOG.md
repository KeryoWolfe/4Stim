# Changelog

## 0.5.0 alpha (in progress)

### Who has a penis, from the body
- **Keyword conditions see keywords a script gave an actor** (`AddKeyword`), as Papyrus's `HasKeyword` does: before, only the race's and base's.
- **NPCs pick their package again directly** as they enter and leave a scene (`iEvaluatePackage=2`, the default now; 0 for Papyrus as before): tests showed the same packages either way, without going through the VM.
- **Timing for the freeze when picking a transition**: `Player scene timing:` lines in the log.
- **Clothed men read as having no penis**: the body check said "none" when a man's clothes hid his body. It can now only give a penis, never take one: otherwise it's the base sex.
- **A clothed futa read as a woman** (Futanari FEV takes its penis off while clothed): new actor property condition `carried` / `notCarried` (4Stim's own), items in the inventory, worn or not; the patch's property now uses it.
- **Futanari FEV patch: no erection at scene start**: the watcher's erect / flaccid functions used the script's empty `FutaChar` property instead of their parameter of the same name (the original's bug, hidden under AAF). Fixed; the patch also quiets the mod's polling script (errors every 4 seconds).
- **A futa's penis came off when a tongue came out**: giving an NPC an item makes the game choose their gear again, and it took off what a script had put on (Futanari FEV's penis). After putting on an equip object, 4Stim now puts back anything the actor lost that the object doesn't replace, Undress didn't take off, and nothing else now fills (`Equip objects: … lost … putting back` in the log).
- **Futanari FEV patch: the Cure didn't stick**: the mod's reveal spell, cast on every futa every 4 seconds, gave the penis back right after the Cure took it. The patch now ships the Reveal script too (it does nothing for an actor without the futa keyword), and its Cure takes the actor off the mod's list before the penis (the original never could: FutaFEV.esp leaves the Cure's list property empty, so the patch reads the list from the plugin) and removes it again half a second later.
- **A futa losing their penis mid-scene** (in the log, not on screen): when a tongue went on, the next scene change re-read the actors while the game was still adding the item and rebuilding the worn 3D, and saw neither the penis item nor its mesh. The worn-items read now holds the inventory's lock, and the body check keeps its last answer while an item that gave a penis is still being drawn again.
- **Futa still getting lesbian scenes** (Futanari FEV): the body check read only mesh names, and the game names a worn mesh after its armor (`Futanari Flaccid`), so it saw no penis. It now reads the mesh's material file too (`Materials\Futa\penis.bgsm`). The patch also gains a second actor property, on wearing the mod's flaccid or erect penis, and the log says for each actor whether a property file or the body gave them a penis.
- **Futanari FEV patch**: an actor property gives the mod's futa (its keyword) a penis dressed or not: the mod shows the penis only once they're naked, after 4Stim had picked the scene, so they got lesbian scenes.
- **`iEvaluatePackage` covers scene ends**: NPCs let go at a scene's end are evaluated through it too (`FourStim.EvaluatePackageOf`), where the test can tell "now" from "queued".
- **Aliases and magic effects can register for events**: `FourModEvents.RegisterForModEvent` and `FourStim.RegisterForSceneEvents` take any script (`ScriptObject`), as SKSE's does, not only a form's. Scripts compiled against the old `Form` versions keep working. Needed a fix in CommonLibF4 (a `ScriptObject` parameter couldn't be bound).
- **Futanari FEV patch** (`patches/Futanari FEV - 4Stim`): its AAF watcher replaced by one listening to 4Stim's scene events: the penis goes erect at a scene's start and flaccid at its end, without AAF.
- **Wearable penises from before ZeX's bones** (Bag of Dicks, Futanari FEV): their meshes are skinned to the pelvis, so the bone check missed them, and Undress took them off as clothing. A mesh named like a penis now counts too (`sPenisMeshNames=penis,schlong`), the log names the item that gives it, and Undress leaves that item on.
- **Scene check: talking no longer warns.** 4Stim's own moans count as talking, so every scene change warned ("Scene check: ... talking") with no chatter heard; talking is now only noted. A scene's idle not playing still warns.
- **Fixed: the picker froze the game** (this build's first try): the body check didn't recognise the main thread and queued itself as a task, which queued itself again, for ever. The queued look now runs directly, and the main thread is taken from inside an F4SE task (`kGameLoaded` comes on another thread), which also makes `FindFurniture`'s and `MigrateThread`'s main-thread checks right: before, they always answered no.
- **Penis from the body** (roadmap 2b.7; OStim's `useSoSSex` / `useTNGSex`): when no actor property says, anything an actor wears, their naked body included, with a mesh skinned to ZeX's `Penis_*` bones gives them a penis, so a wearable penis makes futa without an actor property. 4Stim's equip objects (the strap-on) never count. `Penis from body:` lines in `4Stim.log` name the mesh; `bPenisFromBody=0` goes back to the base sex.
- **`iEvaluatePackage` test switch** (roadmap 2b.6): 1 or 2 calls `EvaluatePackage` directly with false or true first, logging which package NPCs run before, right after and half a second later, to tell which means "now". 0 (the default) keeps Papyrus.

### Code: main.cpp split by area
- **Scale set directly** (roadmap 2b.6): an actor's scale for a scene, back after it and when a saved scene is let go or resumed is set at once (`TESObjectREFR::SetScale`), not through Papyrus a frame or more later. That also fixes a resumed aligned scene that could measure the actor before their own size was back. `bDirectNatives=0` in 4Stim.ini goes back to Papyrus.
- **JSON loaders stay on nlohmann** (roadmap 2b.4, deferred): loading takes 795 ms for 724 scenes on its own thread at startup, not worth risking files that load today.
- **Settings as a table** (roadmap 2b.2): `LoadSettings`' 189-branch `if (key == ...)` chain (split in seven for MSVC's limit on else-ifs) is now `SETTINGS` in `Core/Settings.cpp`, one line per setting with its clamp. Same reading as before, key by key; a new setting is one line there.
- **main.cpp split** (roadmap 2b.1 and 2b.3, a modder's review): its 14,000 lines are now `src/Core/` (one file per area: settings, scenes, threads, collision, auto mode, scene lock, camera, picker, hotkey, save data...; `Core.h` holds what they share) and `src/Natives/` (one file per Papyrus script, plus `Register.cpp`); main.cpp keeps the entry points. Code moved, not changed (`tools/refactor/split_main.py` did it). Map: docs/SOURCE_LAYOUT.md.

### Code: combined CommonLibF4
- CommonLibF4 is now KeryoWolfe/CommonLibF4's `4stim/combined` (Dear Modding plus the other forks' fixes; see its COMBINED.md). Physics.cpp no longer defines `IStackCallbackFunctor`'s destructor, which the library now does.

### Code: one scheduler instead of a thread per delay
- **A scheduler thread** (roadmap 2b.5, `src/Scheduler.h`): every "a moment later" (a scene's end and release, auto mode and HUD fades, slow motion's end, collision back after stepping apart, IdleStop again, settings read again, undress checks, the penis bones' rest pose, waiting for a loading screen or dialogue, resuming saved scenes) waits in one queue on one thread instead of starting a thread each, and the autoplay clock runs on it too: 17 thread starts gone, several of them every scene. Same delays as before. Animation cue files are read on one worker thread. Still threads of their own: sex toy calls (a toy plugin may block for as long as a peak lasts, as in OStim), Intiface's connection, the scene preload, and a thread move by another plugin (rare, and it waits on the game).

### Code: natives in namespaces; scene events checked
- **Papyrus natives grouped by script** (roadmap 2b.1, a modder's review): the `T_` / `TB_` / `L_` / `M_` / `A_` / `AM_` / `AU_` / `F_` / `J_` / `ME_` prefixes became namespaces named after the Papyrus scripts (`Natives::FourThread::IsRunning`, `Natives::FourMetadata::GetName`...). Renames only: the code is otherwise unchanged (checked line by line). They're still in main.cpp; moving them into their own files comes with the main.cpp split (2b.3).
- **Scene events' actor arrays checked** (roadmap 2.9): at the first scene of a session, `4Stim.log` says what type the scene events' actor list reaches Papyrus as (`Scene events: actors reach Papyrus as Actor[] ...: fine`, or a warning if it isn't `Actor[]`).
- **Tongues moved** 1.5 up and 1.5 further back, into the mouth (they came out of nothing at the lower lip, clipping the teeth).

### Eye directions: looking for the bones
- **OStim's eye directions** find the eye bones among the bones the head and eye meshes are skinned to when they aren't in the actor's 3D by name (the owner's log, 7 November: neither the player nor a ghoul had them by name, so no eyes moved), and the "no bones" log line now lists what the eye meshes are skinned to.

### Tongues for ghouls and by hand
- **Tongues drawn again**: the meshes rebuilt on 7 November had an empty bounding sphere on their head bone (the newer mesh library doesn't fill it), and Fallout 4 never draws a skinned shape like that; now set. Placed from HiPoly Faces' head and its JawOpen morph: 2.2 lower and 1.2 further back than head to head, on the open lower lip.
- **Helmets and hats come off visibly again**: when the game takes a head item off the player but doesn't redraw him (the same shapes drawn), 4Stim redraws him, as for body clothes. Since 5 November only body clothes got that (redrawing for gloves was a stutter), so a helmet taken off for a kiss stayed on his head.
- **Ghouls wear the 4Stim Equip Objects add-on's tongues and strap-on**: their armor addons now list the ghoul race too (they were humans only, so a ghoul got the item with nothing drawn). Rebuilt with `tools/make-equip-plugin.py`.
- **A tongue put on by hand holds the mouth open** (the `tongue` face) until it's taken off: put on from the HUD's options it sat inside a closed mouth, out of sight.

### NPC height and a crash
- **Scaling counts the race's height** (parity audit 2 §21, checked in game): Fallout 4 draws an actor at their base's height times their race's height for their sex (0.98 for human women), and 4Stim divided by the base's height only, so women came out about 2% small.
- **The collision watch only runs in the first 8 seconds after each scene plays**, not the whole scene: two crashes on 7 November were Havok's thread check tripping in another NPC's movement well into a scene, and the watch's physics reads every quarter second, alongside the game's own physics jobs, are the likeliest part of 4Stim to collide with them. Not proven: watch for the same crash (`hknpWorld::forceBodyPairCollisions` in a crash log).

### OStim's data, unchanged, checked against 4Stim
- **`tools/check-ostim-data.py`** (parity audit, "never considered" 1): lists every field in OStim's data files that 4Stim's loader for that kind never reads, and every action or furniture type OStim's scenes use that 4Stim lacks. First run on OStim NG: no missing ids, one skipped field OStim uses.
- **Expression delays**: a facial expression's modifier or phoneme `delay` / `delayVariance` (ms) now holds it back that long before it moves, as OStim's (they were read and ignored).
- **A Light page on each actor's HUD options** (OStim's facelight options): the light on or off, and which one.

Needs the plugin rebuilt and `FourStimOptions.psc` compiled.

### Tongues seated in the mouth
- **The 4Stim Equip Objects tongues** sit 2.5 lower and 0.6 further back: they came out from under the upper lip, floating above the open jaw (owner's screenshot, 7 November). Fallout 4's mouth is lower against the head bone than Skyrim's, and there's no jaw bone to follow, so they're placed for an open mouth (they only show with one). Rebuilt with `tools/refit/build_tongues.cpp`.

### Tier 4 of the parity audit: the small differences
- **Settings export / import in OStim's layout**: one flat list under the settings' names (`mcmsettings.json`); import also takes OStim's own file copied from Skyrim (leaving out its keys and dropdowns, and settings 4Stim doesn't have) and 4Stim's older files, and warns when nothing was imported. MCM.md now says why OStim's auto export / import don't exist here.
- **The MCM's volume sliders** run 0.1-1, as OStim's (they went to 2 while only 1 did anything).
- **Options**: a plain option's function gets `asState` "" as OStim's (`toggle` / `object` for equip-object rows).
- **Excitement after a climax** as OStim's: -3, or the post-climax value for anyone without a penis (and women with one unless `futaUseMaleExcitement`); a man's body counts as a man's climax for the ending settings, penis or not.
- **An actor property's `sex`** other than `male` / `female` leaves the penis to the body (OStim's agender).
- **Override expressions** fall through to the next set when one isn't installed (actor property, the role's own, then each action's); `"phonemes": []` closes the mouth.
- **Climax faces only with a climax sound**, as OStim's (no voice set, muted, or an entry without an expression: none); with `bSounds=0` the entry's face still shows.
- **Default voice sets as OStim's**: the women's sucking sound only for the mouth's side of `blowjob` / `deepthroating`, the men's set with no muffled sound. Voice-set `actions` take `"<action>:actor"` / `:target` / `:performer`.
- **Sex toy inputs as OStim's**: the stimulation rate without the sex and actor multipliers, the speed as speed / number of speeds.
- **Lights through the equip-object API** (type `light`, as OStim's): `SetEquipObjectID`, `SetFormEquipObjectID` (per base, race, sex), `EquipObject`, `UnequipObject`, `IsObjectEquipped` and options files with `"equipType": "light"`.
- **After a load**, actors let go from a scene (the player too) get their foot IK, head tracking and idle reset, as OStim's unlock.
- **The camera after a scene** is third person unless `SetForceFirstPerson`, as OStim's (4Stim went back to the view the scene started in).
- **Role choice**: "gay" or "straight" by who has a penis, as OStim's (it compared body sex).
- **API**: `FourThread.CallEvent` as OStim's (out-of-range positions do nothing, target -1 is the actor); `FourActor.GetExcitementMultiplier` -1 outside a scene, `FourMetadata.GetMaxSpeed` / `GetDefaultSpeed` 0 for an unknown scene; `IsMuted` right after `Mute` says muted; random scenes take the actors in the order given, as OStim's; new `FourEvent` (`IsChildOf`) and `FourData` (OStim's `OData`) scripts, `FourStim.IsEventChildOf`, `GetUndressingSlotMask`, `SetUndressingSlotMask`, `Localize`.
- **Plugin interface**: `getVersion()` gives 4Stim's packed version (0.5.0: 0x00050000), and plugins can ask for the interface from 4Stim's load on (as OStim's), not only after 4Stim's kPostLoad.
- **OStim's furniture type names** (`$ostim_furniture_bed`...) are translated.
- **Diagnostics**: scaling log lines give the NPC's height, height max and drawn scale, to check NPC height in game (audit 2 §21).

Needs the plugin rebuilt and Papyrus compiled (`FourStim.psc`, `FourStimScene.psc`, `FourThread.psc`, the new `FourData.psc` and `FourEvent.psc`); the MCM config and the translation file changed.

### The rest of Tier 3, as OStim's (parity audit)
- **Dialogue**: voice sets' `dialogue` lines (said with Fallout 4's `Say`) on actions that `talk`, after a random number of moans (OStim's countdown settings), climax comment lines, `postSceneDialogue` as a scene ends, `playerDialogue`, `preventSameVoiceCrossTalk`; `FourThreadBuilder.NoPostDialogue` works (docs/SOUND.md).
- **OStim's legacy voice-set syntax** (its own `defaultmale.json`), single-object entries and `soundExpression`; unread fields are logged.
- **`SetUseScreenShake`** (off by default, as OStim's) turns on climax and event shakes; **`SetCustomTimescale`** sets the time scale in scenes with you.
- **Furniture**: OStim's `offset*Global`, with its bed settings **`SetBedRealignment`** and **`bedOffset`** (MCM Furniture page); keywords checked on the reference too.
- **The hotkey held 2 seconds** starts a scene with you alone, as OStim's (a short press opens the picker as before).
- **Undressing**: **`SetRemoveWeaponsAtStart`** and **`SetRemoveWeaponsWithSlot`**; OStim's Papyrus undressing hook (`FourUndress.psc`); an item on an undress slot comes off when an action asks for any of its slots; undressing at the start ignores the first scene's `noStrip`.
- **Per-toy and per-slot toy settings** in `Toy Settings.json` (OStim's layout), with `individual` for `sToySynchronization` / `sToyScaling`.

Needs the plugin rebuilt and Papyrus compiled (`FourStim.psc`, `FourStimScene.psc`, `FourThreadBuilder.psc`, the new `FourUndress.psc`). New MCM settings on the General, Camera, Furniture, Undress, Sound and Toys pages.

### Finding the wall-scene shake
- **Fixed (needs testing):** the player invisible for a whole chair scene (owner's log, 6 November): the camera began fading him out as the scene started, before its fade was off, and the guard turned that into the game's fade-in, which never moved (`actor fade state 1, alpha 0.00`, over a thousand ticks). A scene actor's fade-out, or a fade-in still under way, now ends at once, fully shown.
- **`Shake watch:` lines with `bCollisionWatch=1`**: every frame, how far each scene actor (the reference and the drawn body) is from their spot and how often their movement turns back on itself, logged every 2 seconds. The owner's video of 6 November (a wall scene) has both actors moving about 2 pixels back and forth every frame while the camera and the wall stay still; the collision watch saw nothing touching them.
- **`bTranslationLock`** (4Stim.ini, 1): 0 holds scene actors without the never-ending translation (placed once, put back if they drift), to tell whether the lock is what's fighting.
- **The player is AI driven in scenes, as OStim's lock** (`SetAIDriven`; Papyrus `Game.SetPlayerAIDriven`), `bPlayerAIDriven=1`. The owner's player vibrated in every scene, start pose included, while the shake watch showed the reference and its root still and nothing touching him: 4Stim only blocked the player's input, so the player's own movement could pull against the lock between frames. Needs testing.
- **The cause: the wall.** With `bTranslationLock=0` (owner's log, 6 November) both actors sat 10-11 units off the wall spot, away from the wall, and the collision watch showed each touching a body no reference owns (a precombined wall). Wall scenes' spot is 15 units out, inside the actors' collision capsules, so the wall pushed them out and the lock pulled them back every frame. Now a scene actor touching such a wall collides with nothing for the rest of the scene (their gravity is already 0 and the lock holds them), and gets their collision back at the end: **`bPassThroughWalls=1`** (4Stim.ini). Needs the plugin rebuilt and testing with `bTranslationLock=1`.

### Add-on settings, as OStim's (parity audit Tier 3)
- **OStim's add-on settings folder**: add-ons' JSON files in `Data\F4SE\Plugins\4Stim\Settings\` (OStim's format) show on an **Add-Ons** MCM page, which 4Stim writes into its MCM config when the game starts (docs/MCM.md).

Needs the plugin rebuilt.

### Per-actor and per-sex choices, as OStim's (parity audit Tier 3)
- **Equip objects (strap-ons, tongues) are chosen in OStim's order**: the actor, their base, race, sex, then the default file. OStim's `GetEquipObjectPairs`, `GetEquipObjectName` and `SetEquipObjectID` (as `SetFormEquipObjectID`) are on `FourStim`, with OStim's 0x0 / 0x1 for men and women (docs/ACTORS.md).
- **OStim's MCM choices as 4Stim.ini settings**: `sDefaultStrapOn`, `sPlayerStrapOn`, `sDefaultTongueMale`, `sDefaultTongueFemale`, `sPlayerTongue`, `sPlayerVoice`; and `sPlayerLight` (also on the MCM's General page).

Needs the plugin rebuilt and `FourStim.psc` compiled.

### Starting scenes from scripts, and sound descriptors, as OStim's (parity audit Tier 3)
- **A script's scene with you goes through OStim's start flow**: it waits until you're out of dialogue, then asks which furniture to use (or "use the nearby ...?" with a starting scene), whether to add actors near you, and your position, as OStim's settings say, and sorts with `PlayerAlways...`. The questions come up in the picker (docs/API.md).
- **New setting `addActorsAtStart`** (OStim's, on by default, MCM General page).
- **`FourThreadBuilder.Create` returns -1 for an ineligible actor**, as OStim's: disabled, deleted, a child, dead, no actor type, or already in a scene.
- **OStim-style sound descriptors play natively**, as OStim's: with `moanVolume` / `soundVolume`, stopped by muting, climax and the scene's end, and timed by their real length (docs/SOUND.md). Before, through Papyrus `Sound.Play`, counted as 2 seconds.

Needs the plugin rebuilt; no Papyrus or SWF changes. The MCM's config.json, settings.ini and 4Stim_en.txt are new.

### Furniture, as OStim's (parity audit Tier 3)
- **One picker row per furniture list type**, the nearest piece: single and double beds are one "Bed" row, the bench keeps its own (OStim's `listIndividually`; docs/FURNITURE.md). Before, each type had a row, and a `listIndividually` type listed every piece.
- **Furniture someone is using is left out of the picker**, as OStim's: a chair an NPC sits in, a bed someone sleeps in.
- **OStim's furniture type conditions all work and must all hold**: `anykeyword`, `keywordblacklist`, `formlist`, `formblacklist`, `markercount` (sit / sleep markers), `cellname`. Before, only the keyword ones, and two `anykeyword` lists were ORed.
- **OStim-layout furniture types' `rotation` is radians**, as OStim's (a `3.14159` turned scenes by 3 degrees instead of 180).
- **The player goes back to where they stood after a furniture scene**, with `SetResetPosition`, as OStim's (docs/ALIGNMENT.md).

### Fixed
- **The bounce at transitions in converted packs** (the Lovemaking Compendium's table and bed transitions): their Idles loop (`dyn_ActivationLoop`, all 708 in its plugin), which makes the game blend back from the clip's start. 4Stim now sets every transition's Idles to play once (`dyn_Activation`) when it loads the scenes, for any pack, as the test pack's plugin already had by hand (docs/SCENES.md).

### Climax and stimulation, as OStim's (parity audit Tier 3)
- **A climax waits for the moan the actor is making to end**, as OStim's, and so do event reactions: no more moans cut off mid-sound.
- **Climax comments**: voice sets' `climaxCommentSelf` and `climaxCommentOther` (their sound part) play after a climax (docs/SOUND.md).
- **Scene end rules as OStim's**: `endOnPlayerOrgasm`, `SetEndOnOrgasm`, `SetEndOnSubOrgasm` and `SetEndOnBothOrgasm` count in NPC scenes too, with `endNPCSceneOnOrgasm` on top; each counting climax restarts the end delay.
- **Auto mode goes to its main stage after any climax**, foreplay included.
- **The climax blur is only for the player's own climax**, as OStim's.
- **A climax takes 250 Action Points** (OStim's 250 Stamina).
- **Per-actor stimulation**: OStim's OData functions on `FourStim` (`Get/Set/ResetActionStimulation`, `...MaxStimulation`, the same for events, `GetActions`), saved with the game (docs/EXCITEMENT.md).

Needs the plugin rebuilt and `FourStim.psc` compiled.

### Scenes and transitions, as OStim's (parity audit Tier 3)
- Transitions keep using the scene they go to's per-role values and alignment (tried OStim's own-values rule; it brought back the bounce at both ends of each transition).
- **OStim's nine missing annotations**: `4StimUndress`, `4StimRedress`, `4StimUndressPartial.<mask>`, `4StimRedressPartial.<mask>`, `4StimRemoveWeapons`, `4StimAddWeapons`, `4StimSetLooking.<left>[,<up>]`, `4StimUnsetLooking`, `4StimResetLooking` (docs/EVENTS.md). The partial masks take Fallout 4's slots.
- **Sequences**: no navigation while one plays, as OStim's scene menu; an interrupted one ends a scene started with `EndAfterSequence`; every scene's roles count, and the actor types must match (docs/SCENES.md).
- **`fadeOnEntry` fades when you pick the scene too**, not only in auto mode.
- **Navigation links as OStim builds them**: a second link to the same scene is left out, links between incompatible furniture are dropped (logged), the default label is the name of the scene the link ends at, a link with no icon gets OStim's icon for that scene, and a link is offered only when the actors suit every scene on its way.
- **`FourThread.NavigateTo` goes along the navigation graph** (within `navigationDistanceMax`), as OStim's navigateTo; `WarpTo` still jumps.
- **Actor conditions are read again at every scene change**, so a penis put on or taken off mid-scene counts for the navigation from the next scene on.

Needs the plugin rebuilt. No Papyrus or SWF changes.

### The MCM and translations, as OStim's
- **The MCM has OStim's pages, in OStim's order, with its section headers, labels and help texts** (docs/MCM.md): General, Controls, Auto Control, Camera, Excitement, Gender Roles, Furniture, Undressing, Expressions, Sound, Alignment, Toys, Debug, About. 4Stim's own settings sit on the page they belong to (the HUD and partner finding on General, the controller on Controls, climax effects on Excitement, eyes on Expressions), and the undressing slots and Save and Load are on OStim's pages for them. Where a setting works differently in 4Stim, its help says what 4Stim does. Nothing you've set in the MCM changes.
- **Translation keys follow OStim's**: OStim's own keys with "4stim" for "ostim" (`$4stim_reset_position`, `$4stim_tooltip_fades`), so OStim's translations carry over, and 4Stim's own texts in the same style (`$4stim_tab_search`, was `$4Stim_TabSearch`). A translation made for the old keys needs them renamed (docs/TRANSLATIONS.md).

Needs the plugin and both SWFs rebuilt (the keys are in them). No Papyrus changes.

### The HUD, closer to OStim's (parity audit, 5 §1, §4, §9, §17, §21)
- **The speed meter shows OStim's speed number** (the scene's `displaySpeed`, "1.5"), else speed / speeds as before; **the scene's name sits under the list's header**, its pack at the header's end.
- **The HUD comes back on every scene change**, as OStim's menu does (before, only when a scene started).
- **The excitement bars stay up when the HUD fades**, as OStim's, and **OStim's four bar settings** (docs/SETTINGS.md, MCM HUD page): `enablePlayerBar`, `enableNpcBar`, `SetAutoHideBar` (the bars fade with the HUD) and `SetMatchColorToGender`. **By default the bars are colored by role as OStim's** (the first blue, the second pink, the rest purple); turn `SetMatchColorToGender` on for the old colors by sex.
- **The Align tab works as OStim's align menu** (docs/ALIGNMENT.md): its steps (10, 1, 0.1, 0.01, 0.001, for the size too), its order (size before turn), its readout (role, sex, the scene's id and pack, the alignment key), Toggle for the next actor, and the pull-out key for the next step while the tab shows. The controller's controls are as before (B, the d-pad, the Step row).
- **Search takes typing** (the search key, or Search > Browse all scenes): type part of a scene's name to filter, Backspace deletes, the arrows, Enter, the controller and the mouse pick, Esc closes. As OStim's, only scenes for these actors and this furniture, no transitions. The game is paused while it's open.
- HUD API (docs/HUD_API.md): optional `SetSpeedLabel`, `SetBarOptions` and the movie's `TabChanged`; `SetScene` gains `modpack` and `SetAlign` the readout. The picker movie gains `SetFooter`.

Needs the plugin and both SWFs rebuilt (`Interface-src\build.bat`). No Papyrus changes.

### Aimed animations
- **The `4StimPenisAim` annotation** (docs/EVENTS.md, docs/ALIGNMENT.md): AnimBridge now bends the whole penis chain toward the partner when it converts a pack and marks those animations with it. On them the scene file's penis bend is left out (it was written for SoS's straight schlong, and bent the aimed shaft off target); the Align tab's bend still counts.

Needs the plugin rebuilt; FourModEvents.psc, FourJSON.psc, FourStimMenu.psc, FourStim.psc, FourStimOptions.psc, FourStimPhysics.psc, FourStimScene.psc and FourStimFactions.psc compiled (and FourStimTest.psc, if you use it); the new test pack (its scene files changed); and `4Stim.esl` enabled.

### Scenes ending, failing and loading (parity audit, tier 1 items 1 to 3)
- **Scenes end when someone in them dies or is in combat, and NPC scenes end when the player isn't in their cell**, as OStim's (docs/ACTORS.md, "When a scene ends by itself"). Before, a scene went on around a corpse or through a fight, and NPC scenes left behind stayed until their timer ran out.
- **Fixed:** loading a save while in a scene left the player's controls locked (no menus, no activate), the camera slowed and zoomed and the HUD hidden: they were only put back when the game first started. They're now put back on every load.
- **Fixed:** a scene that couldn't start (no such scene, or the game refused every animation) left its actors restrained, without collision and unable to talk, and the player locked in the scene camera with nothing running to end. They're now let go again, as at a scene's end.

- **Scenes end when someone in them is hurt** (`bEndOnDamage`, 4Stim's, on by default; MCM General page): actors in a scene don't start fights, so OStim's combat check let a scene go on while the owner shot one of its actors. Shooting near them doesn't count.
- **A scene broken off by a death, a fight or harm leaves its actors where the scene was** (`bStayWhenInterrupted`, 4Stim's, on by default): OStim's "Reset position" sent a surviving raider back to the wall she stood at before the scene, to attack from there.
- **MCM:** the Erections toggle now comes from `tools/make-mcm.py` like the rest (it had been added to the page by hand, and regenerating the page dropped it).

### OStim content as written (parity audit, tier 1 items 4 to 7)
- **The base scenes are OStim's own scene files, under OStim's ids** (`OStim2PStandingApartMF`, not `4StimTest_OStim2PStandingApartMF`): OStim add-on packs hub onto the base scenes by id, so with the old ids their rows into the standing, kneeling and bed scenes went nowhere, silently for packs with `noWarnings`. The test pack now ships OStim NG's scene files unmodified (the 246 it has animations for) in `Scenes\4Stim Test Anims\`, and `4StimTestAnims.json` holds only 4Stim's additions: the redress scenes, the undressing scenes' `undress` steps and the two test sequences. Alignments saved for the old ids are copied to the new ones on load, and links that still name an old `4StimTest_` id (packs converted against the old test pack: hubs, transitions back to a base scene) find the new one; without that, those transitions played with no way out but ending the scene (the owner's Lovemaking Compendium bed scenes). OStim's own `OStim/...` icon paths are shown from 4Stim's icons (they were blank in the navigation list). A navigation to a missing `OStim...` scene is now logged even with `noWarnings`.
- **OStim's `singlebed` and `doublebed` furniture types** (docs/FURNITURE.md): subtypes of `bed` (one sleep marker, or two or more), so OStim's 40 single and double bed scenes play as written. A scene naming a furniture type nobody defines plays as a floor scene with a warning, as OStim's does; before, it was never offered.
- **An action of a type no Actions file defines is kept with the `default` action's attributes**, as OStim's (docs/SCENES.md): before, it was dropped, which moved every later action down one, so `4StimPeak.<n>` annotations, `expressionAction` and the action API pointed at the wrong action. An action without `actor` now applies to no one, and `HasAction` / `FindSceneAction` find types by id or alias even when no file defines them.
- **Facial expressions named by an action alias work** (docs/EXPRESSIONS.md): OStim's own expression files say `cunnilingus`, `lickingpenis`, `ticklingfeet` and six more aliases where 4Stim looked for the ids, so oral, licking and tickling scenes got the generic face.

### 4Stim.esl: OStim's API factions, form lists and keywords (parity audit, tier 2 item 15)
- **A new light plugin, `4Stim.esl`** (enable it; no load order slot): 4Stim's counterparts of the 642 factions, 231 form lists and 2 keywords in OStim's `OStim.esp`, with OStim's form IDs and "4Stim" for "OStim" in their editor IDs (docs/API.md, "Factions, form lists and keywords"). Add-ons' dialogue and voice-set conditions read these.
- **4Stim fills them in during scenes, as OStim does:** the excitement, times climaxed, actor count and has-a-penis factions; each action's factions for what an actor is doing now, and the furniture's; the stat factions that count, for good, the scenes an actor did something in, the climaxes they had at it and with whom; and the form lists of who's been in a scene and who did what with the player.
- **OStim's action tags** (`Action Tags\`, docs/ACTIONS.md): a tag's forms go to every action with it, and `-tag` swaps actor and target, as OStim's. The action data's forms and the tags are OStim's, copied by `tools/sync-action-forms.py`; `tools/make-api-plugin.py` writes the plugin from OStim's.
- **Furniture with OStim's `NoStim` keyword is never used for scenes.**
- Needs `FourStimFactions.psc` compiled. Without the plugin, 4Stim logs it once and goes on without the factions.

### Threads as OStim's (parity audit, tier 2 items 11 to 13 and 16)
- **A thread exists as soon as `Start` returns it** (docs/API.md, "Threads"): `IsRunning`, metadata, `GetActors` and the climax stall work on the next line, and are read and written at once (`AddMetadata` then `HasMetadata` was false). Speed, navigation, stop and auto mode asked for before the actors are placed are done once the scene plays; before, they were dropped without a word.
- **Several threads can start at once**: each start keeps its own options, metadata, furniture and sequence. Before, a second start before the first was placed took the first's, and the first scene got another thread ID than `Start` returned. A start that fails later ends its thread (`IsRunning` false, the reason in the log).
- **The end event comes after the actors are free** (unrestrained, interaction back), as OStim's: a scene a mod starts on the end event isn't undone by the old scene's release any more. The thread's metadata is readable inside the event.
- **Starting scenes as OStim's**: the actors play the roles in the order given, a starting scene that doesn't fit falls back to a picked start scene, unknown scene IDs are left out, entries without a duration play for their scene's length (they flashed by), `navigateTo` routes there through the navigations, one starting scene is a plain start, and the nearest furniture the scene is made for is used. Duplicate and unloaded actors are refused.

### Crash
- **No more full physics rebuilds after a scene by default** (`bResetPhysicsAfterScene=0`, docs/PHYSICS.md): the owner's crash of 4 November, like two on 23 October, was inside FSMP's rebuild (`TryRebuildPendingActorsLocked` freeing a collider tree twice), a second after the NPC's after-scene `ResetPhysics`. A physics swap is still put back after the scene, which rebuilds that body. Set it to 1 for the old behaviour. The crash itself is FSMP's (FO4 Faster HDT-SMP 1.3.1.0); this only stops 4Stim from asking for the extra rebuilds.

### Build
- **CommonLibF4 is now Dear Modding's fork** (github.com/Dear-Modding-FO4/commonlibf4, pinned at c71da99): libxse's CommonLibF4 with 437 more commits of engine mapping and AE fixes, among them the face animation data 4Stim's expressions can write to directly. It brings the DearModdingUI API as headers only: 4Stim doesn't need Dear Modding UI installed. Update the submodule (`git submodule sync --recursive` then `git submodule update --init --recursive`) and rebuild.
- **Fixed: no scene would start on the Dear Modding CommonLib.** The fork names each Papyrus native's parameter types by the class's script type, and its `RE::Actor` had none, so it took ObjectReference's: every native with an Actor parameter (PlaySceneIdles, IsInScene, PlaceGroup, ...) was registered as taking an ObjectReference, and the game refused to bind them ("does not match existing signature" in Papyrus.0.log), leaving the player stuck with controls locked. The build now gives Actor its own (`xmake.lua` adds it to the submodule's `Actor.h` if it's missing).
- **Fixed: every scene animation was refused on the Dear Modding CommonLib.** Its address IDs for `AIProcess::SetupSpecialIdle` and `StopCurrentIdle` are one too high for Next-Gen / Anniversary (2231705 / 2231706 instead of 2231704 / 2231705), so playing an idle called the function that stops one ("PlayIdle returned false / false"). `xmake.lua` puts the right IDs into the submodule's `IDs.h` before building.
- **Fixed: NPC scenes outdoors ended within seconds** ("the player isn't in its cell"). OStim ends an NPC scene when the player isn't in its cell; Fallout 4's exterior cells are 4096 units, so the player standing beside a scene in a settlement was often one cell over. Outdoors a scene now ends only when the player leaves its worldspace or its cell unloads; indoors it's still OStim's same-cell rule.
- **Fixed: actors could keep a scene's animation after it ended** (seen on a ZaZ pillory scene: the NPC sent back still posed, the player held in place). The end's IdleStop came in the same moment as a transition playing the next scene, and the game kept the scene's idle. Now nothing plays on a scene once its end has begun (from the end key, picker or HUD, before the fade), and when an end comes right after a scene's idle, IdleStop is sent again shortly after.
- **FourLibrary has all 97 of OLibrary's functions** (was 37): the multi-actor tag searches (ForAll, Any / All per actor, with scene tags), the actions done by or to a role, `GetRandomSceneSuperloadCSV` and `GetScenesInRange`. `GetRandomSceneWithMultiActorTagForAny` (and its CSV and furniture versions) now mean what OStim's do, tag *i* for role *i*, instead of any role having any of the tags.
- **Fixed: a crash calling a native with an empty (None) array**, e.g. `FourLibrary.GetRandomScene`, whose wrappers pass unset arrays for the filters they don't use. CommonLibF4 read a None array through a null pointer; the build now patches it to read as an empty array.
- **FourMetadata has all 282 of OMetadata's functions** (was 22): action searches by actor, target, performer, mate and participant, the action superloads, action tags, roles' requirements and tag overlaps, `ScenesToNames`, `GetAnimationId`, and OStim's custom action data (`ints`, `floats`, `strings` and their lists on an action's sides, now read from action files). `HasActionTag` now takes an action index, as OStim's (`HasActionTagOnAny` is the old meaning). `cgf "FourStimTest.Metadata"` checks them in game.
- **OStim's second C++ API** (its `OstimNG-API`, for external scene UIs and companion plugins): `include/4Stim/FourStimNG-API.h`, a Scene interface (start couple / threesome / foursome / any scenes, stop, auto mode) and a Thread interface (thread data, navigation, speed, alignment, search, options menu, actor checks, thread migration, keys), with OStim's events and control callbacks and its external-UI switch (4Stim's HUD stays closed while another UI drives the scene). The interface map is also exported as `requestInterfaceMap`. `cgf "FourStimTest.PluginAPI"` checks it in game.
- **Speed as OStim's:** scenes start at their `defaultSpeed`, and a scene moved to (navigation, a transition's destination, an auto transition) keeps the pace: the speed's place among the old scene's speeds is put on the new one's (OStim's ChangeNode). Was: the slowest speed at the start, and the same speed number after a move.
- **Speed control in every NPC scene** (and the player's with `SetActorSpeedControl`), auto mode or not, as OStim's; before, it only ran in auto mode.
- **Auto mode at the start as OStim's:** OStim's `SetForceAIForMasturbation` and `SetForceAIInConsensualScenes` (ini and MCM) start the player's solo / partnered scenes in auto mode; `NoAutoMode` threads never run it, whoever asks; `NoPlayerControl` no longer turns it on.
- **HUD: group scenes' excitement bars in columns.** With more than three actors the bars are half as wide, in two columns at most: three rows up to six actors, then four tighter rows (smaller names, thinner bars) for seven or eight; the speed meter moves over. Up to three actors they're as before. Needs the HUD SWF rebuilt (`Interface-src\build.bat`).
- **1.10.163:** the fork gives old-gen Address Library IDs for functions whose ID changed with next-gen, where libxse's gave next-gen ones only. 4Stim now reads 1.10.163's own Address Library (`version-1-10-163-0.bin` beside the plugin) for those first, then the Runtime Database and its own table as before (`Addresses: old-gen IDs ...` in `4Stim.log`). Without that Address Library installed, such functions may be found wrongly on 1.10.163.

### Physics
- **4Stim's changes to the physics world now hold its lock** (raised to the owner, 5 November: Havok steps the world on its own threads, so touching its memory mid-step races it). Everything 4Stim does to Havok (furniture and touched things out of the actors' way and back, the actors' shared collision group and their own filters back, controller gravity, reading what a controller touches) runs in the F4SE task queue, which can overlap a step; it now takes the world's lock (`hknpBSWorld::m_worldLock`, as the game and another F4SE plugin do) for writing first, so it waits for a step to finish. If the lock isn't free within 20 ms it goes ahead as before and logs `Physics lock:` (at most 3 times). Nearby references are looked up before the lock, not inside it.

### Animation annotations
- **Fixed (needs testing): no slap on spanks, and every other annotation cue never arriving** (the owner's report, 5 November). Fallout 4 doesn't send an animation's text annotations as animation events for the idles scenes play: across five spanks the log had only the behavior graph's own events, never `4StimEvent.spankright,1`. 4Stim now reads the annotations from each idle's `.hkx` when it plays and fires them itself at their times, on every loop (docs/EVENTS.md). That covers `4StimEvent` (spank and wash sounds, shakes, reactions), `4StimClimax`, `4StimPeak`, `4StimPlayExpression`, `4StimBendSoS` / `4StimResetSoS`, `4StimPenisAim` and `4StimEquipObject`, none of which can have worked in game before.
- **Fixed: the game freezing on a scene change with the first build of this** (the owner's "Turn around" from a standing hold). It read the animation file on the main thread through the game's own file system right after asking the game to play that file. Files are now read on a worker thread, loose files with plain file reads (the game's file system only for a file packed in a .ba2), and a scene's cues start once its file is in. `bTimeAnimCues=0` turns the timing off.
- **Fixed (needs testing): a stutter on the scene after one that took off gloves** (the owner's "Turn around" from a standing hold). Two seconds after a strip, 4Stim redraws the player if the items are off but nothing drawn changed (a 1.10.163 tester's clothes stayed on screen); gloves change nothing drawn, so it redrew the player for nothing, mid-scene. It now does that only when clothes on the body (slot 33) came off.

### API coverage (parity audit, tier 2 item 17, first part)
- **FourActor** now has all of OStim's OActor: `SetExpressionsEnabled`, `HasExpressionOverride`, `UndressPartial` / `RedressPartial` (by slot mask), `IsObjectEquipped`, `SetObjectVariant` / `UnsetObjectVariant`, and the deprecated `UpdateExpression` and `GetSceneID`. `PlayExpression` returns the expression's duration (a float, -1 if none) as OStim's, so renamed OStim calls compile; it was a bool. `VerifyActors` also turns down children, disabled and deleted actors (OStim's `isEligible`).
- **FourFurniture** now has all of OFurniture: `GetOffset` (where a scene goes on the piece, in its own frame, as `[x, y, z, rotation, scale]`), `GetSceneID`, and `ResetClutter`, which does nothing, as in OStim (its body is commented out there).
- **New FourActorUtil** (OStim's OActorUtil, all 14): `HasSchlong`, `FulfillsCondition` / `Any` / `All` (a perk's conditions), `SayTo` / `SayAs`, `EmptyArray` / `CreateArray` / `ToArray`, `Sort` (OStim's actor order: dominant actors, then those with a penis, then the player's place), `SelectIndexAndSort`, `GetActorsInRange` / `GetActorsInRangeV2`, `ActorsToNames`. Fallout 4 departures: `SayAs` speaks in the actor's own voice (Fallout 4's Papyrus can't say a topic in another voice type), and `SelectIndexAndSort` doesn't ask for the player's role (4Stim asks in its picker) but sorts as `Sort`.
- **New FourSequence** (OStim's OSequence, all 12): random sequences for actors, by furniture type and sequence tags, as OStim's.
- Recompile `FourActor.psc`, `FourFurniture.psc`, `FourActorUtil.psc` and `FourSequence.psc`.

### Climax stalls (parity audit, tier 2 item 19)
- **As OStim's:** an actor's own stall (`FourActor.StallClimax`) and their thread's (`FourThread.StallClimax`) are kept apart; `FourThread.PermitClimax(id, true)` also lifts the actors', and `FourThread.IsClimaxStalled` reads the thread's only. Reaching 100 while stalled queues the climax, which goes off once nothing stalls it (it used to be dropped). `FourActor.Climax(act, true)` climaxes now and leaves the stall in place (it used to lift it for good); `Climax(act)` while stalled waits instead of doing nothing. `FourActor.IsClimaxStalled` takes OStim's `CheckThread` (default true). `FourStim.Climax` and `FourStim.IsClimaxStalled` take the new arguments: recompile `FourStim.psc` and `FourActor.psc`.

### Chair shaking
- **Fixed (needs testing): a T-posed player with a bent head, then an FSMP crash, after a scene that began in first person** (the owner's table scene, 5 November). The camera goes back to first person half a second before the scene lets its actors go, and in that half second the scene guard saw the player's third-person body culled (as first person leaves it) and showed it again. Shown in first person, that body isn't animated (the T-pose), and FSMP crashed in its collider (`hdtCollider.cpp:209`) when the physics swap went back on it. The guard leaves the body alone in first person now.
- **Fixed (needs testing): the player shaking in a chair by a wall again** (the owner's video of 4 November, `OStimChair2PStandingSittingMF` on a chair against a wall). The chair was out of the way, but what else the actors touched (the wall behind the chair) only was while the collision watch ran, and the watch had been off since it crashed the game on 1 November. It now runs again, on by default with `bFurnitureOutOfTheWay`, without the call that crashed (`bhkNPCollisionObject::Getbhk`, whose Havok read check faulted alongside the physics step): touched bodies are matched against the collision objects of the references around the actor instead. A precombined wall has no reference of its own and is logged and left (`touches a body no reference near them owns`). `bCollisionWatch` now only adds the position lines.
- **Fixed (needs testing): the cause of the chair shaking.** A scene with the player is built on the player's own spot, which overrode the furniture spot he had just been placed on. By then the game had pushed him up out of the chair's collision (9 units in the owner's log), so both actors were held that far in the air over the chair for the whole scene and the player, not restrained, bounced against the lock. After a placement the scene now uses the furniture spot.
- **No gravity for scene actors while the lock holds them (needs testing):** the owner's position log ruled out collision: both actors stayed exactly on their spot, the player held about 5 units over the floor and standing on nothing. The player isn't restrained (that would freeze the free camera's look), so his character controller kept falling under gravity between the lock putting him back, his whole body bouncing about six times a second, only in chair scenes (whose spots hold actors off the floor) and in either role. Scene actors' controllers now have no gravity during the scene and get it back with their collision (`controller gravity` in `4Stim.log`).
- **Chair spots raised onto the floor (needs testing):** in the second video the shake was the player bouncing a few pixels up and down about six times a second, touching nothing but what he stood on. A chair without furniture markers puts the scene at the chair's own origin, which can sit a little below the floor; the lock then holds the actor below the floor while the floor pushes their character controller back up. Such a spot is now raised onto the floor under it, found by a ray straight down past the chair (`spot is ... below the floor under it` in `4Stim.log`). `bCollisionWatch=1` logs the controller against the spot five times a second.
- **Fixed:** the scene restarting three times in its first 11 seconds when an NPC's process kept pointing at furniture she had been taken out of (Ana and a garden idle marker in the same log). The same furniture again after it is taken as a stale handle and left alone (`a stale handle, left be`).

### Voices (parity audit, tier 1 items 8 to 10)
- **Choose an actor's voice set**, as OStim's "Player voice" / "Actor voice" (docs/SOUND.md): each actor's page in the HUD's options has a **Voice** page listing the voice sets, and a **Mute** set (`mute.json`) for silence. Saved with the game per actor base. OStim's are in its MCM, which in Fallout 4 can't list what's installed. Papyrus: `FourStim.GetVoiceSetPairs` / `GetVoiceSetName` / `SetVoiceSet` (OStim's `OData` ones), `SetActorVoiceSet` / `GetActorVoiceSetName`.
- **Who gets which voice set follows OStim's order**: the choice, the NPC, the voice type (else the race's default voice for the actor's sex), the race, and the default for their sex only for humans: creatures without a set of their own are silent instead of moaning like people.
- **The first voice set entry that fits plays**, as OStim's, instead of one picked at random: packs list specific sounds first and a catch-all last, so the catch-all had played about half the time during a blowjob. OStim's perk `condition` on an entry now works (actor as subject, their primary partner as target); before, it was ignored and conditional sounds always played.
- **Fixed:** muting an actor mid-moan, or an event reaction with nothing to play, left the moan face on for the rest of the scene.

### Docs
- **The doc lines the parity audit found describing 4Stim as OStim are corrected** (docs/PARITY_AUDIT.md, "Docs that say the wrong thing"): auto mode's speed control, standing rule and pull-out, `fadeOnEntry`, `listIndividually`, the player not being put back after a furniture scene, weapons at scene start, the sex toy inputs, `FindRandomScene`, the alignment API, and OStim's key defaults and gamepad / mouse keys. Equip-object choices and excitement were said not to be saved; they are. The ini-only settings missing from SETTINGS.md and MCM.md are listed.

### OStim NG's 22 October update
- **`gropingchest`** (aliases `gropechest`, `holdchest`, `holdingchest`): OStim's new action for groping a man's chest (`gropingbreast` is for women's). `gropingbreast`, `gropingbutt`, `gropingtesticles` and `gropingchest` carry OStim's new `groping` tag. OStim's `groping` action tag file only sets factions, which need a plugin file (parity audit tier 2, item 15). The update's other change, a new channel in OStim's C++ plugin interface (`requestInterfaceMap`), belongs to OStim's second C++ API, which 4Stim doesn't have yet (item 18).

### Actor 3D rebuilds
- **A scene actor whose 3D the game rebuilds gets the scene's physics swap back** (docs/PHYSICS.md): equipping something, the undress redraw or another mod's redraw took A-Body back to its own physics. SKSE has an event for this and F4SE doesn't, so 4Stim watches its scene actors itself. This replaces 0.4.2's re-swap after the undress redraw only.

### Loading
- **The first picker of a game no longer pauses** while 4Stim reads the scene files (the owner's install: 935 scenes from 635 files, 2774 scene texts, every plugin's Idle editor IDs). They're read on a thread of their own when the game reaches the main menu, with the actor properties, expressions and lights (`Preload: ... read in ... ms` in `4Stim.log`).

### Mod events
- **Mod events, as OStim's** (docs/API.md, "Mod events"): OStim sends its events as SKSE mod events, which Fallout 4 doesn't have, so 4Stim's plugin now keeps mod event registrations itself. A script registers by name (`FourModEvents.RegisterForModEvent(self, "4stim_thread_start", "OnThreadStart")`) and gets OStim's four arguments, with the thread ID. All of OStim's events are sent, renamed "4stim_": the thread events (start, scene changed, speed changed, end with OStim's JSON, actor orgasm, furniture changed, event) and the player thread's older ones (start, scenechanged, scenechanged_<id>, animationchanged, orgasm, end, totalend, spank), in OStim's order. `FourJSON` reads the end events' JSON, as OStim's `OJSON`. Add-ons can send their own with `FourModEvents.SendModEvent`. For 4Stim and its add-ons for now. 4Stim's own `FourStim_On...` scene events still work.
- **Every speed change now sends the speed event**, whoever made it (auto mode, a climax, `FourThread.SetSpeed`); before, some didn't.

## 0.4.2 alpha (1 November 2026)

Everything since the 0.4.1 package: beds, A-Body's physics swap, the penis bend, OStim's scene text, the flat navigation list and YAML scene files. Needs the plugin rebuilt (`xmake build -r`; the first build fetches yaml-cpp). No script or HUD changes since 0.4.1. Take the new `4Stim.ini`: the penis bend settings changed.

### Beds and physics
- **Fixed (needs testing):** lying scenes on a bed put heads at the foot (the owner's vault bed: heads through the foot rail, pillows at the other end). With no furniture markers to go by, a scene lying lengthwise on a bed faced the end farther from the player; it now faces the end with the bed model's pillows, and only falls back to the far end when the model has none (`Furniture: ... pillow(s)` in `4Stim.log`, or the model's parts when it has none). Furniture markers are also looked for deeper in a model.
- **Fixed (needs testing):** the player sinking through a bed in lying scenes, drawn inside it with the head at the rail, while the NPC lay on top: the bed's collision was off for the scene's actors (the chair-shaking fix of 0.4.0). Beds now stay solid (`Furniture collision: ... is a bed` in `4Stim.log`); chairs and other furniture are as before. Confirmed by the owner with the bed left solid by hand.
- **Fixed (needs testing):** A-Body's physics swap never happening for an actor dressed when the scene started (the owner's player in the Silver Shroud outfit): 0.4.0 looked once, at the start, for the physics file the body's mesh names, and the naked body isn't loaded under clothes. An actor with no physics file yet is now looked at again every second, and swapped once the clothes come off (`Physics: ... a physics file now (undressed): swapping`). The owner was in fact naked (helmet, scarf and gloves only) and his body still named no physics file to 4Stim, so with FSMP installed (its `DynamicHDT` script) a swap that names its own files, as A-Body's does, is now tried anyway and FSMP answers whether the body uses it; without FSMP nothing is called, as in 0.4.0.
- **Lying scenes on beds sit 30 units toward the feet and 10 up** (the bed type's new `lyingShift` and `lyingRaise`, docs/FURNITURE.md): from the middle of the owner's vault bed, heads went into the head rail instead of onto the pillows. The pillows are found by their shapes' middles (a shape's own origin is the bed's, which made the first tries guess the end at random).
- **Fixed (needs testing):** A-Body's rigid swap going back to flaccid a moment after it took (the owner's cot scene): the undress check's redraw of the player rebuilds his 3D, and FSMP builds the physics again from the mesh's own `MaleBody.xml`. After a redraw, the scene's swap is now put in again 1.5 s later (`Physics: ...'s 3D was rebuilt`).
- **Diagnostics:** `bFurnitureOutOfTheWay` (default 1) turns off the furniture collision change of 0.4.0 for all furniture.

### Penis bend
- **The penis bend curves the shaft, as TNG's does** (docs/ALIGNMENT.md): OStim's `SOSBend` steps are answered in Skyrim by TNG's bend poses, which turn four segments (read from its clips: up 43/9/9/4 degrees along `Genitals01`-`04`, down 53/3/0/3.5/4 along `01`-`05`). 4Stim turned one hinge at the base; it now turns ZeX's `Penis_01_Offset` to `Penis_05_Offset`, each TNG's share, and a step is 7.2 degrees in all (TNG's full bend, about 65 degrees, at 9). New `sPenisBendUp` / `sPenisBendDown`; `sPenisBendNode` takes a list. Step n is n/9 of the pose, as TNG's and SoS's behavior graphs blend them (checked in both). At -10, a body that can't go flaccid (A-Body) takes TNG's flaccid pose (`sPenisFlaccid`, 50 and 15 degrees down) instead of the -9 bend.

### Scene text
- **Fixed:** OStim's translated scene text showed as its keys: navigation labels as `$OSTIM_NAV_LIE_ON_TSAMANTHA` instead of "Lie on Samantha", and OStim's base scenes named `$OStimChair1PSittingF` instead of "Sitting [F]", which also sorted them together at the top of the picker. Scene, navigation, furniture, voice set, light and equip object names starting with `$` are now translated as OStim's `LocaleManager` does it, from every `Interface\Translations\*_<language>.txt` (Fallout 4's `_en` or Skyrim's `_ENGLISH`), with the actor name put in after the translation (docs/TRANSLATIONS.md). 4Stim comes with OStim's `ONav_ENGLISH.txt` and `OScenes_ENGLISH.txt`, which packs made for OStim (the Lovemaking Compendium) use without shipping.
- **The picker no longer lists transitions** (its scene list when starting and "Change scene"), as OStim's search menu: a transition plays on into its destination, it isn't a scene to pick.

### YAML scene files
- **Scene and sequence files can be YAML** (`.yaml` / `.yml`), with the same fields as the JSON ones, in either layout, side by side in the same folders (docs/SCENES.md, "YAML"). 4Stim's: OStim reads JSON only. Builds now fetch yaml-cpp (xmake does it on the first `xmake build`).

### Navigation
- **The HUD lists a scene's navigations in one list again, as OStim does** (`SceneMenu::BuildMenuData`). 0.4.0's navigation groups are gone: a scene's own `group` and the automatic group per pack (`modpack`). OStim's packs keep the list short with hub scenes, and 4Stim's packs do the same (docs/SCENES.md, "Hubs").
- **`tools/make-hubs.py`** gives a converted pack hub scenes: one per shared scene its scenes hook into, with the pack's rows moved into it. The Atomic Lust test pack has them.

### Docs
- **OStim parity audit** (docs/PARITY_AUDIT.md, docs/audit/): what 4Stim is missing or does differently from OStim NG, ranked. Nothing in it is fixed in this build.

## 0.4.1 alpha (31 October 2026)

Needs the plugin rebuilt and FourStimErection.psc compiled (with LooksMenu's BodyGen.psc in the compiler's sources).

### New
- **Erections** (docs/ERECTION.md): men are erect during their scenes and flaccid after, the way their body does it, where OStim leaves it to Schlongs of Skyrim. BodyTalk and Atomic Muscle's `.tri` install get the body's `Erection` morph through LooksMenu (and `Erection Up` / `Down` follow the penis bend on skeletons without ZeX's bend node); A-Body needs nothing (its penis is rigid in the mesh, and the scene's physics swap already turns off what makes it hang), only the bend; Atomic Muscle's bodysuit install gets its erect bodysuit while the man is naked, which undressing leaves alone. The log says what each actor's body got (`Erection: ... body:`). MCM: Alignment page, **Erections** (`bErections`).
- **A penis bend of -10 is flaccid**, as OStim's (`SOSFlaccid`): the morphs go to 0 and the bodysuit comes off, and the bend node goes back to its own angle. Bodies that can't go flaccid (A-Body during a scene, others) are bent as far down as they go (-9) instead. The Align tab now goes down to -10.
- Saves made mid-scene: the erection morphs and a bodysuit 4Stim gave are removed when the save is loaded (save data version 6).

### Fixed
- **Fixed:** a crash on a scene's first second (the owner's game, 1.11.240): the collision watch read the physics world from the scene clock, which runs alongside the physics step, and the game's own read check crashed inside `bhkNPCollisionObject::Getbhk`. The collision watch is now off unless `bCollisionWatch=1` in `4Stim.ini` (only for chasing a shake). The furniture collision at scene start is unchanged.

### Undressing
- **Fixed:** the HUD's Undress did nothing for slots a scene had already undressed, so clothes an earlier strip missed (or the game put back on) stayed on. Undressing by hand now takes off everything worn in the undress slots again. The log names each item taken off (`Undress: ... to take off: <id> "<name>"`).
- **Fixed (needs testing):** a female player who stayed dressed all scene while the NPC undressed (a 1.10.163 tester). The log showed her one outfit item taken off and never put back on, yet her body drew the same 27 shapes all scene: the game had the clothes off but never redrew her. Two seconds after each strip, 4Stim now checks whether the items are off and whether the body drawn changed (`Undress check:` in `4Stim.log`), and redraws a player whose clothes are off but still drawn (`Actor::Reset3D`, as F4SE's `Actor.QueueUpdate`).

## 0.4.0 alpha (October 2026)

Everything since the 0.3.0 alpha and its two 1.10.163 hotfixes, from the alpha testers' reports. Needs every script recompiled (FourStim, FourStimScene, FourStimPhysics, FourStimUndress) and a clean install: delete the old 4Stim files first (docs/ALPHA.md).

### New
- **Collision watch** (for the player still shaking in a chair in a corner, with the chair out of the way): the scene guard checks what each scene actor's character controller touches. Each thing is logged once a scene, and anything but what they stand on, the ground and other actors joins the scene's collision group; if they still touch it, it collides with nothing until the scene ends. It also logs when the controller or the drawn body is away from the actor's spot (`Collision watch:` in `4Stim.log`). Walls done this way let other NPCs through them for the scene.
- **As OStim's lock:** foot IK and head tracking are off for scene actors (`bHumanoidFootIKDisable`, and Fallout 4's `SetHeadTracking`): head tracking turned heads off the animation, so kisses missed.
- **Navigation groups** (4Stim's; OStim's lists are flat): another pack's scenes linked into a scene are listed under one row named after the pack (its `modpack`), which opens a list of just those, with Back at the top, so a pack such as Atomic Lust no longer fills the bottom of the list. A navigation's own `group` names a group by hand (docs/SCENES.md).
- **Undressing, OStim's way for Fallout 4's clothes:** an MCM page of slot checkboxes (OStim's), OStim's default mapped to Fallout 4 (now also mouth and the modders' slots 54-58, where outfit mods put jackets and belts; neck stays on, as OStim's amulet slot), and OStim's NoStrip keyword rule (docs/UNDRESS.md).

### Fixed
- **Fixed (needs testing):** male NPCs invisible for whole scenes on a 1.10.163 tester's game, even with the camera's fade off: below the 3D's fade, the game keeps its own per-actor fade (a fade-out, `SetAlpha`, Invisibility) and pushes it onto the 3D every update. A scene actor left in a plain fade-out is now faded in again and a `SetAlpha` below 1 put back to 1 (`Scene guard: ... was fading out` / `had alpha`), and the "keeps being faded out" warning now says what the game's fade is made of.
- **Fixed:** weapons never put back on after a scene: the weapon came back from the plugin as a Form, which arrived in Papyrus as the wrong type ("Mismatched types" in the Papyrus log at every scene end). It's now a form ID (`FourStim.TakeSceneWeaponID`). Needs FourStim.psc and FourStimScene.psc recompiled.
- **Fixed:** "Cannot open store for class dynamichdt" errors at every scene start and end on games without FO4 Faster HDT-SMP, or with a body without SMP (BodyTalk): actors whose meshes name no physics file get no physics swaps or rebuilds.
- **Fixed:** an actor invisible for a whole scene (a 1.10.163 tester's guard in Diamond City): the camera's own actor fade, which hides whoever the camera is inside or close to, was fading them out every frame, faster than the scene guard fades them back. The camera's actor fade (`fActorFadeOutLimit:Camera`, `fActorFadeOutBoundMax:Camera`) is now off while any scene runs and back to the game's values after the last one (`Scene camera: actor fade` in `4Stim.log`). The collision watch no longer reports the character controller's height as an offset.
- **Fixed:** an actor barely following a scene, or turned 90 degrees in a chair scene: she had drawn her weapon before the scene, and taking the weapon off left her in the drawn state with bare fists, whose pose shows over the scene's animations (both such scenes in the owner's log, `weapon out` on every `Scene check`). As OStim's lock does, a drawn weapon is now put away with the game's own sheathe at scene start and by the scene guard, the player's too (`Weapons: ... puts their weapon away` in `4Stim.log`). Needs FourStim.psc and FourStimScene.psc recompiled.
- **Fixed:** actors shaking in chair scenes, worst by a desk or a wall: the scene lock moves an actor's character controller, which collides, and a chair scene stands the actors inside the chair's collision. The scene's furniture, and other furniture within 120 units of it, no longer collide with the scene's actors for the scene (`Furniture collision:` in `4Stim.log`). Not in OStim (Skyrim's lock ignores collision); walls and precombined desks aren't covered.
- **Fixed:** the third-person camera fading the player out kept winning between the guard's checks: scene actors are now faded back in on every tick of the scene clock, with a warning if something keeps fading them (`keeps being faded out`).
- **Fixed:** the player (or an NPC) in a scene but invisible: the third-person camera fades an actor out as it closes in on them, as it does against a wall, and once the scene switched to the free camera nothing faded them back in (`Visibility: ... fade 0.0` on the player in a wall scene). The scene guard fades scene actors back in (`Scene guard: ... was faded out` in `4Stim.log`).
- **Fixed:** the penis left at an odd angle after a scene, and its physics swinging about after: Fallout 4's own animations never move ZeX's genital bones, so the last pose a scene's animation gave them (AAF packs' animations key them) stayed. Their pose from before the actor's first scene is noted and put back when each scene ends, before the physics swap back and rebuild (`Genital bones:` in `4Stim.log`). The penis bend now also goes on top of the node's own angle and puts that back.
- **Fixed:** the scene restarting every second when an NPC's weapon counted as drawn whatever was done (a companion, in a tester's log): the scene guard puts a weapon away at most three times a scene, then leaves it.
- **Fixed:** physics swaps causing long freezes (and in two crash logs, a crash in FSMP's rebuild): FSMP rebuilds physics on the game's main thread for every swap and reset, and 4Stim asked for several at once, up to three tries each with FSMP's verbose logging on. Swaps now wait a second after a scene starts and go one actor at a time, the swaps back and rebuilds after a scene are spread a second or more apart, each swap is one call once a file spelling has worked, and verbose logging is off (docs/PHYSICS.md). Needs FourStimPhysics.psc recompiled.
- **Fixed:** the player could be in a scene but invisible: started from first person, a first-person body mod (True First Person) could leave the player's third-person body hidden after the scene camera switched to third person. The scene guard now shows it again (`Scene guard: the player's body was hidden` in `4Stim.log`).
- **Fixed:** NPCs' own AI could pull them out of a scene: a settler went back to her work spot three times in one scene, and each snap blew up her body's physics (stretched spikes) until a later scene. As OStim does, an NPC in a scene now gets a do-nothing scene package from the game's alias-package check, which comes before their own packages (settler jobs, sandboxing, work spots, follow and dialogue packages), until the scene ends. A real attack should still break a scene, as before (the package doesn't ignore combat), but that's untested.
- **Fixed:** tongues stuck out of the right cheek: their meshes were bound to Fallout 4's HEAD bone without its rotation (4Stim Equip Objects).
- **Fixed:** OStim-style scene files naming their Idles by editor ID ("animation") found none in game, since Fallout 4 drops Idles' editor IDs when it loads them: 4Stim now reads them from the plugin files.
- **Fixed:** the options' "Back to {actor}" row now shows the actor's name.
- **Fixed:** clothes coming back on mid-scene, so actors kept armor on or outfits came off only sometimes: NPCs no longer put their outfit back on by themselves, and anything 4Stim took off that's back on is taken off again within a second (docs/UNDRESS.md). What a full undress leaves on is logged with its slots.

### Diagnostics
- **Physics swaps, diagnostics:** a swap that finds no piece using its file (the owner's A-Body player, whose body names `MaleBody.xml`) now logs whether FSMP has the physics active or soft suspended, and what the meshes name. A body already on the swap's file (FSMP keeps swaps, saves included) counts as swapped. FourStimPhysics.psc uses no F4SE script types, so it compiles with the game's own sources. Needs FourStimPhysics.psc recompiled.
- **Diagnostics:** a second after each scene change, what each NPC is doing (`Scene check:` in `4Stim.log`, a warning when they're not on one of the scene's idles or are talking), for the reports of an NPC not following scene changes.
- **Diagnostics:** actors placed in a scene but not drawn: every scene actor's 3D is checked every 2 seconds and logged at the start and whenever it changes (`Visibility:` in `4Stim.log`), with a warning when something would hide them (no 3D, culled, faded out, no geometry, or the 3D away from the actor).

### Other game versions and build
- **Packaging:** `tools\package-alpha.ps1` also makes `4Stim-EquipObjects-<version>.zip` (the optional add-on), and the tester's guide says how to install clean and how to tell the new build from an old one (an old `4Stim.dll` left in a tester's `Data` folder stopped 4Stim loading on 1.10.163). Old F4SE (1.10.163) is told the version from `xmake.lua` too.
- **1.10.163:** the furniture collision and collision watch need two game functions the Runtime Database has no 1.10.163 entry for; where they aren't found, 4Stim now leaves both off (logged at load) instead of failing on first use mid-scene (docs/OLDGEN.md).
- **Build:** `xmake build -r` now builds releasedbg by default (optimized, with `4Stim.pdb`, in `build\windows\x64\releasedbg\`); plain "release" isn't one of the project's modes and made no .pdb.

## 0.3.0 alpha (October 2026)

OStim parity: everything on the roadmap's "OStim parity" list is in.

- **Fixed:** the HUD's options (strap-on, tongue, undress, mute, and other mods' options files) did nothing: the actor never reached the option's script. Options now go through `FourStimOptions.Run` with the actor's form ID.
- **Toy slots:** the actions have OStim's `toySlot`s (genital, anus, nipples, scrotum), so sex toys get used in scenes; the log says when a scene has none.
- **Other game versions (experimental):** CommonLibF4RD's resolver is built in: with [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394) installed, the same 4Stim.dll runs on 1.10.163, 1.10.984 and other 1.11.x versions, finding the game's code at startup (docs/OLDGEN.md). 1.11.240 still uses the Address Library. On 1.10.163, the 20 addresses the database can't find there come from their OG IDs (12) or are done another way (8; one, taking NPCs out of furniture, is lost).

- **For other mods** (docs/API.md): OStim NG's Papyrus API scripts as `FourThread`, `FourThreadBuilder`, `FourLibrary`, `FourMetadata`, `FourActor` and `FourFurniture` (OStim's function names: start scenes with options, follow them by thread ID, random scenes by tags and actions, scene data, per-actor and per-thread metadata, find furniture); new scene events `FourStim_OnNPCSceneStart` / `End` and `FourStim_OnSequenceEnd`.
- **C++ plugin interface** (`include/4Stim/PluginInterface.h`): OStim's interface map for F4SE plugins, with threads (listeners, builder), alignment and sex toys.
- **Sex toys** (OStim's): device plugins register toys through the plugin interface; 4Stim drives them from the player's scene, on actions' `toySlot`s, with OStim's settings (`[SexToys]`, MCM Sex toys page). **Intiface Central** built in (`bIntiface`): 4Stim connects to it and uses its vibrate-type devices with no other mod.
- **`playbackSpeed`** (OStim's): a scene speed can play its animation faster or slower (`bUsePlaybackSpeed`).
- **Physics swaps, suffix syntax:** `toSuffix` / `toReceivingSuffix` ("Hard", "HardBottom") swap whatever physics file the actor's body names for its suffixed version, so one entry covers a body's variants and other bodies named the same way (docs/PHYSICS.md).
- **Saved:** the equip objects chosen for actors (strap-on, tongue...) are kept in the save.
- **Fixed:** scene sounds (moans, climaxes, slaps) followed the player instead of staying where the scene is: sound files now play through one of the game's positional sound output models.
- **Fixed:** a penis could be left stretched and stuck at an angle after a scene: the physics swap back now waits until the actor is out of the scene's pose, and everyone's SMP physics is rebuilt from the reference pose after a scene (FSMP's `ResetPhysics`).
- **Controller scheme** (MCM Controller page, `sPadScheme`): 4Stim Modern (B to the tab buttons, then hide) or OStim Classic (OStim's navigation on the d-pad and A: Left to the tab buttons, Right back).
- **The HUD, closer to OStim's menu:** the tab buttons are OStim's menu selector (Up / Down move a ring, Yes / A opens the tab and goes back to the list); "Faster" / "Slower" rows under the Navigation list, as OStim's speed buttons; and OStim's fade: after `fHUDFadeTime` seconds (10) without input the HUD fades out and hands the keys back, and any of its keys brings it back.
- **Weapons back on after a scene:** the weapon taken off as a scene starts goes back on when it ends (the player and NPCs), also after loading a save mid-scene.
- **OStim's keys by default:** Up arrow starts a scene, numpad + / - speed, numpad 1 pull out, numpad 0 auto mode, numpad / free camera, numpad * hide the HUD, numpad . end, K search, L Align; and OStim's navigation keys for the HUD (numpad 8 / 5 / 4 / 6, 7 to choose, 9 between the list and the tab buttons; `SetOsaUpKey`...), besides the arrows and Enter.
- **Controller:** B goes from the HUD's list to its tab buttons (Up / Down pick a tab), B again hides the HUD while the scene goes on, and A shows it again. The old LB / RB (tabs) and Y / X (speed) are gone, and the d-pad's Left / Right only change an Align value (no more moving between the list and the tabs); the MCM's new Controller page (`[Controller]`) gives any scene key a button, none by default.
- **MCM Hotkeys page:** now sets `4Stim.ini`'s keys, showing them (OStim's defaults) in OStim's layout with its navigation keys; the MCM's own hotkeys are gone. Millisecond settings show in seconds in the MCM.
- **Physics:** a man on the receiving end (anal sex, pegging, fingering...) gets `MaleBodyHardBottom.xml`, decided by the scene's actions instead of the role's place, and switches as the scene changes.
- **Undressing scenes:** "take off her top / bottoms" are now "chest armor" (slot 41) and "outfit" (the whole outfit and its armor), as Fallout 4's clothes are one piece.
- **Fixed:** the MCM's theme (and HUD on / off, control scheme) didn't apply until the game restarted.
- **Actors and roles** (OStim's, docs/ACTORS.md): actor properties decide who can be in scenes and take which roles (an actor type, what they have for roles' requirements), with conditions written in the file instead of OStim's perks. Defaults for people, women, ghouls and super mutants (who only take roles made for them).
- **Equip objects and strap-ons** (OStim's): items put on during scenes by actions, actor properties, `4StimEquipObject` annotations or Papyrus, and taken away at the end; a strap-on for a woman in a role that needs a penis (with a strap-on mod's armor in `Equip Objects\`).
- **OStim's strap-on for Fallout 4** (optional add-on, 4Stim Equip Objects): Calyps' strap-on refit to True Wasteland Body as a BodySlide project, its dildo bending with the penis bend.
- **OStim's tongues** in the same add-on (normal, long, snake; pierced; cum variants), shown by tongue expressions (OStim's `phonemeObjects`).
- **"Who leads?"** for scenes without you: the picker asks which NPC takes the first role.
- **Futa:** an actor property gives those wearing an equippable penis one, with OStim's `futaUseMaleRole`, `futaUseMaleExcitement` and `futaUseMaleClimax`.
- **Your role:** OStim's order (those with a penis first), `PlayerAlwaysDom` / `Sub` settings, and the picker's "Your role?" step (`playerSelectRoleGay` on by default, as OStim).
- **Penis bend** (OStim's sosBend): per scene and role, from the scene file, the Align tab's new "Penis bend" row and `4StimBendSoS` annotations, by turning ZeX's `Penis_01_Offset` (docs/ALIGNMENT.md).
- **Test pack:** OStim's penis bends (`sosBend`) copied into the test scenes' roles as `penisBend` (149 scenes).
- **Fixed:** an actor whose high heels came off mid-scene floated at the heel height for the rest of the scene.
- **Heels off with HHS's own API:** in roles with the feet off the ground, the High Heels System's height is turned off for the scene (and back on after), instead of lowering the actor.
- **Furniture** (OStim's, docs/FURNITURE.md): change furniture mid-scene (the HUD's Utility tab, "Change furniture..."); `SetSelectFurniture=0` puts a new scene on the nearest furniture without asking, `SetEnableFurniture=0` leaves furniture out; furniture in use can't be used by anyone else; `SetResetClutter` puts back the loose items a furniture scene knocked about (only what it moved). Furniture another scene is on isn't offered.
- **Walls and shelves:** the test pack's wall and shelf scenes now play: walls are found by ray casts (Fallout 4 has hardly any wall-lean markers), shelves by their models. Spots are first guesses to tune.
- **Walls and edges, refined:** a wall only counts when nothing stands in front of it (an open shelf no longer lets a wall scene through), and furniture scenes take the side away from a wall, checked at several heights.
- **OStim's scene format, complete:** the actor `noStrip`, `singleSpeed`, OStim's eye directions (`lookUp` / `lookLeft`..., by turning the eye bones), `scaleOffsetWithFurniture` and furniture `multiplyScale`. The test pack now has OStim's actor scales, eye directions and the rest from its scene files.
- **MCM page** (docs/MCM.md): every setting a control can show, the climax effects (with the new `fClimaxGlowSize`), the MCM's own hotkeys, and export / import (OStim's mcmsettings.json); changes apply at once. The page has the 4Stim logo, and its labels are translated (they showed as `$4Stim_MCM_...` keys).
- **Options in the HUD** (docs/OPTIONS.md): OStim's options files in the Utility tab; each actor's page has undress, mute, and their strap-on and tongue (on / off, and which one). Papyrus `Mute` / `Unmute` / `IsMuted`, `SetEquipObjectID` / `GetEquipObjectID`.
- **Translations** (docs/TRANSLATIONS.md): the picker, HUD, notifications, MCM and options read `Interface\Translations\4Stim_<language>.txt`.
- **Navigation `border` and `noWarnings`** (OStim's): a colored ring around a navigation's icon, and no warning for links to scenes that aren't installed.
- **Papyrus:** `GetFurnitureType`, `IsFurnitureChildOf`, `GetSceneFurniture`, `GetSceneFurnitureType`, `ChangeFurniture`, `OpenFurnitureMenu`, and the `FourStim_OnFurnitureChange` scene event.
- **Changed:** `SetOnlyGayAnimsInGayScenes=0` no longer offers every scene: roles still need an actor of their type with what they require (OStim's; `unrestrictedNavigation=1` drops that too).

## 0.2.1 alpha (October 2026)

The 0.2.0 build, plus experimental support for the pre-next-gen game.

- **Fallout 4 1.10.163 (experimental):** 4Stim can load on the pre-next-gen version from an address table in `Addresses\` (docs/OLDGEN.md). The table is still missing 31 addresses, which a tester on 1.10.163 can supply.

## 0.2.0 alpha (October 2026)

Second test build: OStim's settings, file formats, faces, voices, peaks, events and lights (see `docs/ALPHA.md`).

- **OStim's settings:** `4Stim.ini` now uses OStim NG's setting names and defaults (docs/SETTINGS.md); the older names still work. Changed defaults, to OStim's: post-climax excitement 10 / 30, auto mode 7.5-15 s per scene, pull-out up to 90, 5 navigation steps, auto speed every 2.5-7.5 s between 15 and 85 excitement, furniture search 1600 units, no redress animations, clothes off all at once, and scenes in a save end on load (`bResumeScenes=1` starts them again).
- **OStim's file formats:** OStim-style scene files (one per file, id = file name, `speeds[].animation` found by Idle editor ID, `origin` / `priority` navigations, `offset`, `modpack`), sequence files, action files (Skyrim `strippingSlots` mapped to Fallout 4) and furniture type files are read alongside 4Stim's own.
- **Alignment like OStim's:** kept per set of actors (by sex, as OStim's defaults) in `Documents\My Games\Fallout4\4Stim\alignment.json`, in OStim's layout. The old `Alignment.json` is converted once.
- **Starting like OStim:** a new scene with you starts with a scene tagged `intro` (OStim's intro scenes are tagged in the test pack), else `idle`; off furniture someone stands, on a bed no one does. NPC-only scenes start with any fitting scene.
- **Climax timed by the animation:** with a climax animation, the climax happens at its `4StimClimax` annotation (or when it ends), not as it starts.
- **Auto mode fades** (`SetUseAutoFades`, and scenes marked `fadeOnEntry`).
- **Actor scaling and heels** (OStim's): actors are scaled to the animation's size, and heels from the High Heels System are scaled out or taken off (docs/SCALING.md).
- **Peaks and action sounds** (OStim's): actions find their thrusts and strokes from the actors' bones (or a timer, or the animation's `4StimPeak` annotations) and play their slaps and strokes on them (docs/EVENTS.md).
- **Events** (OStim's): spank and wash events with their sound, stimulation, camera shake, rumble and voice reactions, from the animation's `4StimEvent` annotations or Papyrus `FourStim.CallEvent`; scripts hear them through `FourStim_OnEvent` (docs/EVENTS.md). Also the `4StimPlayExpression` annotation.
- **Lights** (OStim's): a face, crotch or ass light on each actor during scenes, per sex, optionally only in the dark (docs/LIGHTS.md). Off by default, as OStim.
- **Heels from the mesh:** the High Heels System's `HHS` extra data in a shoe's mesh is read too (docs/SCALING.md).
- **Fixed:** 4Stim annotations (such as `4StimClimax`) were ignored more than a minute into a scene.
- **Facial expressions** (OStim's): faces by action, moan and climax faces, open mouths for oral, from OStim's own expression files mapped onto Fallout 4's face morphs (docs/EXPRESSIONS.md).
- **Sound** (OStim's voice sets): moans, muffled moans during oral, and climax sounds, per NPC, voice type or sex (docs/SOUND.md).
- **OStim's HUD controls** as an option (`sHUDControls=Columns`): Left / Right move between the list and the tab buttons.
- **Fixes:** floor scenes played on a bed now lie along the middle of it, not off its edge; a label typo ("sit up (...)gs", from OStim's own translation).
- **New, from OStim:** slow motion at a climax, fades to black as scenes with you start and end, the free camera's FOV during scenes, first person after scenes, a free camera switch, NPC scenes ending after 5 minutes, and keys for pull out, end scene, search, the Align tab, the free camera and hiding the HUD (unbound by default).

## 0.1.0 alpha (October 2026)

First test build, for private testers (see `docs/ALPHA.md`).

- **Scenes** from JSON files: solo, pair and group scenes, speeds, navigation between scenes, transitions and sequences.
- **Starting scenes like OStim's:** the hotkey lists who's near you, then where (here, or nearby furniture: beds, chairs, tables, mattresses...), then an idle scene. Scenes between NPCs only, and several scenes at once, each watchable from the picker.
- **Furniture:** scenes placed on beds, chairs, tables and mattresses, avoiding a bed's side against a wall.
- **In-scene HUD:** navigation, scene icons, actor and speed meters, themes, keyboard and gamepad.
- **Actions:** who does what to whom in each scene (OStim's action types); roles go to actors who fit them.
- **Excitement and climax:** meters driven by each scene's actions, climax at 100 with climax animations, screen effects and rumble, and scene-end rules.
- **Auto mode:** scenes move on by themselves (foreplay, intercourse, pull-out), faster with excitement.
- **Undressing:** what each scene needs comes off, one piece at a time, everything optionally at the start, and it all goes back on at the end. Undress / Dress anyone in the scene by hand from the HUD's Utility tab. Animated undressing scenes, and redress animations after a scene (OStim's, in the test pack).
- **Scene lock and alignment:** everyone in a scene is locked on its spot (nothing can push them off); the HUD's Align tab moves, turns and scales each actor per scene and role, saved to `Alignment.json`.
- **Saving mid-scene:** running scenes are kept in the save and start again when it's loaded.
- **NPC scene key** (Shift + `N` by default): start a scene between NPCs even while you're in one.
- **Scene guard:** NPCs are taken out of furniture and work spots when a scene starts, and kept on the scene's animations.
- **Physics swap** for A-Body's SMP during scenes.
- **Papyrus API and events** for other mods: start / change / speed / end / climax events, actions, excitement, auto mode and undressing calls.
