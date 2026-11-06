Scriptname FourSequence Native Hidden
{OStim NG's OSequence for 4Stim (docs\API.md): random sequences for actors,
with OStim's names. The actors in the role order given; FurnitureType ""
picks sequences played without furniture, a type picks those on it or on a
kind of furniture it is. "" if none fits.}

; Mode: 0 no tag test, 1 has Tags[0], 2 any of Tags, 3 all of Tags; CSV,
; if not "", is used for Tags instead.
string Function RandomSequence(Actor[] Actors, string FurnitureType, string[] Tags, string CSV, int Mode) Global Native

string Function GetRandomSequence(Actor[] Actors) Global
	return RandomSequence(Actors, "", None, "", 0)
EndFunction
string Function GetRandomFurnitureSequence(Actor[] Actors, string FurnitureType) Global
	return RandomSequence(Actors, FurnitureType, None, "", 0)
EndFunction

string Function GetRandomSequenceWithSequenceTag(Actor[] Actors, string Tag) Global
	string[] Tags = new string[1]
	Tags[0] = Tag
	return RandomSequence(Actors, "", Tags, "", 1)
EndFunction
string Function GetRandomSequenceWithAnySequenceTag(Actor[] Actors, string[] Tags) Global
	return RandomSequence(Actors, "", Tags, "", 2)
EndFunction
string Function GetRandomSequenceWithAnySequenceTagCSV(Actor[] Actors, string Tags) Global
	return RandomSequence(Actors, "", None, Tags, 2)
EndFunction
string Function GetRandomSequenceWithAllSequenceTags(Actor[] Actors, string[] Tags) Global
	return RandomSequence(Actors, "", Tags, "", 3)
EndFunction
string Function GetRandomSequenceWithAllSequenceTagsCSV(Actor[] Actors, string Tags) Global
	return RandomSequence(Actors, "", None, Tags, 3)
EndFunction

string Function GetRandomFurnitureSequenceWithSequenceTag(Actor[] Actors, string FurnitureType, string Tag) Global
	string[] Tags = new string[1]
	Tags[0] = Tag
	return RandomSequence(Actors, FurnitureType, Tags, "", 1)
EndFunction
string Function GetRandomFurnitureSequenceWithAnySequenceTag(Actor[] Actors, string FurnitureType, string[] Tags) Global
	return RandomSequence(Actors, FurnitureType, Tags, "", 2)
EndFunction
string Function GetRandomFurnitureSequenceWithAnySequenceTagCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return RandomSequence(Actors, FurnitureType, None, Tags, 2)
EndFunction
string Function GetRandomFurnitureSequenceWithAllSequenceTags(Actor[] Actors, string FurnitureType, string[] Tags) Global
	return RandomSequence(Actors, FurnitureType, Tags, "", 3)
EndFunction
string Function GetRandomFurnitureSequenceWithAllSequenceTagsCSV(Actor[] Actors, string FurnitureType, string Tags) Global
	return RandomSequence(Actors, FurnitureType, None, Tags, 3)
EndFunction
