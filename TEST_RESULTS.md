# Test results

Every recorded run (roadmap 1.4), newest first, in the instructions'
format. Test numbers refer to TEST_PLAN.md; roadmap numbers to
docs/PORTING_ROADMAP.md. A result is Pass only when it was seen in game and
in the log.

## Setup (all runs below unless a run says otherwise)

| | |
|---|---|
| Game | Fallout 4 1.11.240.0 (Steam) |
| F4SE | 0.7.9.0 |
| 4Stim | `4Stim v0.5.0.0` (development builds from `main`, built by the owner with `xmake build -r`) |
| Load order | The owner's: Mod Organizer 2, profile "4Stim Testing", a large load order (not a clean install). No small-load-order run yet (test 13.2, roadmap 3.2). |
| Animations | The test pack (OStim's base animations, converted) and other converted packs (test material) |
| Logs | `4Stim.log`, `Papyrus.0.log`, Addictol crash logs, read by Claude from the owner's computer |

## Coverage of TEST_PLAN.md

| Area | Status |
|---|---|
| 1. Installation | Not tested as a whole: development builds are copied into a mod folder, not installed from zips (15.3 covers it at beta) |
| 2. Dependency checks | Not tested: needs the small load order |
| 3. Plugin loading | Pass (every run: `4Stim loaded`, `Papyrus functions bound`, `Scenes: loaded 724 scene(s)…`) |
| 4. Papyrus | Pass for 4.1 and 4.3 (owner compiles every change; an add-on's scene events registered, 10 November); 4.2 Pass in the runs below |
| 5. Actor validation | 5.2 Pass (12 November); 5.1, 5.3 Not tested |
| 6. Scene startup | 6.1, 6.2 Pass; 6.3, 6.4 Not recorded in this format |
| 7. Animation | 7.1 Pass (7 November, 2.3); 7.2 Pass with the known first-play hitch; 7.3 Pass; 7.4 Pass (12 November) |
| 8. Scene completion | 8.1 Pass (`Scene end: X back to where they stood before the scene`, every run); 8.2 Not tested |
| 9. Cancellation | Not tested in this format |
| 10. Save and load | Not tested in this format |
| 11. Cell transitions | Not tested |
| 12. UI and controls | 12.1, 12.3 Pass (used in every run); 12.2, 12.4 Not tested |
| 13. Compatibility | Not tested |
| 14. Performance | 14.1 measured for scene changes only (12 November) |
| 15. Packaging | Not tested (beta) |

## Runs

### 12 November 2026: strap-on, timing, EvaluatePackage

| | |
|---|---|
| Steps | `Intended gender only` off; two women (Cait, FF01E414), a strap-on scene; then a scene with the player, picking new scenes and scenes played before. `iEvaluatePackage=2`. |
| Expected | The strap-on goes on and its wearer reads "has no penis" throughout (7.4); timing lines for each pick; NPCs back on their own packages at the end. |
| Actual | `Equip objects: 00079305 puts on "ostimstrapon"`; no `Actor properties` change for her all scene; penis aim on the strap-on. Timing: 11-16 ms pick to task, `PlayIdle` 0.2-0.4 ms; the next frame 43-119 ms on a scene's first play, 4 ms when played before. `EvaluatePackage (scene end)`: package already the NPC's own before the call and after. |
| Status | **Pass** (roadmap 2b.6, 2b.7 done). The first-play hitch is known and kept for beta (owner). |

### 12 November 2026: futa detection, Futanari FEV test patch

| | |
|---|---|
| Steps | Futanari FEV with 4Stim's test patch: a futa made with the mod's stim, scenes with Cait; a tongue scene; the mod's Cure. |
| Expected | The futa reads "has a penis" while clothed and naked; straight scenes; the penis stays on through tongues; the Cure removes it for good. |
| Actual | Futa: `has a penis, "sex" from Futanari FEV - worn.json` (with `carried`); straight scenes picked. Tongue: `lost 1 item(s) … putting back` the penis and scarves. Cure: penis gone and stayed gone (after the patched Cure found the mod's list). Clothed men: "has a penis" (the body check can only add one). |
| Status | **Pass** after fixes in this run: clothed men read as "no penis" (Fail, fixed), the futa lost while clothed (Fail, fixed with `carried`), the penis knocked off by tongues (Fail, fixed). The patch is test material only. |

### 11 November 2026: Goodneighbor crash

| | |
|---|---|
| Steps | Entering Goodneighbor. |
| Actual | Two crashes, both in Goodneighbor's visibility code (`BGSGrassManager::RemoveCellGrass` through Addictol's `BSPreCulledObjects` module; the distant fade for loaded cells), around PRP's Goodneighbor cells. 4Stim idle, not on either stack. |
| Status | **Not 4Stim** (external: Addictol's pre-culled objects and PRP). |

### 10 November 2026: direct natives, body check, scene events

| | |
|---|---|
| Steps | `bDirectNatives=1`; scenes with differently sized actors; the picker near men and women; an add-on registered for scene events. |
| Actual | Sizes right (`SetScale` direct). `Penis from body:` men by their naked body (`skinned to Penis_Offset_SMP`), women none. `Scene events: actors reach Papyrus as Actor[] (2 actor(s)): fine`. The picker first softlocked (the body check queued itself for ever off the main thread): fixed the same day. Bag of Dicks' penis read and kept on. |
| Status | **Pass** after the softlock fix (roadmap 2.9, 2b.6, 2b.7). |

### 8 November 2026: code restructure

| | |
|---|---|
| Steps | Each restructure stage built and played (namespaces, natives by script, settings table, `main.cpp` split, the scheduler). |
| Actual | Builds; scenes start, play and end as before. |
| Status | **Pass** (roadmap 2b.1-2b.3, 2b.5). |

### 7 November 2026: alpha fixes

| Problem (roadmap) | Result |
|---|---|
| 2.1 Actor missing from a scene (camera fade) | **Pass** |
| 2.3 Actors shaking in chair scenes | **Pass** |
| 2.4 A-Body erect physics not swapping on the player | **Pass**: swaps to `MaleBodyHard.xml` at each scene start and back at the end |
| 2.5 Penis left at an odd angle after scenes | **Pass** for the bend; mid-scene aim is animation-level |
| 2.8 NPCs talk during scenes | **Pass** |
| 2.15 No erections on non-SMP bodies | **Pass** (confirmed by a tester) |

### 23-25 October 2026: alpha testers

The problems found are roadmap 2.1-2.16 (Phase 2), with their status
there. Still open from them: 2.2 (weapon drawn: built, untested), 2.6
(tongue position: untested), 2.7 (penis clipping: animation-level), 2.11
(first person: not tested in 0.4, owner), 2.12 and 2.16 (1.10.163 tester:
need their logs with the current build).
