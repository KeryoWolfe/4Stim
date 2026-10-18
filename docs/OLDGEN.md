# Older Fallout 4 versions (1.10.163)

4Stim is built for Fallout 4 1.11.240 (AE). It can also run on 1.10.163, the version before the next-gen update, with an **address table**: a file that tells it where each game function it uses is in that version. OStim has no such thing (Skyrim SE / AE builds use the Address Library); Fallout 4's Address Library for 1.10.163 numbers things differently from AE's, so 4Stim carries its own table instead.

**Status: experimental.** The table isn't complete yet (below), and no one has played a scene on 1.10.163.

## What's needed on 1.10.163

- F4SE 0.6.23 (the 1.10.163 one).
- 4Stim as usual, with `Data\F4SE\Plugins\4Stim\Addresses\version-1-10-163-0.txt`.
- The Address Library isn't needed for 4Stim.

When the game starts, 4Stim looks for `Addresses\version-<game version>.txt`. If it's there, it uses it instead of the Address Library and checks that every address it needs is in it. If any are missing, 4Stim doesn't load, and `4Stim.log` lists them:

```
Address table F4SE\Plugins\4Stim\Addresses\version-1-10-163-0.txt: missing ID::PlayerCharacter::Singleton (4798212)
...
4Stim needs 31 address(es) the table doesn't have; not loading
```

On AE there's no table for the version, so nothing changes.

## Making the table

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

Until then 1.10.163 doesn't load (4Stim checks every address at startup and says which are missing).

## Departures

- OStim (Skyrim) uses the Address Library on every version. On 1.10.163, 4Stim uses its own table, because Fallout 4's 1.10.163 Address Library uses different IDs from AE's.
- To read the table, 4Stim builds its own copy of CommonLibF4's address loader (`src/compat/IDDB.cpp`) and F4SE setup (`src/compat/F4SE_API.cpp`: F4SE 0.6.23 has no save folder name, so it's taken as `Fallout4`), and exports `F4SEPlugin_Query`, which old F4SE versions ask for.
- 1.10.984 (next-gen) isn't supported: no tester.
