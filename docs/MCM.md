# The MCM page

With Fallout 4's [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497) installed, 4Stim has a page in it (pause menu, Mod Config, 4Stim) with every setting of `4Stim.ini` that a control can show, its hotkeys, and export / import, as OStim's MCM has. Without the MCM, `4Stim.ini` is all there is, as before.

## Pages

General, Camera, Furniture, Roles, Alignment, HUD, Excitement, Climax effects (slow motion, camera shake, blur and its strength, the edge glow's strength and size, rumble), Auto mode, Undressing, Faces and sound, Lights, Hotkeys, and Save and load. Each setting's help text is its comment in `4Stim.ini`, and its name there is in SETTINGS.md.

Only in `4Stim.ini` (the MCM has no control for them): the undress slots (`sUndressSlots`), bone names (`sPenisBendNode`, `sLeftEyeNode`, `sRightEyeNode`), sound flags (`iSoundFlags`), and the keys' Windows key codes (the Hotkeys page has the MCM's own hotkeys instead).

## Where the settings go

The MCM keeps what you change in `Data\MCM\Settings\4Stim.ini` (with Mod Organizer, in `overwrite`), over its defaults in `Data\MCM\Config\4Stim\settings.ini`. 4Stim reads `4Stim.ini` and then the MCM's file over it, and reads both again whenever a setting changes in the MCM, so changes apply at once.

When the game starts, whichever of the two you changed last wins: if `4Stim.ini` is newer than the MCM's file, its values are copied into the MCM's (so the MCM shows them); if you changed something in the MCM since, the MCM's values stay. **Use 4Stim.ini's settings** (Save and load) forgets what was changed in the MCM.

In the MCM's file, 4Stim's own setting names stay as they are (`bLockScenes`, `fActorRadius`); OStim's names get the type letter the MCM needs and `4S_` (`SetEndOnOrgasm` is `b4S_SetEndOnOrgasm`), and a choice (`sTheme`, the lights...) is stored as the dropdown's number (`i4S_sTheme=1`, PipBoy).

## Hotkeys

The Hotkeys page has the MCM's own hotkeys for every scene key: start a scene / the HUD, a scene without you, faster, slower, auto mode, pull out, end, search, Align tab, free camera, hide the HUD. They do what the keys in `4Stim.ini` do (`FourStim.PressKey`), and those keys keep working: bind each action in one place only. The MCM's hotkeys can have Shift, Ctrl or Alt with them.

## Export and import

As OStim's: **Export settings** writes the settings in effect to `Documents\My Games\Fallout4\4Stim\mcmsettings.json` (OStim's file name, next to `alignment.json`), by `4Stim.ini`'s sections and names; **Import settings** reads it back into the MCM's file. Copy the file to share settings, or keep it before trying something.

## For authors

`tools/make-mcm.py` writes the page (`config.json`, `settings.ini`, `keybinds.json`) from `4Stim.ini`: its table has each setting's label, kind and range. Its labels are `$4Stim_MCM_...` texts, translated like the rest (TRANSLATIONS.md).

The logo on the first page is MCM's `image` control, from `Data\MCM\Config\4Stim\lib.swf`, which `Interface-src\build.bat` builds from `Interface-src\logo\`. There are two versions of it while we find out which one MCM shows: `FourStimLogoBitmap`, the picture `4StimMCMLogo.png` itself (MCM's docs say images must be vector art), and `FourStimLogo`, the picture redrawn as vector layers with gradients and Flash bevels (`tools/trace-mcm-logo.py` traces them). MCM's image has no alignment, so each is centered in a clear box as wide as the page; `ROW_WIDTH` in `FourStimLogo.as` moves them.
