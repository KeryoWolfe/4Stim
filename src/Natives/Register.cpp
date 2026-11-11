#include "Core/Core.h"

namespace Core
{
	// Papyrus-facing script name. Must be "FourStim", not "4Stim" -- Papyrus
	// identifiers can't start with a digit. The plugin/project itself is
	// still called 4Stim everywhere else (xmake project name, log text).
	// Must match the .psc script name exactly.
	constexpr auto SCRIPT_NAME = "FourStim"sv;

	template <StimOverrides::Kind K>
	float GetStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		return StimOverrides::Stimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id).value_or(DefaultStim(K, a_role, a_id).first);
	}

	template <StimOverrides::Kind K>
	float GetMaxStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		return StimOverrides::MaxStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id).value_or(DefaultStim(K, a_role, a_id).second);
	}

	template <StimOverrides::Kind K>
	void SetStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id, float a_value)
	{
		StimOverrides::SetStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id, a_value);
	}

	template <StimOverrides::Kind K>
	void SetMaxStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id, float a_value)
	{
		StimOverrides::SetMaxStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id, a_value);
	}

	template <StimOverrides::Kind K>
	float GetDefaultStim(std::monostate, std::int32_t a_role, std::string a_id)
	{
		return DefaultStim(K, a_role, a_id).first;
	}

	template <StimOverrides::Kind K>
	float GetDefaultMaxStim(std::monostate, std::int32_t a_role, std::string a_id)
	{
		return DefaultStim(K, a_role, a_id).second;
	}

	template <StimOverrides::Kind K>
	void ResetStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		StimOverrides::ResetStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id);
	}

	template <StimOverrides::Kind K>
	void ResetMaxStim(std::monostate, std::int32_t a_role, std::int32_t a_formID, std::string a_id)
	{
		StimOverrides::ResetMaxStimulation(K, a_role, static_cast<std::uint32_t>(a_formID), a_id);
	}

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		g_vm = a_vm;

		REX::INFO("RegisterPapyrusFunctions called, binding {}", SCRIPT_NAME);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartScene"sv, StartScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "TestPluginAPI"sv, TestPluginAPI);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopScene"sv, StopScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SuppressInteraction"sv, SuppressInteraction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RestoreInteraction"sv, RestoreInteraction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "LockActor"sv, LockActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnlockActor"sv, UnlockActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "MoveActorTo"sv, MoveActorTo);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSelectedActorID"sv, GetSelectedActorID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlacePair"sv, PlacePair);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IgnorePairCollision"sv, IgnorePairCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RestoreCollision"sv, RestoreCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlaySceneIdles"sv, PlaySceneIdles);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActorCount"sv, GetSceneActorCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "QueueSequence"sv, QueueSequenceNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartSequenceOnScene"sv, StartSequenceOnSceneNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopSequence"sv, StopSequenceNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ReloadScenes"sv, ReloadScenes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneHasAction"sv, SceneHasAction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneHasActionTag"sv, SceneHasActionTag);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActionCount"sv, GetSceneActionCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "FindSceneAction"sv, FindSceneAction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActionType"sv, GetSceneActionType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActionRole"sv, GetSceneActionRole);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionTypes"sv, GetActionTypes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "CallEvent"sv, CallEvent);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEvents"sv, GetEvents);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionName"sv, GetActionName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionTags"sv, GetActionTags);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ActionHasTag"sv, ActionHasTag);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetExcitement"sv, GetExcitement);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetExcitement"sv, SetExcitement);
		a_vm->BindNativeMethod(SCRIPT_NAME, "AddExcitement"sv, AddExcitement);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetTimesClimaxed"sv, GetTimesClimaxed);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Climax"sv, Climax);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StallClimax"sv, StallClimax);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsClimaxStalled"sv, IsClimaxStalled);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetExcitementMultiplier"sv, GetExcitementMultiplier);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetExcitementMultiplier"sv, SetExcitementMultiplier);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetTimeUntilClimax"sv, GetTimeUntilClimax);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetAutoMode"sv, SetAutoMode);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NoteStripped"sv, NoteStripped);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UndressActor"sv, UndressActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RedressActor"sv, RedressActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsInScene"sv, IsInScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsAutoMode"sv, IsAutoMode);
		a_vm->BindNativeMethod(SCRIPT_NAME, "AutoTransition"sv, AutoTransition);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneActorHasTag"sv, SceneActorHasTag);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActorTags"sv, GetSceneActorTags);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopPair"sv, StopPair);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SaveStartView"sv, SaveStartView);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenScenePicker"sv, OpenScenePicker);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenNPCScenePicker"sv, OpenNPCScenePicker);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenSceneNavigation"sv, OpenSceneNavigation);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ChangeSceneSpeed"sv, ChangeSceneSpeed);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NavigateScene"sv, NavigateScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "BeginSceneCamera"sv, BeginSceneCamera);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UseFades"sv, UseFades);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetSceneFade"sv, SetSceneFade);
		a_vm->BindNativeMethod(SCRIPT_NAME, "EndSceneCamera"sv, EndSceneCamera);
		a_vm->BindNativeMethod(SCRIPT_NAME, "LockPlayerControls"sv, LockPlayerControls);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnlockPlayerControls"sv, UnlockPlayerControls);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetPlayerSceneActorID"sv, GetPlayerSceneActorID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetPlayerSceneActorCount"sv, GetPlayerSceneActorCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IgnoreGroupCollision"sv, IgnoreGroupCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlaceGroup"sv, PlaceGroup);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlayGroupIdles"sv, PlayGroupIdles);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopGroup"sv, StopGroup);
		a_vm->BindNativeMethod(SCRIPT_NAME, "EquipObject"sv, EquipObject);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnequipObject"sv, UnequipObject);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsObjectEquipped"sv, IsObjectEquipped);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetObjectVariant"sv, SetObjectVariant);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnsetObjectVariant"sv, UnsetObjectVariant);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectTypes"sv, GetEquipObjectTypes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "HasPenis"sv, HasPenis);
		a_vm->BindNativeMethod(SCRIPT_NAME, "HeelsChanged"sv, HeelsChanged);
		a_vm->BindNativeMethod(SCRIPT_NAME, "EvaluatePackageOf"sv, EvaluatePackageOf);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActorType"sv, GetActorType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NoteFurnitureBlocked"sv, NoteFurnitureBlocked);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ApplyFurnitureChange"sv, ApplyFurnitureChangeNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ChangeFurniture"sv, ChangeFurnitureNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetFurnitureType"sv, GetFurnitureType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsFurnitureChildOf"sv, IsFurnitureChildOf);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsEventChildOf"sv, IsEventChildOf);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetUndressingSlotMask"sv, GetUndressingSlotMask);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetUndressingSlotMask"sv, SetUndressingSlotMask);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Localize"sv, LocalizeText);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneFurniture"sv, GetSceneFurniture);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneFurnitureType"sv, GetSceneFurnitureType);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenFurnitureMenu"sv, OpenFurnitureMenu);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ReloadSettings"sv, ReloadSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ExportSettings"sv, ExportSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ImportSettings"sv, ImportSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UseIniSettings"sv, UseIniSettings);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PressKey"sv, PressKey);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Mute"sv, Mute);
		a_vm->BindNativeMethod(SCRIPT_NAME, "Unmute"sv, Unmute);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IsMuted"sv, IsMuted);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SceneReleased"sv, SceneReleased);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetVoiceSetPairs"sv, GetVoiceSetPairs);
		{
			using K = StimOverrides::Kind;
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActions"sv, GetActions);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionStimulation"sv, GetStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetActionStimulation"sv, SetStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionMaxStimulation"sv, GetMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetActionMaxStimulation"sv, SetMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionDefaultStimulation"sv, GetDefaultStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetActionDefaultMaxStimulation"sv, GetDefaultMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetActionStimulation"sv, ResetStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetActionMaxStimulation"sv, ResetMaxStim<K::kAction>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventStimulation"sv, GetStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetEventStimulation"sv, SetStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventMaxStimulation"sv, GetMaxStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "SetEventMaxStimulation"sv, SetMaxStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventDefaultStimulation"sv, GetDefaultStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "GetEventDefaultMaxStimulation"sv, GetDefaultMaxStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetEventStimulation"sv, ResetStim<K::kEvent>);
			a_vm->BindNativeMethod(SCRIPT_NAME, "ResetEventMaxStimulation"sv, ResetMaxStim<K::kEvent>);
		}
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetVoiceSetName"sv, GetVoiceSetName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetVoiceSet"sv, SetVoiceSet);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetActorVoiceSet"sv, SetActorVoiceSet);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetActorVoiceSetName"sv, GetActorVoiceSetName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RememberSceneWeapon"sv, RememberSceneWeapon);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SheatheForScene"sv, SheatheForScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RemoveWeaponsAtStart"sv, RemoveWeaponsAtStart);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlayerAIDrivenInScenes"sv, PlayerAIDrivenInScenes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "TakeSceneWeaponID"sv, TakeSceneWeaponID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetEquipObjectID"sv, SetEquipObjectID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectID"sv, GetEquipObjectID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectPairs"sv, GetEquipObjectPairs);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetEquipObjectName"sv, GetEquipObjectName);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SetFormEquipObjectID"sv, SetFormEquipObjectID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RegisterForSceneEvents"sv, RegisterForSceneEvents);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnregisterForSceneEvents"sv, UnregisterForSceneEvents);
		// OStim's API scripts (docs/API.md).
		constexpr auto THREAD = "FourThread"sv;
		a_vm->BindNativeMethod(THREAD, "QuickStart"sv, Natives::FourThread::QuickStart);
		a_vm->BindNativeMethod(THREAD, "IsRunning"sv, Natives::FourThread::IsRunning);
		a_vm->BindNativeMethod(THREAD, "Stop"sv, Natives::FourThread::Stop);
		a_vm->BindNativeMethod(THREAD, "GetThreadCount"sv, Natives::FourThread::GetThreadCount);
		a_vm->BindNativeMethod(THREAD, "GetAllThreadIDs"sv, Natives::FourThread::GetAllThreadIDs);
		a_vm->BindNativeMethod(THREAD, "GetScene"sv, Natives::FourThread::GetScene);
		a_vm->BindNativeMethod(THREAD, "NavigateTo"sv, Natives::FourThread::NavigateTo);
		a_vm->BindNativeMethod(THREAD, "WarpTo"sv, Natives::FourThread::WarpTo);
		a_vm->BindNativeMethod(THREAD, "GetSpeed"sv, Natives::FourThread::GetSpeed);
		a_vm->BindNativeMethod(THREAD, "SetSpeed"sv, Natives::FourThread::SetSpeed);
		a_vm->BindNativeMethod(THREAD, "GetActors"sv, Natives::FourThread::GetActors);
		a_vm->BindNativeMethod(THREAD, "GetActor"sv, Natives::FourThread::GetActor);
		a_vm->BindNativeMethod(THREAD, "GetActorPosition"sv, Natives::FourThread::GetActorPosition);
		a_vm->BindNativeMethod(THREAD, "StallClimax"sv, Natives::FourThread::StallClimax);
		a_vm->BindNativeMethod(THREAD, "PermitClimax"sv, Natives::FourThread::PermitClimax);
		a_vm->BindNativeMethod(THREAD, "IsClimaxStalled"sv, Natives::FourThread::IsClimaxStalled);
		a_vm->BindNativeMethod(THREAD, "GetFurniture"sv, Natives::FourThread::GetFurniture);
		a_vm->BindNativeMethod(THREAD, "GetFurnitureType"sv, Natives::FourThread::GetFurnitureType);
		a_vm->BindNativeMethod(THREAD, "IsInAutoMode"sv, Natives::FourThread::IsInAutoMode);
		a_vm->BindNativeMethod(THREAD, "StartAutoMode"sv, Natives::FourThread::StartAutoMode);
		a_vm->BindNativeMethod(THREAD, "StopAutoMode"sv, Natives::FourThread::StopAutoMode);
		a_vm->BindNativeMethod(THREAD, "HasMetadata"sv, Natives::FourThread::HasMetadata);
		a_vm->BindNativeMethod(THREAD, "AddMetadata"sv, Natives::FourThread::AddMetadata);
		a_vm->BindNativeMethod(THREAD, "GetMetadata"sv, Natives::FourThread::GetMetadata);
		a_vm->BindNativeMethod(THREAD, "HasMetaFloat"sv, Natives::FourThread::HasMetaFloat);
		a_vm->BindNativeMethod(THREAD, "GetMetaFloat"sv, Natives::FourThread::GetMetaFloat);
		a_vm->BindNativeMethod(THREAD, "SetMetaFloat"sv, Natives::FourThread::SetMetaFloat);
		a_vm->BindNativeMethod(THREAD, "HasMetaString"sv, Natives::FourThread::HasMetaString);
		a_vm->BindNativeMethod(THREAD, "GetMetaString"sv, Natives::FourThread::GetMetaString);
		a_vm->BindNativeMethod(THREAD, "SetMetaString"sv, Natives::FourThread::SetMetaString);
		a_vm->BindNativeMethod(THREAD, "PlaySequence"sv, Natives::FourThread::PlaySequence);
		constexpr auto BUILDER = "FourThreadBuilder"sv;
		a_vm->BindNativeMethod(BUILDER, "Create"sv, Natives::FourThreadBuilder::Create);
		a_vm->BindNativeMethod(BUILDER, "SetDominantActors"sv, Natives::FourThreadBuilder::SetDominantActors);
		a_vm->BindNativeMethod(BUILDER, "SetFurniture"sv, Natives::FourThreadBuilder::SetFurniture);
		a_vm->BindNativeMethod(BUILDER, "SetDuration"sv, Natives::FourThreadBuilder::SetDuration);
		a_vm->BindNativeMethod(BUILDER, "SetStartingAnimation"sv, Natives::FourThreadBuilder::SetStartingAnimation);
		a_vm->BindNativeMethod(BUILDER, "AddStartingAnimation"sv, Natives::FourThreadBuilder::AddStartingAnimation);
		a_vm->BindNativeMethod(BUILDER, "SetStartingSequence"sv, Natives::FourThreadBuilder::SetStartingSequence);
		a_vm->BindNativeMethod(BUILDER, "ConcatStartingSequence"sv, Natives::FourThreadBuilder::ConcatStartingSequence);
		a_vm->BindNativeMethod(BUILDER, "EndAfterSequence"sv, Natives::FourThreadBuilder::EndAfterSequence);
		a_vm->BindNativeMethod(BUILDER, "UndressActors"sv, Natives::FourThreadBuilder::UndressActors);
		a_vm->BindNativeMethod(BUILDER, "NoAutoMode"sv, Natives::FourThreadBuilder::NoAutoMode);
		a_vm->BindNativeMethod(BUILDER, "NoPlayerControl"sv, Natives::FourThreadBuilder::NoPlayerControl);
		a_vm->BindNativeMethod(BUILDER, "NoUndressing"sv, Natives::FourThreadBuilder::NoUndressing);
		a_vm->BindNativeMethod(BUILDER, "NoPostDialogue"sv, Natives::FourThreadBuilder::NoPostDialogue);
		a_vm->BindNativeMethod(BUILDER, "NoFurniture"sv, Natives::FourThreadBuilder::NoFurniture);
		a_vm->BindNativeMethod(BUILDER, "SetMetadata"sv, Natives::FourThreadBuilder::SetMetadata);
		a_vm->BindNativeMethod(BUILDER, "SetMetadataCSV"sv, Natives::FourThreadBuilder::SetMetadataCSV);
		a_vm->BindNativeMethod(BUILDER, "Start"sv, Natives::FourThreadBuilder::Start);
		a_vm->BindNativeMethod(BUILDER, "Cancel"sv, Natives::FourThreadBuilder::Cancel);
		constexpr auto LIBRARY = "FourLibrary"sv;
		a_vm->BindNativeMethod(LIBRARY, "FindRandomScene"sv, Natives::FourLibrary::FindRandomScene);
		a_vm->BindNativeMethod(LIBRARY, "FindRandomSceneByRoleTags"sv, Natives::FourLibrary::FindRandomSceneByRoleTags);
		a_vm->BindNativeMethod(LIBRARY, "FindRandomSceneByActionRoles"sv, Natives::FourLibrary::FindRandomSceneByActionRoles);
		a_vm->BindNativeMethod(LIBRARY, "FindRandomSceneSuperload"sv, Natives::FourLibrary::FindRandomSceneSuperload);
		a_vm->BindNativeMethod(LIBRARY, "GetScenesInRange"sv, Natives::FourLibrary::GetScenesInRange);
		a_vm->BindNativeMethod(LIBRARY, "SplitCSV"sv, Natives::FourLibrary::SplitCSV);
		a_vm->BindNativeMethod(LIBRARY, "GetAllScenes"sv, Natives::FourLibrary::GetAllScenes);
		constexpr auto METADATA = "FourMetadata"sv;
		a_vm->BindNativeMethod(METADATA, "GetName"sv, Natives::FourMetadata::GetName);
		a_vm->BindNativeMethod(METADATA, "IsTransition"sv, Natives::FourMetadata::IsTransition);
		a_vm->BindNativeMethod(METADATA, "GetDestination"sv, Natives::FourMetadata::GetDestination);
		a_vm->BindNativeMethod(METADATA, "GetSceneTags"sv, Natives::FourMetadata::GetSceneTags);
		a_vm->BindNativeMethod(METADATA, "GetFurnitureType"sv, Natives::FourMetadata::GetFurnitureType);
		a_vm->BindNativeMethod(METADATA, "GetMaxSpeed"sv, Natives::FourMetadata::GetMaxSpeed);
		a_vm->BindNativeMethod(METADATA, "GetDefaultSpeed"sv, Natives::FourMetadata::GetDefaultSpeed);
		a_vm->BindNativeMethod(METADATA, "GetNavigations"sv, Natives::FourMetadata::GetNavigations);
		a_vm->BindNativeMethod(METADATA, "GetAutoTransition"sv, Natives::FourMetadata::GetAutoTransition);
		a_vm->BindNativeMethod(METADATA, "GetAutoTransitionForActor"sv, Natives::FourMetadata::GetAutoTransitionForActor);
		a_vm->BindNativeMethod(METADATA, "IsNoRandomSelection"sv, Natives::FourMetadata::IsNoRandomSelection);
		a_vm->BindNativeMethod(METADATA, "GetModpack"sv, Natives::FourMetadata::GetModpack);
		a_vm->BindNativeMethod(METADATA, "ScenesToNames"sv, Natives::FourMetadata::ScenesToNames);
		a_vm->BindNativeMethod(METADATA, "IntsOfCSV"sv, Natives::FourMetadata::IntsOfCSV);
		a_vm->BindNativeMethod(METADATA, "FloatsOfCSV"sv, Natives::FourMetadata::FloatsOfCSV);
		a_vm->BindNativeMethod(METADATA, "IntListOverlap"sv, Natives::FourMetadata::IntListOverlap);
		a_vm->BindNativeMethod(METADATA, "FloatListOverlap"sv, Natives::FourMetadata::FloatListOverlap);
		a_vm->BindNativeMethod(METADATA, "StringListOverlap"sv, Natives::FourMetadata::StringListOverlap);
		a_vm->BindNativeMethod(METADATA, "GetAnimationId"sv, Natives::FourMetadata::GetAnimationId);
		a_vm->BindNativeMethod(METADATA, "GetActorRequirements"sv, Natives::FourMetadata::GetActorRequirements);
		a_vm->BindNativeMethod(METADATA, "GetActionTypes"sv, Natives::FourMetadata::GetActionTypes);
		a_vm->BindNativeMethod(METADATA, "GetActionActors"sv, Natives::FourMetadata::GetActionActors);
		a_vm->BindNativeMethod(METADATA, "GetActionTargets"sv, Natives::FourMetadata::GetActionTargets);
		a_vm->BindNativeMethod(METADATA, "GetActionPerformers"sv, Natives::FourMetadata::GetActionPerformers);
		a_vm->BindNativeMethod(METADATA, "GetActionTags"sv, Natives::FourMetadata::GetActionTags);
		a_vm->BindNativeMethod(METADATA, "GetAllActionsTags"sv, Natives::FourMetadata::GetAllActionsTags);
		a_vm->BindNativeMethod(METADATA, "ListHasAny"sv, Natives::FourMetadata::ListHasAny);
		a_vm->BindNativeMethod(METADATA, "ListHasAll"sv, Natives::FourMetadata::ListHasAll);
		a_vm->BindNativeMethod(METADATA, "ListOverlap"sv, Natives::FourMetadata::ListOverlap);
		a_vm->BindNativeMethod(METADATA, "FindActionsBy"sv, Natives::FourMetadata::FindActionsBy);
		a_vm->BindNativeMethod(METADATA, "FindActionsSuperload"sv, Natives::FourMetadata::FindActionsSuperload);
		a_vm->BindNativeMethod(METADATA, "HasCustomAction"sv, Natives::FourMetadata::HasCustomAction);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionInt"sv, Natives::FourMetadata::GetCustomActionInt);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionFloat"sv, Natives::FourMetadata::GetCustomActionFloat);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionString"sv, Natives::FourMetadata::GetCustomActionString);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionIntList"sv, Natives::FourMetadata::GetCustomActionIntList);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionFloatList"sv, Natives::FourMetadata::GetCustomActionFloatList);
		a_vm->BindNativeMethod(METADATA, "GetCustomActionStringList"sv, Natives::FourMetadata::GetCustomActionStringList);
		a_vm->BindNativeMethod(METADATA, "GetCustomSceneActorInt"sv, Natives::FourMetadata::GetCustomSceneActorInt);
		a_vm->BindNativeMethod(METADATA, "GetCustomSceneActorFloat"sv, Natives::FourMetadata::GetCustomSceneActorFloat);
		constexpr auto ACTOR = "FourActor"sv;
		a_vm->BindNativeMethod(ACTOR, "GetThreadID"sv, Natives::FourActor::GetThreadID);
		a_vm->BindNativeMethod(ACTOR, "HasMetadata"sv, Natives::FourActor::HasMetadata);
		a_vm->BindNativeMethod(ACTOR, "AddMetadata"sv, Natives::FourActor::AddMetadata);
		a_vm->BindNativeMethod(ACTOR, "GetMetadata"sv, Natives::FourActor::GetMetadata);
		a_vm->BindNativeMethod(ACTOR, "HasMetaFloat"sv, Natives::FourActor::HasMetaFloat);
		a_vm->BindNativeMethod(ACTOR, "GetMetaFloat"sv, Natives::FourActor::GetMetaFloat);
		a_vm->BindNativeMethod(ACTOR, "SetMetaFloat"sv, Natives::FourActor::SetMetaFloat);
		a_vm->BindNativeMethod(ACTOR, "HasMetaString"sv, Natives::FourActor::HasMetaString);
		a_vm->BindNativeMethod(ACTOR, "GetMetaString"sv, Natives::FourActor::GetMetaString);
		a_vm->BindNativeMethod(ACTOR, "SetMetaString"sv, Natives::FourActor::SetMetaString);
		a_vm->BindNativeMethod(ACTOR, "PlayExpression"sv, Natives::FourActor::PlayExpression);
		a_vm->BindNativeMethod(ACTOR, "ClearExpression"sv, Natives::FourActor::ClearExpression);
		a_vm->BindNativeMethod(ACTOR, "SetExpressionsEnabled"sv, Natives::FourActor::SetExpressionsEnabled);
		a_vm->BindNativeMethod(ACTOR, "HasExpressionOverride"sv, Natives::FourActor::HasExpressionOverride);
		a_vm->BindNativeMethod(ACTOR, "UndressPartial"sv, Natives::FourActor::UndressPartial);
		a_vm->BindNativeMethod(ACTOR, "RedressPartial"sv, Natives::FourActor::RedressPartial);
		constexpr auto FURNITURE = "FourFurniture"sv;
		a_vm->BindNativeMethod(FURNITURE, "FindFurniture"sv, Natives::FourFurniture::FindFurniture);
		a_vm->BindNativeMethod(FURNITURE, "FindFurnitureOfType"sv, Natives::FourFurniture::FindFurnitureOfType);
		a_vm->BindNativeMethod(FURNITURE, "GetOffset"sv, Natives::FourFurniture::GetOffset);
		a_vm->BindNativeMethod(FURNITURE, "GetSceneID"sv, Natives::FourFurniture::GetSceneID);
		constexpr auto ACTORUTIL = "FourActorUtil"sv;
		a_vm->BindNativeMethod(ACTORUTIL, "FulfillsCondition"sv, Natives::FourActorUtil::FulfillsCondition);
		a_vm->BindNativeMethod(ACTORUTIL, "FulfillsAnyCondition"sv, Natives::FourActorUtil::FulfillsAnyCondition);
		a_vm->BindNativeMethod(ACTORUTIL, "FulfillsAllConditions"sv, Natives::FourActorUtil::FulfillsAllConditions);
		a_vm->BindNativeMethod(ACTORUTIL, "EmptyArray"sv, Natives::FourActorUtil::EmptyArray);
		a_vm->BindNativeMethod(ACTORUTIL, "CreateArray"sv, Natives::FourActorUtil::CreateArray);
		a_vm->BindNativeMethod(ACTORUTIL, "ToArray"sv, Natives::FourActorUtil::ToArray);
		a_vm->BindNativeMethod(ACTORUTIL, "Sort"sv, Natives::FourActorUtil::Sort);
		a_vm->BindNativeMethod(ACTORUTIL, "GetActorsInRangeV2"sv, Natives::FourActorUtil::GetActorsInRangeV2);
		a_vm->BindNativeMethod(ACTORUTIL, "ActorsToNames"sv, Natives::FourActorUtil::ActorsToNames);
		a_vm->BindNativeMethod("FourSequence"sv, "RandomSequence"sv, SQ_Random);
		constexpr auto MOD_EVENTS = "FourModEvents"sv;
		a_vm->BindNativeMethod(MOD_EVENTS, "RegisterForModEvent"sv, Natives::FourModEvents::Register);
		a_vm->BindNativeMethod(MOD_EVENTS, "UnregisterForModEvent"sv, Natives::FourModEvents::Unregister);
		a_vm->BindNativeMethod(MOD_EVENTS, "UnregisterForAllModEvents"sv, Natives::FourModEvents::UnregisterAll);
		a_vm->BindNativeMethod(MOD_EVENTS, "SendModEvent"sv, Natives::FourModEvents::Send);
		constexpr auto JSON = "FourJSON"sv;
		a_vm->BindNativeMethod(JSON, "GetActors"sv, Natives::FourJSON::GetActors);
		a_vm->BindNativeMethod(JSON, "GetScene"sv, Natives::FourJSON::GetScene);
		a_vm->BindNativeMethod(JSON, "GetMetadata"sv, Natives::FourJSON::GetMetadata);
		REX::INFO("Papyrus functions bound");
		return true;
	}
}
