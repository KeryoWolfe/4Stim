# Dependency matrix

What 4Stim needs, at runtime and to build, what each is for, and what
4Stim takes from it. Roadmap 1.5. No versions are declared (owner's
decision, 31 October 2026): 4Stim is built and tested against the latest
of each, and if an update breaks 4Stim, the mod page says which version to
stay on until it's fixed.

"Shipped" means files of it are in 4Stim's packages. Nothing in the
runtime tables is: users install those themselves, under their own terms.

## Game and script extender

| Dependency | Required? | What 4Stim uses it for | Shipped |
|---|---|---|---|
| Fallout 4 1.11.240 (Steam) | Required | The supported runtime. Other versions (1.10.163, 1.10.984, other 1.11.x) are experimental (docs/OLDGEN.md). | No |
| [F4SE](https://f4se.silverlock.org/) for that runtime | Required | Loads the plugin (`4Stim.dll`); Papyrus natives, the co-save, input, tasks on the main thread. | No |
| Address Library for F4SE Plugins | Required on 1.11.240 | The game's addresses by ID. | No |
| [Runtime Database](https://www.nexusmods.com/fallout4/mods/108394) | Instead of the Address Library, on the experimental versions | The same, for runtimes the Address Library doesn't cover (CommonLibF4RD's resolver is built in). | No |
| `4Stim.esl` | Recommended | OStim's API factions, form lists and keywords for add-ons (docs/API.md). 4Stim runs without it, without those. | Yes (4Stim's own) |

## Optional mods 4Stim works with

| Dependency | What 4Stim does with it | Without it | Shipped |
|---|---|---|---|
| [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497) | The settings pages (docs/MCM.md) | Everything is in `4Stim.ini` | No |
| [LooksMenu](https://www.nexusmods.com/fallout4/mods/12631) | Erections by morphs on BodyTalk and Atomic Muscle's `.tri` install (docs/ERECTION.md) | No morph erections | No |
| FO4 Faster HDT-SMP | Physics swaps during scenes: erect (and receiving) physics for A-Body's male body (docs/PHYSICS.md) | No physics swaps | No |
| High Heels System | Actors in heels scaled or lowered to fit the animation (docs/SCALING.md) | Heels not corrected | No |
| ZeX skeleton's `Penis_*` bones | Penis bend and aim, the strap-on's bend, reading a penis from the body (docs/ERECTION.md, docs/ACTORS.md) | No bend or aim; a penis read only by actor properties and base sex | No |
| Male bodies: A-Body, BodyTalk, Atomic Muscle | Erections (by physics, morphs or the bodysuit's meshes), whichever is installed | No erections | No |
| True Wasteland Body | The body the Equip Objects add-on's strap-on is refit to | The strap-on won't fit another female body | No |
| [Intiface Central](https://intiface.com/central/) and a supported toy | Sex toys (docs/API.md, "Sex toys") | No toys | No |
| Animation packs (scene files and their animations) | What 4Stim plays. 4Stim ships no scenes of its own beyond `4StimTestAnims.json`'s additions; see "Animations" below | Nothing to play | See below |
| Addictol (or another crash logger) | Recommended: crash logs for bug reports (docs/ALPHA.md) | No crash logs | No |
| Futanari FEV | A test patch (`patches/Futanari FEV - 4Stim`): its AAF calls on 4Stim's scene events, and an actor property for its futas. Only for testing 4Stim's futa support (owner, 13 November 2026) | Not needed | No: never published |

## Libraries built into the plugin

| Library | Licence | What for |
|---|---|---|
| CommonLibF4 (KeryoWolfe/CommonLibF4, branch `4stim/combined`: Dear Modding's, with other forks' fixes) | GPL-3.0 | The game's types and functions |
| [commonlib-shared](https://github.com/Dear-Modding-FO4/commonlib-shared) (CommonLibF4's) | GPL-3.0 | CommonLibF4's shared base |
| [DearModdingUI-API](https://github.com/Dear-Modding-FO4/DearModdingUI-API) (CommonLibF4's) | GPL-3.0 | CommonLibF4's UI interface |
| CommonLibF4RD's resolver (`src/compat/rd/`) | MIT (`LICENSE-CommonLibF4RD.txt`) | Runtime Database support |
| spdlog (through commonlib-shared) | MIT | Logging |
| xbyak (through commonlib-shared, when enabled) | BSD-3-Clause | Code generation for hooks |
| nlohmann_json | MIT | JSON scene, action and settings files |
| yaml-cpp | MIT | YAML scene files (`src/YamlJSON.cpp`) |
| zydis | MIT | The resolver's instruction decoder |

All are compatible with 4Stim's GPL-3.0. CommonLibF4 and commonlib-shared
carry the same modding exception as 4Stim (their `EXCEPTIONS` files).

## Build tools (developers only)

| Tool | What for |
|---|---|
| [XMake](https://xmake.io) 3.0+, MSVC or Clang-CL (C++23) | The plugin (`xmake build -r`) |
| Apache Flex SDK (playerglobal 11.0), Java | The HUD's SWFs (`Interface-src\build.bat`) |
| Fallout 4 Creation Kit's Papyrus compiler | The scripts (`4Stim Core\Scripts\Source\User`) |
| BodySlide / Outfit Studio | Building the Equip Objects add-on's strap-on |

## Content 4Stim ships that came from elsewhere

The full provenance list belongs in SOURCE_AUDIT.md (roadmap 1.9); in short:

| Content | Source | Licence or permission | Status |
|---|---|---|---|
| Scene data, action types and other data formats | OStim NG (github.com/VersuchDrei/OStimNG) | GPL-3.0 | Taken |
| Animations: OStim's base animations, converted for Fallout 4 | OStim NG | Permission for the base animations only, without the vampire and tailjob ones (owner, 31 October 2026) | Beta packages only these |
| Other converted animation packs (Atomic Lust and others) | Their authors | No permission | Test material only: in no package |
| The strap-on (Equip Objects add-on) | OStim NG's "Calyps Strap-On" (Calyps; 3BA conversion by VersuchDrei) | GPL-3.0, as part of OStim NG | Taken; refit by `tools/refit/build_strapon.cpp` |
| The strap-on's TWB weights and sliders | True Wasteland Body | Cleared for use (owner, 13 November 2026) | Taken |
| Futanari FEV scripts (patched copies) | Futanari FEV (its scripts are under `Raknar:`) | None needed: the patch is test material, never published (owner, 13 November 2026) | In no package |
