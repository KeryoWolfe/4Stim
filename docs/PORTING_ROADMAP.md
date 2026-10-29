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
| 1.1 | `dev` branch for all work; `main` only takes what the owner has tested. | Branch pushed; the owner agrees. |
| 1.2 | **PORTING_STATUS.md**: every feature in docs/TODO.md marked Tested / Built, untested / Open, with the log line that shows it. | Every "Done" in TODO.md has a status. |
| 1.3 | **TEST_PLAN.md**: the instructions' matrix (installation, dependencies, loading, Papyrus, actors, start, animation, end, cancel, save/load, cells, UI, compatibility, performance, packaging), with steps and expected log lines for each. | Each row has steps a tester can follow. |
| 1.4 | **TEST_RESULTS.md**, started from the 23–25 October tests (see Phase 2), in the instructions' format: setup and versions, steps, expected, actual, logs, status. | This week's tests recorded. |
| 1.5 | **DEPENDENCY_MATRIX.md**: F4SE, Address Library / Runtime Database, MCM, FSMP, HHS, ZeX, A-Body, Equip Objects add-on, Intiface; purpose, licence, required or optional, version tested. | The versions come from the owner's MO2 profile. |
| 1.6 | **SOURCE_AUDIT.md**: OStim NG's structure and what 4Stim took from it (designs, file formats, action types, scene data), classed Reusable / Adapted / Rewritten / Unresolved. | Each OStim subsystem classed. |
| 1.7 | **ARCHITECTURE.md**: the plugin's modules (src/), the Papyrus scripts, the HUD, and how a scene's data and events move between them, with a diagram. | Matches the code. |
| 1.8 | **DECISIONS.md**: the departures from OStim and why (Idle forms instead of behavior files, the scene lock and collision, sheathing, physics swaps, Runtime Database...), gathered from code comments and commits. | One entry per departure. |
| 1.9 | **Provenance list** (in SOURCE_AUDIT.md): every borrowed file (OStim's action types and scene data, converted animations, icons, sounds) with its source and licence or permission. | Nothing in a release package is unaccounted for. |

## Phase 2: fix what the alpha testers found (beta blockers)

Claude writes the fix; the owner builds and tests; the result goes into TEST_RESULTS.md.

| # | Problem | Status | Next step |
|---|---|---|---|
| 2.1 | Actor missing from a scene (camera fade, worst in wall scenes) | Built, untested | Owner: wall scene; look for `keeps being faded out`. |
| 2.2 | Actor barely following a scene / turned 90° in a chair (weapon drawn) | Built, untested | Owner: draw a weapon, start a scene; look for `Weapons: ... puts their weapon away`. |
| 2.3 | Actors shaking in chair scenes | Partly fixed (chair collision); player still shook in a corner | Owner: same corner chair; send the `Collision watch:` lines. Claude: fix from what they show. |
| 2.4 | A-Body erect physics not swapping on the player | Diagnostics built, untested | Owner: send the `Physics: ... has no piece using` or `already uses` line. |
| 2.5 | Penis left at an odd angle after scenes | Built, untested | Owner: three scenes in a row, check after each. |
| 2.6 | Alignment: kisses missing, tongue on the top lip, hands clipping | Head tracking and foot IK now off (built, untested); tongue mesh open | Owner: kiss scenes. Claude: tongue position in the Equip Objects add-on. |
| 2.7 | Penis clipping through bodies and faces | Open: animation-level. The converter keys only the first 95 bones of ZeX's skeleton, so converted animations never move the penis. | Fix in the animation converter (separate project); until then, the per-scene bend and alignment. |
| 2.8 | NPCs talk during scenes | Open | Claude: silence scene actors and bystanders' greetings for the scene. |
| 2.9 | Scene events pass `Actor[]` from native code (may arrive broken) | Open | Claude: test with a listening script; pass form IDs if broken. |
| 2.10 | Short hitch when a transition starts | Open | Claude: measure from the log; preload transition clips. |
| 2.11 | First-person / True First Person body hidden | Open; needs first person allowed in scenes to test | Owner decides whether 0.4 tests first person. |
| 2.12 | 1.10.163 (OG) tester: NPC stayed in a chair, player didn't follow, dialogue opened | Needs the current build and logs from the tester | Owner: send the tester the new build. |
| 2.13 | Save loads taking 8+ minutes | External: ModSwitchFramework_NG (confirmed by the owner) | None for 4Stim; worth a report to its author. |
| 2.14 | Crash at Tenpines Bluff (pathfinding) | External: navmesh, A Forest.esp / PRP | None for 4Stim. |

## Phase 3: beta (0.4.0)

| # | Task | Done when |
|---|---|---|
| 3.1 | Run TEST_PLAN.md on 1.11.240 in a separate MO2 profile, with backups. | Every row Pass, Fail (with an issue) or Not tested (with a reason). |
| 3.2 | At least one run in a small load order (4Stim and its requirements only). | Shows which problems come from 4Stim and which from other mods. |
| 3.3 | Beta tester guide (docs/BETA.md, from ALPHA.md): install, uninstall, requirements with versions, what to test, how to report. | Reviewed by the owner. |
| 3.4 | Package with tools/package-alpha.ps1 (renamed for beta), checking the zip against the provenance list. | Nothing unlicensed in the zip; nothing from Skyrim. |
| 3.5 | Merge `dev` into `main`, tag `v0.4.0-beta`. | Owner's go-ahead. |

## Phase 4: first public release (1.0)

The instructions' definition of done:

- Documented, legally supportable origin (Phase 1.9).
- Runtime and dependency versions stated (1.5).
- All scripts compile with Fallout 4's compiler; the plugin builds and loads on 1.11.240.
- Scene lifecycle and cleanup tested, including save/load, cancellation and actors unloading (3.1).
- Install and removal instructions; known limitations; untested compatibility labelled as such (1.10.163 stays experimental until a tester confirms it).
- Release package validated (3.4).

## Later (after 1.0)

From docs/TODO.md, unchanged: automatic body-mesh detection, in-game HUD editor, YAML scene files, NPC greetings, race filtering, claim registry, first-person camera. Separate projects: the animation converter rebuild (which also fixes 2.7), the Sim Settlements 2 furniture module, placeable scene markers.

## Risks

1. **Scene animations are the limit on alignment.** Clipping and missed kisses mostly come from the converted animations (2.6, 2.7); the plugin can only correct them per scene.
2. **Fallout 4's collision fights the scene lock** (2.3); OStim has nothing for this, since Skyrim's lock ignores collision. The fix is new ground.
3. **Large load orders hide 4Stim's problems behind other mods'** (2.13, 2.14); hence 3.2.
4. **Other runtimes are untested** (2.12); the release must say so.
5. **Content provenance**: scene data and animations from other authors need permission before a public release (1.9).

## Decisions needed from the owner

- `dev` branch from now on (1.1)?
- Does 0.4 test first person (2.11)?
- Which dependency versions are supported: the ones in the owner's profile, or ranges?
- Permission status for the animation packs linked into the test pack (Atomic Lust and others).
