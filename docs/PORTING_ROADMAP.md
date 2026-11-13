# 4Stim: plan to beta and first release

Written 29 October 2026, from the repository at `e21315e`, docs/TODO.md, docs/ALPHA.md and the owner's test logs of 23–25 October. It follows the project instructions (claude.ai project "4Stim"). Every status is one of **Tested** (seen working in game, with the log), **Built, untested** (in the code, not yet seen in game), **Open** (not done), or **External** (not 4Stim's to fix).

## Where the project stands

| | |
|---|---|
| Source of truth | OStim NG, github.com/VersuchDrei/OStimNG (GPL-3). The local copy was read at commit `2c98cb2` (22 October 2026). |
| 4Stim | 0.3.0 alpha, GPL-3 with the modding exception (LICENSE, EXCEPTIONS). Native plugin (CommonLibF4, xmake), Papyrus scripts, ActionScript 3 HUD, test animation pack. |
| Primary runtime | Fallout 4 1.11.240 (Steam), Address Library. Other versions through Runtime Database: experimental (docs/OLDGEN.md). |
| Owner's test setup | Mod Organizer 2, profile "4Stim Testing", a large load order (not a clean install). |
| Milestones in the instructions (A–F) | A–E reached in alpha: builds, loads, scenes start, play, change and end through the picker and HUD. F (feature migration) is largely done: docs/TODO.md, "OStim parity". |
| What's missing | Eight of the ten required documents; written test records; a provenance list; a dev branch; tested dependency versions. |

## Phase 1: bring the project in line with the instructions

No code changes. Claude does this; the owner reviews.

| # | Task | Done when |
|---|---|---|
| 1.1 | From release: `dev` branch for all work; `main` only takes what the owner has tested. Until release, work goes on `main` (see Decided). | Switched at release. |
| 1.2 | **PORTING_STATUS.md**: every feature in docs/TODO.md marked Tested / Built, untested / Open, with the log line that shows it. | Every "Done" in TODO.md has a status. |
| 1.3 | **TEST_PLAN.md**: the instructions' matrix (installation, dependencies, loading, Papyrus, actors, start, animation, end, cancel, save/load, cells, UI, compatibility, performance, packaging), with steps and expected log lines for each. | Each row has steps a tester can follow. |
| 1.4 | **TEST_RESULTS.md**, started from the 23–25 October tests (see Phase 2), in the instructions' format: setup and versions, steps, expected, actual, logs, status. | This week's tests recorded. |
| 1.5 | **DEPENDENCY_MATRIX.md**: F4SE, Address Library / Runtime Database, MCM, LooksMenu, FSMP, HHS, ZeX, A-Body, BodyTalk, Atomic Muscle, Equip Objects add-on, Intiface, Addictol (recommended); purpose, licence, required or optional. No versions (see Decided). | Every dependency listed. |
| 1.6 | **SOURCE_AUDIT.md**: OStim NG's structure and what 4Stim took from it (designs, file formats, action types, scene data), classed Reusable / Adapted / Rewritten / Unresolved. | Each OStim subsystem classed. |
| 1.7 | **ARCHITECTURE.md**: the plugin's modules (src/), the Papyrus scripts, the HUD, and how a scene's data and events move between them, with a diagram. | Matches the code. |
| 1.8 | **DECISIONS.md**: the departures from OStim and why (Idle forms instead of behavior files, the scene lock and collision, sheathing, physics swaps, Runtime Database...), gathered from code comments and commits. | One entry per departure. |
| 1.9 | **Provenance list** (in SOURCE_AUDIT.md): every borrowed file (OStim's action types and scene data, converted animations, icons, sounds) with its source and licence or permission. | Nothing in a release package is unaccounted for. |

## Phase 2: fix what the alpha testers found (beta blockers)

Claude writes the fix; the owner builds and tests; the result goes into TEST_RESULTS.md.

| # | Problem | Status | Next step |
|---|---|---|---|
| 2.1 | Actor missing from a scene (camera fade, worst in wall scenes) | Done (owner, 7 November 2026) | None. |
| 2.2 | Actor barely following a scene / turned 90° in a chair (weapon drawn) | Built, untested | Owner: draw a weapon, start a scene; look for `Weapons: ... puts their weapon away`. |
| 2.3 | Actors shaking in chair scenes | Done (owner, 7 November 2026) | None. |
| 2.4 | A-Body erect physics not swapping on the player | Done (owner, 7 November 2026: swaps to `MaleBodyHard.xml` at each scene start and back at the end) | None. |
| 2.5 | Penis left at an odd angle after scenes | Bend OK (owner, 7 November 2026); the log puts it back to its own angle at the end. Misaligned in places mid-scene, expected until the animations carry the penis aim (converter, another agent) | Recheck once the aimed animations are in. |
| 2.6 | Alignment: kisses missing, tongue on the top lip, hands clipping | Kisses and hands: animation-level, another agent (owner, 7 November 2026). Tongue: moved 1.5 up and 1.5 back into the mouth (7 November, untested) | Owner: tongue in a kneeling or licking scene. |
| 2.7 | Penis clipping through bodies and faces | Open: animation-level. Checked 1 November against the reconverted test pack (AnimBridge, 946 files): animations carry all 128 ZeX tracks; the male role animates `Penis_00` (the base) and nothing further along: `Penis_01`–`05` and the balls hold one pose. | The converter: carry the source's segment motion over to `Penis_01`–`05`, if the OStim originals have it; until then, the per-scene bend and alignment. |
| 2.8 | NPCs talk during scenes | Done (checked off by the owner, 7 November 2026) | None. |
| 2.9 | Scene events pass `Actor[]` from native code (may arrive broken) | Done (owner's log, 10 November): `Scene events: actors reach Papyrus as Actor[] (2 actor(s)): fine`. No change needed | Owner: send the `Scene events: actors reach Papyrus as` line. |
| 2.10 | Short hitch when a transition starts | Open | Claude: measure from the log; preload transition clips. |
| 2.11 | First-person / True First Person body hidden | Open; needs first person allowed in scenes to test | Owner decides whether 0.4 tests first person. |
| 2.12 | 1.10.163 (OG) tester: NPC stayed in a chair, player didn't follow, dialogue opened | Needs the current build and logs from the tester | Owner: send the tester the new build. |
| 2.13 | Save loads taking 8+ minutes | External: ModSwitchFramework_NG (confirmed by the owner) | None for 4Stim; worth a report to its author. |
| 2.14 | Crash at Tenpines Bluff (pathfinding) | External: navmesh, A Forest.esp / PRP | None for 4Stim. |
| 2.15 | No erections on non-SMP bodies (BodyTalk, Atomic Muscle); SMP bodies rigid all scene | Done (confirmed by a tester, 7 November 2026) | None. |
| 2.16 | Clothes not coming off (1.10.163 tester) | The log shows every strip done, OStim's way (what each position needs) | Tester: say which clothes stayed on in which position, or try Undress at the start. |

## Phase 2b: code restructure (before beta)

From a seasoned modder's review (7 November 2026). Details and order in docs/TODO.md, "Before Beta Release". Behaviour stays the same through 2b.1-2b.6; each is its own commit, built by the owner before the next.

| # | Task | Status | Done when |
|---|---|---|---|
| 2b.1 | Papyrus natives out of main.cpp into `src/Natives/`, namespaces instead of the `T_` / `M_` / `L_` / `F_` / `J_` / `A_` prefixes | Done: namespaces (built and working, 8 November); one file per script in `src/Natives/` with 2b.3 (built and working, 8 November) | Builds; every native still binds (no "does not match" or missing-native lines in the Papyrus log). |
| 2b.2 | Settings into `Settings.cpp`, read through a table instead of the `if (key == ...)` chain | Done (built and working, 8 November): `SETTINGS` in `Core/Settings.cpp`, one line per setting (key, what it sets, clamp), 189 keys, same bodies as the old branches (checked key by key). No glaze: the ini names don't match the fields and the settings live in a dozen structs, so glaze would need the same table (owner's choice, 8 November); glaze comes with 2b.4 | Builds; 4Stim.ini, OStim's names and the MCM all still apply in game. |
| 2b.3 | Rest of main.cpp (14,342 lines) split by area: alignment, camera, scene running, a thin main.cpp | Done (built and working, 8 November): `src/Core/` (19 files by main.cpp's old sections, `Core.h` for what they share), `src/Natives/`, main.cpp down to the entry points (docs/SOURCE_LAYOUT.md). Code moved, not changed; checked here with clang: every file compiles and every function has one definition | Builds; a scene plays as before. |
| 2b.4 | JSON loaders from nlohmann to glaze, scenes last | Deferred (8 November): measured first. The owner's 724 scenes from 635 files, with actor properties, expressions and lights, load in 795 ms on a thread of their own at startup (10 s in a later log, a cold start: still on its own thread, done long before the first hotkey), so the game never waits on it. The loaders are deliberately lenient (OStim's and 4Stim's layouts, fallback names, a wrong-typed field skipped, YAML); glaze is strict by default, so the switch would risk files that load today for time no one sees. Worth doing if loading ever gets slow | — |
| 2b.5 | One scheduler thread instead of detached sleep-then-queue threads | Done (7 November; built and working, 8 November): `src/Scheduler.h`, 17 thread starts moved onto it plus the autoplay clock; animation cue reads on one worker. Sex toy calls, Intiface, the preload and MigrateThread keep their own threads (they block) | Builds; timings in the log unchanged (scene end, transitions, resumed scenes, toys). |
| 2b.6 | Vanilla natives (EvaluatePackage, EnableAI, SetScale, SetPosition...) called directly, not through the Papyrus VM | Done (12 November, not yet built): `EvaluatePackage` now direct by default (`iEvaluatePackage=2`; the test logging taken out), the rest stays on Papyrus as below. Before: `SetScale` (built; sizes right in and after a scene, owner, 10 November) (4 places: the scene's scale, back after it, and the save's undo, which through Papyrus could land after a resumed scene had measured the actor) through `SetActorScale`, behind `bDirectNatives` (1; 0 = Papyrus as before). `EvaluatePackage`: CommonLibF4's first argument is `a_queue`, Skyrim's equivalent `a_immediate`, so the test switch `iEvaluatePackage` (10 November, not yet built) calls it directly with false (1) or true (2) and logs the package before, right after and half a second later; Papyrus (0) until that settles it. Runs with 1 and 2 (owner, 10 November) settled nothing: at a scene's start the scene package was already running before the call, and a scene's end evaluated through Papyrus (FourStimScene's release), outside the switch. The release now goes through it too (`FourStim.EvaluatePackageOf`). Run with 2 (owner, 12 November): at the scene's end the NPC was already back on its own package before the call (FF01E414 00038C10, Cait 0002A101), and the same after: the direct call does no harm, and nothing in 4Stim waits on it. Left on Papyrus: `EnableAI`, `AllowPCDialogue`, `BlockActivation`, `Game.SetPlayerAIDriven` (no direct function in CommonLibF4), `Debug.Notification` (nothing waits on it), `LockActor` / `UnlockActor`'s `SetRestrained` (Papyrus test natives, run on Papyrus's thread). Placement already moved to native `SetPosition` earlier | Builds; scaled scenes (`Scaling:` lines) look as before; with `bResumeScenes=1`, a resumed aligned scene keeps the right size. |
| 2b.7 | Who has a penis, from the body (OStim's `useSoSSex` / `useTNGSex`) | Done, built (10 November; the first build froze the picker: the main thread wasn't recognised and the look requeued itself, fixed). In the owner's log: the player "has one: BaseMaleBody:0Full (skinned to Penis_Offset_SMP)", a woman "has none". A wearable penis (Bag of Dicks' "[Schlong] Size 3" on Cait) read as none and was undressed: its mesh is skinned to the pelvis (from before ZeX's bones) and named "Schlong". Now a mesh named like one counts too (`sPenisMeshNames`, `penis,schlong`), the item that gives it is noted and Undress keeps it on (10 November, not yet built). Seen working (owner, 11-12 November): Bag of Dicks kept on and read as a penis; Futanari FEV's futas read as having one through the patch's actor property (`carried`, as the mod takes the penis off while clothed), get straight scenes, and keep it on; clothed men no longer read as having none (the body can only add a penis). Still to see: a woman in the strap-on reads as none. `PenisMesh.h`. No actor property deciding, anything worn (the naked body too) with a mesh skinned to `Penis_*` bones gives a penis; 4Stim's equip objects skipped (their biped clones); the answer kept until what's worn or the 3D changes (a fingerprint of the biped's clones), the 3D read only on the main thread (elsewhere the last answer, else the base sex and a look queued). `bPenisFromBody` (1), OStim's two names read as it | Builds; `Penis from body:` lines name the right mesh for a man, a woman, a woman with a wearable penis, and a woman in the strap-on (none). |

## Phase 3: beta (0.4.0)

| # | Task | Done when |
|---|---|---|
| 3.1 | Run TEST_PLAN.md on 1.11.240 in a separate MO2 profile, with backups. | Every row Pass, Fail (with an issue) or Not tested (with a reason). |
| 3.2 | At least one run in a small load order (4Stim and its requirements only). | Shows which problems come from 4Stim and which from other mods. |
| 3.3 | Beta tester guide (docs/BETA.md, from ALPHA.md): install, uninstall, requirements with versions, what to test, how to report. | Reviewed by the owner. |
| 3.4 | Package with tools/package-alpha.ps1 (renamed for beta), checking the zip against the provenance list: OStim's base animations only, without the vampire and tailjob ones (see Decided). | Nothing unlicensed in the zip; no other pack's animations; nothing from Skyrim. |
| 3.5 | Merge `dev` into `main`, tag `v0.4.0-beta`. | Owner's go-ahead. |

## Phase 4: first public release (1.0)

The instructions' definition of done:

- Documented, legally supportable origin (Phase 1.9).
- Runtime and dependency versions stated (1.5).
- All scripts compile with Fallout 4's compiler; the plugin builds and loads on 1.11.240.
- Scene lifecycle and cleanup tested, including save/load, cancellation and actors unloading (3.1).
- Install and removal instructions; known limitations; untested compatibility labelled as such (1.10.163 stays experimental until a tester confirms it).
- Release package validated (3.4).

## Support after release: working without logs

Once 4Stim is public, most reports will come with no logs: a sentence, maybe a screenshot or a clip. The release has to be ready to work from that. Planned before 1.0:

| # | Task | Done when |
|---|---|---|
| 4.1 | **Symptom index** (docs/TROUBLESHOOTING.md): each symptom the alpha found (invisible actor, not undressing, shaking, no menu, `$` labels, freezes, penis angle...) with its causes, the question that tells them apart, and the fix. Written from the alpha's reports and fixes as they happen. | Every fixed alpha bug has an entry. |
| 4.2 | **Diagnostics page in the MCM**: game version, address source, what was found missing at load, the dependencies 4Stim found (F4SE, MCM, LooksMenu, FSMP, HHS, ZeX), the last scene's actors and what each got (body, erection, physics, undress check), the last warning. One screenshot answers most first questions. | Shows every item. |
| 4.3 | **Short codes in the game's own messages** for known failures (e.g. "4Stim [UND-2]: clothes off but still drawn"), each one an entry in TROUBLESHOOTING.md, so a screenshot or a pasted line is enough to find it. | Every WARN a player would notice has a code. |
| 4.4 | **One-file report**: an MCM button that writes `4Stim-Report.txt` (versions, settings that differ from the defaults, the dependencies, the last scenes' lines from 4Stim.log), so asking for "the report" is one file, not three logs. | Written and read back once. |
| 4.5 | **Reproduction profiles**: MO2 profiles with the common combinations (A-Body, BodyTalk, Atomic Muscle, CBBE; ZeX or not; 1.11.240 and 1.10.163), to reproduce a described symptom without the reporter's files. | Each profile starts a scene. |
| 4.6 | **Bug report template** for Nexus and Discord: game version, body and skeleton, what happened, what scene, and where the report file is. Addictol recommended for its crash logs, since crash logs are what users share most. | On the mod page. |

## Later (after 1.0)

From docs/TODO.md, unchanged: in-game HUD editor, YAML scene files, NPC greetings, race filtering, claim registry, first-person camera. Separate projects: the animation converter rebuild (which also fixes 2.7, and now takes automatic body-mesh detection), the Sim Settlements 2 furniture module, placeable scene markers.

Pinned by the owner (1 November 2026): **an A-Body patch on 4Stim's events** instead of AAF's (A-Body's own swap script, listening to `FourStim_OnSceneStart` / `End`), after beta. Needs first: the scene events' actor arrays checked (2.9); 4Stim's own A-Body swap skipped when the patch is there; the receiving end and the undress redraw handled by the patch; a plugin (4Stim has none) or A-Body's author's permission to replace their script.

## Risks

1. **Scene animations are the limit on alignment.** Clipping and missed kisses mostly come from the converted animations (2.6, 2.7); the plugin can only correct them per scene.
2. **Fallout 4's collision fights the scene lock** (2.3); OStim has nothing for this, since Skyrim's lock ignores collision. The fix is new ground.
3. **Large load orders hide 4Stim's problems behind other mods'** (2.13, 2.14); hence 3.2.
4. **Other runtimes are untested** (2.12); the release must say so.
5. **Content provenance**: only OStim's base animations have permission (see Decided); other packs stay out of every package (1.9, 3.4).

## Decisions needed from the owner

None open.

## Decided by the owner

- **Dependency versions (31 October 2026):** none declared. 4Stim is built and tested against the latest version of each, so the release lists what's required and what's optional. If a dependency's update breaks 4Stim, the mod page says to stay on the version before it until 4Stim is fixed.
- **Crash logger (31 October 2026):** Addictol is the recommended one (not Buffout 4).

- **Branches (31 October 2026):** work goes on `main` until release; from release, on `dev` (1.1). `main` was brought up to `dev` on 31 October.
- **First person (31 October 2026):** not tested in 0.4; it waits until the 0.4.1 features and fixes are confirmed working (2.11).

- **Animations (31 October 2026):** 4Stim has permission for OStim's base animations only. The other packs converted for testing (Atomic Lust and others) are test material: they are not packaged in the beta or release builds. Anyone who wants them converts them from their own copies.
- **Base pack trimmed (31 October 2026):** the vampire and tailjob animations come out of the base pack, since they make no sense in Fallout 4. The action types for them (`tailjob`, `analtailsex`, `vaginaltailsex`, `pullingtail`, `strokingtail`, `massagingtail`) stay in Actions/Default.json, as OStim's, for other packs; only the scenes and animations go.
