#include "Core/Core.h"

namespace Core
{
	// A setting changed in the MCM: 4Stim.ini and the MCM's file read again.
	void ReloadSettings(std::monostate)
	{
		// A moment later, so the MCM has written its file first.
		Scheduler::AfterOnMain(std::chrono::milliseconds(250), []() {
			LoadSettings();
			HUD::LoadConfig();  // the theme and control scheme
			REX::INFO("Settings: read again (the MCM changed one)");
		});
	}

	// The MCM's buttons: export to / import from Documents\My Games\Fallout4\4Stim\mcmsettings.json
	// (OStim's), and back to 4Stim.ini's values.
	bool ExportSettings(std::monostate)
	{
		const bool ok = MCM::Export();
		Notify(ok ? TR("$4stim_settings_exported", "4Stim: settings exported") : TR("$4stim_settings_not_exported", "4Stim: couldn't export the settings"));
		return ok;
	}

	bool ImportSettings(std::monostate)
	{
		const bool ok = MCM::Import();
		if (ok) {
			ReloadSettings({});
		}
		Notify(ok ? TR("$4stim_settings_imported", "4Stim: settings imported") : TR("$4stim_settings_not_imported", "4Stim: no exported settings to import"));
		return ok;
	}

	void UseIniSettings(std::monostate)
	{
		MCM::UseIni();
		ReloadSettings({});
		Notify(TR("$4stim_settings_from_ini", "4Stim: back to 4Stim.ini's settings"));
	}

	// A scene key's action by name (the MCM's hotkeys): hotkey, npcscene,
	// speedup, speeddown, automode, pullout, end, search, align, freecam, hideui.
	void PressKey(std::monostate, std::string a_action)
	{
		std::ranges::transform(a_action, a_action.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		F4SE::GetTaskInterface()->AddTask([action = std::move(a_action)]() { RunKeyAction(action); });
	}

	// The "Change furniture" list for the focused scene (the HUD's Utility
	// entry does the same).
	void OpenFurnitureMenu(std::monostate)
	{
		FourStim::OpenFurnitureForFocused();
	}

	// OStim's OFurniture.GetFurnitureType ("" = none).
	std::string GetFurnitureType(std::monostate, RE::TESObjectREFR* a_furniture)
	{
		return a_furniture ? Furniture::Classify(a_furniture) : std::string{};
	}

	// OStim's OFurniture.IsChildOf: furniture of a_subType takes scenes for a_superType.
	bool IsFurnitureChildOf(std::monostate, std::string a_superType, std::string a_subType)
	{
		return !a_superType.empty() && !a_subType.empty() && Furniture::IsA(a_subType, a_superType);
	}

	// OStim's OEvent.IsChildOf: a_subType is a_superType or has it among
	// its supertypes.
	bool IsEventChildOf(std::monostate, std::string a_superType, std::string a_subType)
	{
		return !a_superType.empty() && !a_subType.empty() && Events::IsChildOf(a_subType, a_superType);
	}

	// OStim's OData.GetUndressingSlotMask / SetUndressingSlotMask: the slots
	// undressed, bit 0 = slot 30 (Fallout 4's slots). Set until the settings
	// are read again (4Stim.ini and the MCM's are what last: OStim's is in
	// the save).
	std::int32_t GetUndressingSlotMask(std::monostate)
	{
		std::uint32_t mask = 0;
		for (const auto slot : Undress::Settings().slots) {
			if (slot >= 30 && slot <= 61) {
				mask |= 1u << (slot - 30);
			}
		}
		return static_cast<std::int32_t>(mask);
	}

	void SetUndressingSlotMask(std::monostate, std::int32_t a_mask)
	{
		const auto       mask = static_cast<std::uint32_t>(a_mask);
		std::vector<int> slots;
		for (int slot = 30; slot <= 61; ++slot) {
			if (mask & (1u << (slot - 30))) {
				slots.push_back(slot);
			}
		}
		F4SE::GetTaskInterface()->AddTask([slots = std::move(slots)]() mutable { Undress::Settings().slots = std::move(slots); });
	}

	// OStim's OData.Localize / ToLocalizedString: a "$key" text translated
	// (4Stim's translation files, OStim's own keys among them).
	std::string LocalizeText(std::monostate, std::string a_text)
	{
		return Translations::Localize(a_text);
	}

	// OStim's OThread.GetFurniture: the furniture a_actor's scene is on (None
	// off furniture, or at a wall, which is no object).
	RE::TESObjectREFR* GetSceneFurniture(std::monostate, RE::Actor* a_actor)
	{
		// Read from a Papyrus thread, as IsInScene.
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		const auto ref = active ? active->furnitureRef : 0;
		return ref ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(ref) : nullptr;
	}

	// The type of the furniture a_actor's scene is on ("" = none).
	std::string GetSceneFurnitureType(std::monostate, RE::Actor* a_actor)
	{
		const auto active = a_actor ? FindActiveScene(a_actor->GetFormID()) : nullptr;
		return active ? active->furnitureType : std::string{};
	}

	// Any script object: a form, a quest alias, an active magic effect.
	bool RegisterForSceneEvents(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver)
	{
		return SceneEvents::Register(a_receiver ? static_cast<std::uint64_t>(a_receiver->GetHandle()) : 0);
	}

	void UnregisterForSceneEvents(std::monostate, RE::BSTSmartPointer<RE::BSScript::Object> a_receiver)
	{
		SceneEvents::Unregister(a_receiver ? static_cast<std::uint64_t>(a_receiver->GetHandle()) : 0);
	}
}
