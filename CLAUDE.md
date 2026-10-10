# 4Stim: working rules

- **OStim first.** 4Stim is a port of OStim NG (github.com/VersuchDrei/OStimNG) to Fallout 4. Before building or changing any feature, read how OStim does it (its SKSE source, Papyrus scripts, scene files, MCM defaults) and match it as closely as Fallout 4 allows: the same behavior, defaults, setting names and scene / file formats, so users and modders coming from Skyrim recognize it. Only depart from OStim where Fallout 4 forces it (say so in the docs and code comments, naming the OStim original), or for features the project owner asks for that OStim doesn't have.
- Commit messages end with the attribution lines given in the session; push to KeryoWolfe/4Stim.
- The owner builds the plugin (`xmake build -r`), the SWFs (`Interface-src\build.bat`) and compiles Papyrus from `4Stim Core\Scripts\Source\User` (MO2 shows it as the game's Data folder). Nothing here can compile; review code carefully for API names against `lib/commonlibf4`.
