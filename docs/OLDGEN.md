# Other Fallout 4 versions (1.10.163, 1.10.984, other 1.11.x)

4Stim is built for Fallout 4 1.11.240 (AE), where it finds the game's code through the Address Library, as OStim does on Skyrim. On every other version it uses **CommonLibF4RD's Runtime Database** ([Runtime Database](https://www.nexusmods.com/fallout4/mods/108394), `Data\F4SE\Plugins\f4rd-runtime.bin`): 4Stim has CommonLibF4RD's own resolver built in (`src/compat/rd/`, MIT), which takes the addresses the database knows for the running version and finds the rest by searching the game's code at startup, exactly as CommonLibF4RD plugins do. So one 4Stim.dll covers:

| Version | F4SE | Addresses from |
|---|---|---|
| 1.11.240 (AE) | 0.7.x | the Address Library (as before); Runtime Database not needed |
| other 1.11.x (AE) | 0.7.x | Runtime Database (or the Address Library for that version, if installed) |
| 1.10.984 (NG) | 0.7.x | Runtime Database |
| 1.10.163 (OG) | 0.6.23 | Runtime Database, with 4Stim's own table (`Addresses\version-1-10-163-0.txt`) behind it |

**Status: experimental** on everything but 1.11.240: no one has played a scene there yet.

## How it loads

At startup (`src/compat/IDDB.cpp`): AE with its Address Library uses the Address Library. Otherwise, with `f4rd-runtime.bin` installed, every address goes through the Runtime Database (cached, and shared with other CommonLibF4RD plugins in the game), and 4Stim's own table for that version (if it has one) fills in anything the database can't. 4Stim then checks every address it needs (`src/compat/RequiredIDs.h`); if any can't be found, it doesn't load, and `4Stim.log` says which and why (not in the database, pattern not found...). A few it can do without (`src/compat/Fallbacks.h`, below): those are logged as warnings with what 4Stim does instead.

## 1.10.163: what the Runtime Database can't find

A 1.10.163 tester's log (October 2026) had the Runtime Database finding 87 of the 107 addresses; the other 20 have no 1.10.163 entry or search pattern in the database, or a pattern that no longer matches. 0.3.0 handles them like this:

- **12 from their OG IDs**, put in 4Stim's table: `alandtse/CommonLibF4` (a CommonLibF4 for OG and NG) names both IDs for `GetDisplayFullName`, `Actor::SetHeading`, `ToggleFreeCameraMode`, `QCameraEquals`, `SetGlobalTimeMultiplier`, the `BSTimer`, `BSInputEnableManager` and `BSAudioManager` singletons, `EnableUserEvent`, `BSInputEnableLayer::DecRef` and `bhkPickData::GetHitFraction`; the player singleton is F4SE 0.6.23's `g_player`. The OG Address Library at the start of `f4rd-runtime.bin` gives their addresses (`OG_EXTRA` in `tools/make-address-table.py`). Their signatures are the same as AE's in that CommonLibF4.
- **8 done another way** (`src/compat/Fallbacks.cpp`), only where the address isn't found (AE and NG still call the game's function):

| Missing | Instead |
|---|---|
| `Actor::GetSex` | the base NPC's sex (`TESNPC::GetSex`, which 1.10.163 has), as Papyrus's `ActorBase.GetSex` |
| `TES::Singleton`, `GridCellArray::Get` | nearby actors, furniture and clutter come from the player's interior cell, or the attached cells in the worldspace's cell map, and the sky cell (what `TES::ForEachReferenceInRange` covers) |
| `hkReferencedObject::RemoveReference` | after a raycast, 4Stim drops the Havok world reference itself (Havok's count: the low 16 bits) |
| `BSResource::ID::GenerateFromPath` | 4Stim works out a sound file's resource ID itself: the CRC-32 of the lowercase file name and folder, and the extension, as BA2 archives store files. On AE the first three sounds are checked against the game's (`Compat: resource ID of ...` in the log) |
| `BSScaleformTranslator::AddTranslations` | MCM's texts go into the game's translator table directly, as F4SE's own translation loader does |
| `BSSoundHandle::SetPosition` | sounds just follow the actor (`SetObjectToFollow`), without a start position |
| `Actor::StopInteractingQuick` | an NPC using furniture or an idle marker is moved onto their own spot through Papyrus (`FourStimScene.LeaveFurnitureByID`, `MoveTo`), which takes them out of it. Without it, a tester's NPC kept a mirror's idle marker animation under every scene |
| `TESPackage::CreatePackageByType` and its procedure set-up (2211661, 2211735; added after that log, so whether 1.10.163 finds them is untested) | the scene package (`src/ScenePackage.h`) is Fallout4.esm's `MQ101VaultTecRepDoNothingScene`, a "Wait" with StopMovement like OStim's, instead of a do-nothing package made as `Actor::InitiateDoNothingPackage` makes one. The hook itself is on Actor's vtable (slot 0x5B), which 1.10.163 already finds |

**A wrong database entry.** The Runtime Database's 1.10.163 address for `TESDataHandler::Singleton` (0x6B2A0) is code, not the data handler, so 4Stim read instructions as a pointer and crashed setting up a scene. F4SE 0.6.23's `g_dataHandler` and the OG Address Library (ID 711558) agree on 0x58CF080; `src/compat/IDDB.cpp` (`CORRECTIONS`) uses that over the database. The other singletons 4Stim uses match F4SE 0.6.23 and `alandtse/CommonLibF4`, as do the 32 functions that CommonLibF4 names for both versions.

**Calling Papyrus.** The VM's `DispatchStaticCall`, `DispatchMethodCall` and `SendEvent` take their arguments as the game's copy of MSVC's `std::function`. 1.10.163 was built with an older MSVC whose `std::function` is laid out differently (0x20 bytes, CommonLibF4RD's `msvc::function`) from today's (0x40, NG and AE), so the first call 4Stim made into Papyrus crashed there (the scene key, in a tester's crash log). On 1.10.163, `src/compat/ScrapFunction.h` passes the old layout.

**Added since 0.3.0:** `bhkNPCollisionObject::Getbhk` and `GetBody` (2277919, 2277926; the Runtime Database has no 1.10.163 entry for either, only patterns, untested) are used by the furniture collision and the collision watch (CHANGELOG). Where they aren't found, both are off: furniture stays solid for scene actors. The other things added since 0.3.0 that depend on the game's layout were checked against a 1.10.163 executable: Actor's vtable (the scene package hook at 0x5B, the sheathe at 0xC9), the character controller's (bhkCharRigidBodyController) and BSFadeNode's fade fields are where AE has them.

That covers all 20 from that log; a log from 1.10.163 with 0.3.0 says whether anything else is missing.

The F4SE version data says 4Stim is address-independent (signatures) and uses the 1.10.980 and 1.11.137 struct layouts (`src/PluginVersion.cpp`), as CommonLibF4RD plugins do, so F4SE on NG and AE versions loads it; old F4SE (1.10.163) asks `F4SEPlugin_Query` instead.

## Struct layouts

The Runtime Database finds functions and objects, not class layouts: 4Stim reads the game's classes as AE lays them out. NG uses the same layouts as AE (F4SE's 1.10.980 layout). OG (1.10.163) is older; the fields 4Stim reads matched AE's in the Runtime Database's layout notes, but that's the part a 1.10.163 tester has to confirm (scenes start, actors are placed and animated, the HUD works).

## The old table

Before the Runtime Database was built in, 1.10.163 depended on 4Stim's own table alone, made by:

`tools/make-address-table.py` makes it from two sources:

1. **CommonLibF4RD's Runtime Database** (`f4rd-runtime.bin`, [Nexus mod 108394](https://www.nexusmods.com/fallout4/mods/108394)). It knows most 1.10.163 addresses outright: 20,877 of CommonLibF4's 24,082, and 65 of the 96 4Stim uses.
2. **A CommonLibF4RD mapping file** from a 1.10.163 game. The database finds the other 34 by searching the game's code when it runs, so they have to come from a game running 1.10.163:
   1. Install Runtime Database and any plugin built on CommonLibF4RD (the "- RD" versions of mods, such as Mod Configuration Menu - RD).
   2. Next to that plugin's `.dll` in `Data\F4SE\Plugins\`, make an empty file with the same name ending in `.mapping` (`ModConfigurationMenuRD.dll` → `ModConfigurationMenuRD.mapping`).
   3. Start the game to the main menu and quit. The `.mapping` file now lists every address (startup is slower while it's there).
   4. Send the `.mapping` file (and `.mapping.fail`, if there is one), then delete it.

```
python tools/make-address-table.py --version 1.10.163.0 --database f4rd-runtime.bin --mapping ModConfigurationMenuRD.mapping
```

It writes the table, then says how many addresses it has and which of 4Stim's are still missing.

**Result (October 2026): the mapping doesn't fill the gap.** A tester's mapping (from Mod Configuration Menu - RD on 1.10.163) turned out to be the 1.10.163 Address Library itself: 1,582,975 entries keyed by 1.10.163's own IDs, identical to the table at the start of `f4rd-runtime.bin`. CommonLibF4RD finds the addresses that only have AE IDs by searching the game's code each time it runs, and doesn't write them out. With it the table has 20,914 of CommonLibF4's IDs, and 4Stim still lacks 34 of the 107 it needs (AE-only functions: sound handles, input layers, the player singleton, `Actor::GetSex`...). The database holds the search patterns for them (records link to signature entries per game version), so the ways left are:

1. **Search the code ourselves:** read those patterns from `f4rd-runtime.bin` and scan the 1.10.163 executable at startup, as CommonLibF4RD does. Needs the format worked out and a 1.10.163 tester.
2. **Build on CommonLibF4RD** ("Runtime Database version" in TODO.md), which does all of that and covers OG, NG and AE with one plugin.

Since then CommonLibF4RD's resolver was built in (above), which searches the code itself.

## Departures

- OStim (Skyrim) uses the Address Library on every version. On 1.10.163, 4Stim uses its own table, because Fallout 4's 1.10.163 Address Library uses different IDs from AE's.
- To read the table, 4Stim builds its own copy of CommonLibF4's address loader (`src/compat/IDDB.cpp`) and F4SE setup (`src/compat/F4SE_API.cpp`: F4SE 0.6.23 has no save folder name, so it's taken as `Fallout4`), and exports `F4SEPlugin_Query`, which old F4SE versions ask for.
- On 1.10.163, 8 game functions are replaced by 4Stim's own code (above); OStim has no such case.
