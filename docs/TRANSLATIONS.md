# Translations

4Stim's own text (the picker, the HUD's labels, notifications, the MCM page, its HUD options) is in `Data\Interface\Translations\4Stim_en.txt`, in Fallout 4's format: UTF-16 with a byte order mark, one `$key<tab>text` line each. To translate it, copy the file as `4Stim_<language>.txt`, with the game's language code (its `sLanguage`: `de`, `fr`, `es`, `it`, `pl`, `ptbr`, `ru`, `ja`, `cn`...), and translate the texts after the tabs. Keep each `{}` (a name or number goes there, in that order; `{:.0f}` is a number without decimals).

4Stim reads the English file and then the game language's over it, so a translation can leave texts out. 4Stim also hands the file to the game's own translator when the game starts, which is how the MCM's `$4Stim_MCM_...` labels are translated: the game itself only reads `<plugin>_<language>.txt` for a plugin of that name, and there's no `4Stim.esp`. The game gets one file, the language's or else the English, so a translation for the MCM should have every text (`tools/make-translations.py` writes them all). Translations are read again with `FourStim.ReloadScenes()`.

Scene names, navigation labels and the like come from the scene files, as in OStim: an animation pack translates its own.

For authors: `tools/make-translations.py` writes `4Stim_en.txt` from the plugin's `TR("$4Stim_Key", "English")` / `TRF(...)` calls, the movies' `Strings.t("$4Stim_Key", "English")`, the MCM page (`tools/make-mcm.py`) and the options files' `"english"`.
