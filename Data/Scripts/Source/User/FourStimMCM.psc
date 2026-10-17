Scriptname FourStimMCM Hidden

; What 4Stim's MCM page calls (Data\MCM\Config\4Stim\config.json and
; keybinds.json, docs\MCM.md). The MCM writes a changed setting into
; Data\MCM\Settings\4Stim.ini; the plugin then reads its settings again.

Function SettingChanged() Global
	FourStim.ReloadSettings()
EndFunction

Function ExportSettings() Global
	FourStim.ExportSettings()
EndFunction

Function ImportSettings() Global
	if FourStim.ImportSettings()
		RefreshMenu()
	endif
EndFunction

Function UseIniSettings() Global
	FourStim.UseIniSettings()
	RefreshMenu()
EndFunction

; The MCM's hotkeys (keybinds.json): asAction as FourStim.PressKey takes it.
Function PressKey(String asAction) Global
	FourStim.PressKey(asAction)
EndFunction

; The MCM shows the new values (through CallGlobalFunction, so this script
; compiles without the MCM's sources).
Function RefreshMenu() Global
	Var[] noArgs
	Utility.CallGlobalFunction("MCM", "RefreshMenu", noArgs)
EndFunction
