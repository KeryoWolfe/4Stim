# The MCM page

With Fallout 4's [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497) installed, 4Stim has a page in it (pause menu, Mod Config, 4Stim) with every setting of `4Stim.ini` that a control can show, its hotkeys, and export / import, as OStim's MCM has. Without the MCM, `4Stim.ini` is all there is, as before.

## Pages

OStim's pages, in OStim's order, with its section headers, labels and help texts (OStim NG's `OSexIntegrationMCM.psc` and `OStim_ENGLISH.txt`): **General** (Scenes, Lights, Save and Load Settings), **Controls** (4Stim Keys, Navigation Keys), **Auto Control** (Auto Speed Control, NPC Scenes, Auto Navigation, Auto Mode Toggle, Auto Mode Settings), **Camera**, **Excitement** (Excitement Calculation, Excitement Bars, Orgasms), **Gender Roles**, **Furniture**, **Undressing** (with the undressing slots, as OStim's), **Expressions**, **Sound**, **Alignment**, **Toys**, **Debug** and **About**.

4Stim's own settings are on the page they belong to, after OStim's under the same header or under a header of 4Stim's: General's Partner (how the hotkey finds a partner) and HUD, Controls' Controller, Excitement's Climax Effects, and Expressions' Eyes. OStim's Actors page isn't here (4Stim's per-actor choices are HUD options, OPTIONS.md; the strap-on, tongue and player voice defaults are in 4Stim.ini, ACTORS.md), and its Add-Ons page is written at startup (below), nor are OStim's settings 4Stim doesn't have (auto export / import, SoS / TNG sexes, weapons and wigs, the player's voice, thrust screenshake, the dark face fix). OStim's "Use auto mode if player dominant / submissive" toggles aren't here either: OStim's own code doesn't use them yet (a TODO in `Thread::evaluateAutoMode`); its "vanilla scenes" toggle is `SetForceAIInConsensualScenes`..

Where 4Stim's setting works differently from OStim's, its help says what 4Stim does: Reset position (NPCs only), the scene start key (the picker, then the HUD's keys), Switch control mode (auto mode on / off for the scene), Alignment menu (saved at once), controller rumble (at a climax, not on thrusts), Toggle (also between the HUD's list and tabs), Automatically hide bars (with the HUD's fade, not after 15 s) and the toys' synchronization and scaling (one setting for every toy). OStim's tooltips that name Improved Camera say nothing of it here.

OStim's millisecond settings (expressions, moans, NPC scene length, excitement grace period, auto mode's scene lengths and speed changes) are in seconds here, as in OStim's MCM; `4Stim.ini` keeps milliseconds, as OStim. Each setting's name in `4Stim.ini` is in SETTINGS.md.

Only in `4Stim.ini` (the MCM has no control for them): bone names (`sPenisBendNode`, `sLeftEyeNode`, `sRightEyeNode`), the penis bend's shape (`sPenisBendUp`, `sPenisBendDown`, `sPenisFlaccid`), sound flags (`iSoundFlags`), Intiface Central's address (`sIntifaceAddress`), `bFurnitureOutOfTheWay`, `bPassThroughWalls`, the diagnostic `bCollisionWatch`, `bResetPhysicsAfterScene` (PHYSICS.md), and the keys' Windows key codes (the Controls page shows the keys themselves).

## Where the settings go

The MCM keeps what you change in `Data\MCM\Settings\4Stim.ini` (with Mod Organizer, in `overwrite`), over its defaults in `Data\MCM\Config\4Stim\settings.ini`. 4Stim reads `4Stim.ini` and then the MCM's file over it, and reads both again whenever a setting changes in the MCM, so changes apply at once.

When the game starts, whichever of the two you changed last wins: if `4Stim.ini` is newer than the MCM's file, its values are copied into the MCM's (so the MCM shows them); if you changed something in the MCM since, the MCM's values stay. **Use 4Stim.ini's settings** (General, Save and Load Settings) forgets what was changed in the MCM.

In the MCM's file, 4Stim's own setting names stay as they are (`bLockScenes`, `fActorRadius`); OStim's names get the type letter the MCM needs and `4S_` (`SetEndOnOrgasm` is `b4S_SetEndOnOrgasm`), and a choice (`sTheme`, the lights...) is stored as the dropdown's number (`i4S_sTheme=1`, PipBoy).

## Keys

The Controls page sets `4Stim.ini`'s keys, laid out as OStim's Controls page: the scene keys (start a scene / the HUD, a scene without you, faster, slower, pull out, free camera, search, Align tab, hide the HUD, auto mode) and the navigation keys (up, down, left, right, toggle, yes, end), with OStim's defaults. They're MCM key inputs, so they show the key that's set (no Shift / Ctrl / Alt). Its Controller section has the controller scheme (4Stim Modern or OStim Classic, `sPadScheme`) and picks a controller button for each scene key (LB, RB, LT, RT, X, Y, either stick pressed in, or Back; none by default); the d-pad, A and B are the HUD's. Earlier versions had MCM hotkeys here; they're gone, so a key set there no longer does anything.

## Export and import

As OStim's: **Export settings** writes the settings in effect to `Documents\My Games\Fallout4\4Stim\mcmsettings.json` (OStim's file name, next to `alignment.json`), by `4Stim.ini`'s sections and names; **Import settings** reads it back into the MCM's file. Copy the file to share settings, or keep it before trying something.

## Add-ons

OStim's add-on settings work as OStim's: an add-on puts a JSON file in `Data\F4SE\Plugins\4Stim\Settings\` (OStim's `settings` folder and format, `README.txt` there), with toggles, sliders, dropdowns and keymaps, each kept in one of its global variables, and they show on an **Add-Ons** page (before About), a header per add-on. Texts starting with `$` are translated, from the add-on's own translation file (`Interface\Translations\<its plugin>_en.txt`).

Departure from OStim, forced by Fallout 4: SkyUI builds pages while the game runs; Fallout 4's MCM reads a fixed `config.json`. So when the game starts, before any menu is read, 4Stim writes the Add-Ons page into its own `Data\MCM\Config\4Stim\config.json`, bound straight to the add-ons' globals (MCM's `GlobalValue`), and takes it out again when no add-on has settings; `4Stim.log` says what it found (`Add-on settings: ...`). A new or changed file shows after restarting the game. As OStim's, the values are the globals, saved with the game; they aren't in 4Stim's export / import. Untested so far: whether MCM's key input writes a keymap's global.

## For authors

`tools/make-mcm.py` writes the page (`config.json`, `settings.ini`, `keybinds.json`) from `4Stim.ini`: its tables have the pages (OStim's), each setting's kind and range, and which of OStim's label and tooltip keys it takes. Its texts are translated like the rest (TRANSLATIONS.md), under OStim's keys with "4stim" for "ostim" (`$ostim_reset_position` is `$4stim_reset_position`, `$ostim_tooltip_fades` is `$4stim_tooltip_fades`) and OStim's English from `tools/ostim-mcm-english.json` ("OStim" written "4Stim"). 4Stim's own settings get keys in the same style, `$4stim_<setting in snake case>` and `$4stim_tooltip_<...>` (`bLockScenes`: `$4stim_lock_scenes`), with their `4Stim.ini` comment as help; an OStim setting that works differently in 4Stim keeps OStim's label key but its help has a key of its own (`$4stim_tooltip_reset_position_4stim`), so an OStim translation can't put OStim's description on it.

The logo on the first page is MCM's `image` control: the class `FourStimLogo` in `Data\MCM\Config\4Stim\lib.swf`, which `Interface-src\build.bat` builds from `Interface-src\logo\` with the picture `4StimMCMLogo.png` inside (MCM's docs say images must be vector art, but a picture inside a movie clip shows; MCM tints it in the Pip-Boy color). MCM's image has no alignment, so the logo sits in a clear box that centers it on the page; `ROW_WIDTH` and `TOP_MARGIN` in `FourStimLogo.as` move it.
