Scriptname FourData Native Hidden
{OStim NG's OData for 4Stim (docs\API.md): OStim's data functions, under
its names. Most are FourStim's; this script is for add-ons written against
OData. Departures are noted by each.}

; Bit 0 = slot 30, Fallout 4's slots (OStim's are Skyrim's). Set lasts until
; the settings are read again (OStim's is in the save).
int Function GetUndressingSlotMask() Global
	return FourStim.GetUndressingSlotMask()
EndFunction
Function SetUndressingSlotMask(int Mask) Global
	FourStim.SetUndressingSlotMask(Mask)
EndFunction

; The names of id, name pairs.
string[] Function PairsToNames(string[] Pairs) Global
	int iCount = Pairs.Length / 2
	if iCount < 1
		string[] None
		return None
	endif
	string[] Names = new string[iCount]
	int i = 0
	while i < iCount
		Names[i] = Pairs[i * 2 + 1]
		i += 1
	endwhile
	return Names
EndFunction

string[] Function GetEquipObjectTypes() Global
	return FourStim.GetEquipObjectTypes()
EndFunction
string[] Function GetEquipObjectPairs(int FormID, string Type) Global
	return FourStim.GetEquipObjectPairs(FormID, Type)
EndFunction
string Function GetEquipObjectName(int FormID, string Type) Global
	return FourStim.GetEquipObjectName(FormID, Type)
EndFunction
Function SetEquipObjectID(int FormID, string Type, string ID) Global
	FourStim.SetFormEquipObjectID(FormID, Type, ID)
EndFunction

string[] Function GetVoiceSetPairs() Global
	return FourStim.GetVoiceSetPairs()
EndFunction
string Function GetVoiceSetName(int FormID) Global
	return FourStim.GetVoiceSetName(FormID)
EndFunction
Function SetVoiceSet(int FormID, string Voice) Global
	FourStim.SetVoiceSet(FormID, Voice)
EndFunction

string[] Function GetActions() Global
	return FourStim.GetActions()
EndFunction
float Function GetActionStimulation(int Role, int FormID, string Actn) Global
	return FourStim.GetActionStimulation(Role, FormID, Actn)
EndFunction
Function SetActionStimulation(int Role, int FormID, string Actn, float Stimulation) Global
	FourStim.SetActionStimulation(Role, FormID, Actn, Stimulation)
EndFunction
float Function GetActionMaxStimulation(int Role, int FormID, string Actn) Global
	return FourStim.GetActionMaxStimulation(Role, FormID, Actn)
EndFunction
Function SetActionMaxStimulation(int Role, int FormID, string Actn, float Stimulation) Global
	FourStim.SetActionMaxStimulation(Role, FormID, Actn, Stimulation)
EndFunction
float Function GetActionDefaultStimulation(int Role, string Actn) Global
	return FourStim.GetActionDefaultStimulation(Role, Actn)
EndFunction
Function ResetActionStimulation(int Role, int FormID, string Actn) Global
	FourStim.ResetActionStimulation(Role, FormID, Actn)
EndFunction
float Function GetActionDefaultMaxStimulation(int Role, string Actn) Global
	return FourStim.GetActionDefaultMaxStimulation(Role, Actn)
EndFunction
Function ResetActionMaxStimulation(int Role, int FormID, string Actn) Global
	FourStim.ResetActionMaxStimulation(Role, FormID, Actn)
EndFunction

string[] Function GetEvents() Global
	return FourStim.GetEvents()
EndFunction
float Function GetEventStimulation(int Role, int FormID, string Evt) Global
	return FourStim.GetEventStimulation(Role, FormID, Evt)
EndFunction
Function SetEventStimulation(int Role, int FormID, string Evt, float Stimulation) Global
	FourStim.SetEventStimulation(Role, FormID, Evt, Stimulation)
EndFunction
float Function GetEventMaxStimulation(int Role, int FormID, string Evt) Global
	return FourStim.GetEventMaxStimulation(Role, FormID, Evt)
EndFunction
Function SetEventMaxStimulation(int Role, int FormID, string Evt, float Stimulation) Global
	FourStim.SetEventMaxStimulation(Role, FormID, Evt, Stimulation)
EndFunction
float Function GetEventDefaultStimulation(int Role, string Evt) Global
	return FourStim.GetEventDefaultStimulation(Role, Evt)
EndFunction
Function ResetEventStimulation(int Role, int FormID, string Evt) Global
	FourStim.ResetEventStimulation(Role, FormID, Evt)
EndFunction
float Function GetEventDefaultMaxStimulation(int Role, string Evt) Global
	return FourStim.GetEventDefaultMaxStimulation(Role, Evt)
EndFunction
Function ResetEventMaxStimulation(int Role, int FormID, string Evt) Global
	FourStim.ResetEventMaxStimulation(Role, FormID, Evt)
EndFunction

; Back to 4Stim.ini's settings (OStim's: its defaults; 4Stim.ini is where
; 4Stim's defaults are, docs\MCM.md).
Function ResetSettings() Global
	FourStim.UseIniSettings()
EndFunction
Function ExportSettings() Global
	FourStim.ExportSettings()
EndFunction
Function ImportSettings() Global
	FourStim.ImportSettings()
EndFunction

string Function ToLocalizedString(string Text) Global
	return FourStim.Localize(Text)
EndFunction
string Function Localize(string Text) Global
	return FourStim.Localize(Text)
EndFunction

; Departure: reloads every scene file (4Stim's ReloadScenes), not only
; SceneID's.
Function ReloadScene(string SceneID) Global
	FourStim.ReloadScenes()
EndFunction
