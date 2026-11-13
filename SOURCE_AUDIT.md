# Source audit

What OStim NG is made of, what 4Stim took from each part and how (roadmap
1.6), and where every borrowed file in a 4Stim package comes from (1.9).

**Source of truth:** OStim NG, github.com/VersuchDrei/OStimNG, GPL-3.0.
Read at commit `2c98cb2` (22 October 2026). 4Stim is GPL-3.0 with the same
modding exception (LICENSE, EXCEPTIONS).

**Classes** (the instructions'): **Reusable** taken with little change;
**Adapted** OStim's design or format kept, the code or data changed for
Fallout 4; **Rewritten** done another way, because Fallout 4 has no
counterpart; **Unresolved** not yet assessed. Why each departure: DECISIONS.md.

## OStim NG's structure

| Part | Where in OStim NG | What it is |
|---|---|---|
| SKSE plugin | `skse/src/` (C++, CommonLibSSE) | The scene engine: threads, graph, actors, furniture, alignment, excitement, events, sounds, UI, MCM, APIs, serialization |
| Papyrus | `data/Scripts/Source/` (29 scripts) | OStim's script API (`OThread`, `OActor`...), the MCM, the main quest, add-on support |
| Data files | `data/SKSE/Plugins/OStim/` | Scenes, actions, action tags, actor properties, equip objects, events, facial expressions, furniture types, options, voice sets, settings, API forms |
| UI | `flash/` (ActionScript), `data/Interface/` | Scene, search and align menus, excitement bars (SkyUI-based) |
| Behaviors | `data/Nemesis_Engine/` | Behavior patches adding OStim's animation events and `OStimSpeed` |
| Plugin | `data/OStim.esp` | Factions, keywords, form lists, quests, equip object armors, lights |
| Meshes, textures | `data/meshes/OStim/`, `data/textures/` | Strap-ons, tongues, lights, tails, books, the vehicle marker |
| Sounds | `data/Sound/fx/OStim/` | Moans, climaxes, effects (thrust, spank, suck, handjob) |

## The SKSE plugin, part by part

| OStim NG (`skse/src/`) | 4Stim | Class |
|---|---|---|
| `Graph` (scenes, actions, action tags, sequences, navigation) | `SceneRegistry`, `Actions`, `Core/Scenes`, `Core/Navigation` | Adapted: same files and fields; animations played through Idle records |
| `Threading` (threads, thread actors, builders) | `Core/Threads`, `Core/Scenes`, `Core/Playback`, `Core/GroupScenes` | Adapted |
| `Core` (settings, the main loop) | `Core/Settings`, `main.cpp`, `Scheduler` | Adapted: OStim's setting names in `4Stim.ini` |
| `ActorProperties` | `ActorProperties` | Adapted: OStim's files; SoS / TNG's role taken by the body check |
| `Alignment` | `Alignment` | Adapted: OStim's `alignment.json` format, Fallout 4's Documents folder |
| `Furniture` | `Furniture`, `Core/Collision` | Adapted: OStim's furniture type files; Fallout 4's own objects (`Fallout4.json`) |
| `Events` (OStim's scene events, peaks) | `Events`, `Peaks`, `AnimCues` | Adapted: annotations timed by 4Stim (Fallout 4 doesn't fire them) |
| `Sound` (voice sets) | `Sounds` | Adapted: OStim's voice set files; sound files played directly |
| `Trait` (expressions, equip objects, facial animation) | `Expressions`, `EyeLook`, `EquipObjects`, `Lights` | Adapted: OStim's expression files turned into Fallout 4 face morphs |
| `GameLogic` / `GameAPI` / `Game` (Skyrim's game wrappers) | CommonLibF4 calls throughout, `Console`, `Actor3D` | Rewritten |
| `UI` (scene, search, align menus) | `HUD`, `Options`, `Core/Picker`, `Interface-src/` | Rewritten: 4Stim's own Scaleform HUD with OStim's functions |
| `MCM` | `MCM`, `Data/MCM/Config/4Stim/`, `FourStimMCM.psc` | Adapted: Fallout 4's MCM, OStim's pages and texts |
| `Settings` (add-on settings) | `AddonSettings` | Adapted |
| `Papyrus` / `ScriptAPI` (natives) | `src/Natives/` | Adapted: OStim's scripts under 4Stim's names (`OThread` → `FourThread`) |
| `ModAPI`, `PluginInterface*` (C++ APIs) | `Core/PluginAPI`, `Core/NGAPI` | Adapted: namespace `FourStimAPI`, scenes by ID |
| `SexToys` | `SexToys`, `Intiface` | Adapted; Intiface is 4Stim's own toy plugin |
| `Serial`, `Serialization` | `Core/SaveData` | Adapted: F4SE's co-save |
| `Util` | spread through the modules | Rewritten |
| SoS / TNG compatibility | `Erection`, `PenisBend`, `PenisMesh`, `Physics` | Rewritten: each body's own means |

Nothing is **Unresolved**: every subsystem has a 4Stim counterpart or a
documented reason it has none (vampire, beast race, tail and in-water
properties; RaceMenu height: DECISIONS.md).

## Papyrus

| OStim NG | 4Stim | Class |
|---|---|---|
| `OThread`, `OThreadBuilder`, `OActor`, `OActorUtil`, `OMetadata`, `OLibrary`, `OSequence`, `OFurniture`, `OEvent`, `OData`, `OJSON`, `OUndress` | `FourThread`, `FourThreadBuilder`, `FourActor`, `FourActorUtil`, `FourMetadata`, `FourLibrary`, `FourSequence`, `FourFurniture`, `FourEvent`, `FourData`, `FourJSON`, `FourUndress` | Adapted: same functions, Fallout 4 types |
| `OSexIntegrationMCM` | `FourStimMCM` and the MCM's `config.json` | Rewritten for Fallout 4's MCM |
| `OSexIntegrationMain`, `OStimPlayerAliasScript`, `OStimSubthread`, `OPlayerThread` | The plugin (`Core/`), `FourStimScene`, `FourStimMenu` | Rewritten: the engine's work is native |
| `OStimAddon`, `OSettings` | `AddonSettings`, `FourStim` | Adapted |
| `OSKSE`, `OSANative`, `OUtils`, `OUtility`, `OIntUtil`, `OCSV`, `OAIUtils`, `OActionData`, `OBarsScript`, `OSexBar` | Not taken: SKSE helpers and old OSA / OSex scripts with no use in Fallout 4, or done by the HUD | Rewritten |
| Mod events (`list of mod events.txt`) | `FourModEvents`, `SceneEvents` | Adapted: `RegisterForModEvent` for Fallout 4, and 4Stim's array events |

## Data files

| OStim NG (`SKSE\Plugins\OStim\`) | 4Stim (`F4SE\Plugins\4Stim\`) | Class |
|---|---|---|
| `scenes` | `Scenes` (OStim's format; plus YAML, 4Stim's) | Reusable: OStim's scene files load unchanged |
| `actions`, `action tags` | `Actions\Default.json`, `Action Tags` | Reusable |
| `actor properties` | `Actor Properties` | Adapted: Fallout 4 races and keywords (4StimNPC, 4StimFemale, 4StimGhoul, 4StimPenis) |
| `equip objects` | `Equip Objects` (README); the items in the Equip Objects add-on | Adapted |
| `events` | `Events` | Reusable |
| `facial expressions` (80) | `Facial Expressions` (the same 80) | Reusable; `Expression Morphs.json` maps them to Fallout 4 |
| `furniture types` | `Furniture\Fallout4.json` | Rewritten: Fallout 4's objects |
| `options` | `Options\4Stim.json` | Adapted |
| `voice sets` | `Voice Sets` | Adapted: sound files instead of Skyrim sound forms |
| `settings` | `Settings` (README) | Reusable |
| API forms (`OStim.esp`) | `4Stim.esl` (`tools/make-api-plugin.py`) | Adapted |

## Provenance: every borrowed file in a package

What `tools/package.ps1` (roadmap 3.4) puts in each zip, and where it came
from. Anything not here doesn't go in; the script refuses what's under
"Never packaged".

### `4Stim-<version>.zip` (the framework)

| Files | Source | Licence or permission |
|---|---|---|
| `4Stim.dll` | 4Stim's code, with CommonLibF4 and the libraries in DEPENDENCY_MATRIX.md | GPL-3.0 (libraries: GPL-3.0, MIT, BSD-3-Clause) |
| Papyrus scripts (`.pex` and sources): `FourStim*` and OStim's API under 4Stim's names (`FourThread`... `FourData`, `FourEvent`, `FourUndress`) | 4Stim's | GPL-3.0 |
| HUD and picker SWFs, icons | 4Stim's (`Interface-src/`; icons drawn in code) | GPL-3.0 |
| `4Stim` logo | Made for the project at the owner's request | The owner's |
| `4Stim.esl` | Made by `tools/make-api-plugin.py` from OStim.esp's API forms | GPL-3.0, as OStim NG |
| `Actions`, `Action Tags`, `Events`, `Facial Expressions` | OStim NG's data files | GPL-3.0 |
| `Actor Properties`, `Erection`, `Furniture`, `Lights`, `Options`, `Physics`, `Voice Sets`, `Expression Morphs.json`, `4Stim.ini`, MCM config, translations | 4Stim's, in OStim's formats where OStim has one | GPL-3.0 |
| `Sound\fx\4Stim\` (85 `.wav`: moans, climaxes, effects) | OStim NG's `Sound\fx\OStim\`, the same files (sizes match), in a 4Stim folder | GPL-3.0, as part of OStim NG |

### `4Stim-TestAnims-<version>.zip` (the base animation pack)

| Files | Source | Licence or permission |
|---|---|---|
| OStim's scene files, without the vampire ones (`*Vampire*`, `*Drained*`) | OStim NG's `scenes` | GPL-3.0 |
| Animations (`meshes\actors\character\animations\OStim`), converted for Fallout 4 | OStim's base animations | Permission for the base animations only (owner, 31 October 2026); without the vampire and tailjob ones |
| `4StimTestAnims.esp` (Idle records) | 4Stim's | GPL-3.0 |
| `4StimTestAnims.json` | 4Stim's additions | GPL-3.0 |
| Other packs (Atomic Lust and others) | Their authors | **No permission: in no package** |

### `4Stim-EquipObjects-<version>.zip` (optional)

| Files | Source | Licence or permission |
|---|---|---|
| Strap-on meshes and textures | OStim NG's "Calyps Strap-On" (Calyps; 3BA conversion by VersuchDrei), refit to TWB by `tools/refit/build_strapon.cpp` | GPL-3.0 as part of OStim NG; TWB refit cleared (owner, 13 November 2026) |
| BodySlide files (`4Stim OStim Strap-On (TWB)`) | Made by the refit tool, with TWB's weights and sliders | Cleared (owner, 13 November 2026) |
| Tongue meshes and textures | OStim NG's `meshes\OStim\tongues`, converted | GPL-3.0, as part of OStim NG |
| `4Stim - Equip Objects.esp`, equip object files | 4Stim's (`tools/make-equip-plugin.py`) | GPL-3.0 |

### Never packaged

`patches/Futanari FEV - 4Stim` (test material, owner, 13 November 2026),
`killer905_pose.json` and the `FourStimTest` console-test script (4Stim's, for testing only),
other converted animation packs, anything from Skyrim (no Skyrim `.pex`,
behaviors or `OStim.esp`).
