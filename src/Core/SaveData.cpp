#include "Core/Core.h"

namespace Core
{
	constexpr std::uint32_t SCENES_RECORD = 'SCNS';
	constexpr std::uint32_t CHOICES_RECORD = 'EQCH';  // each actor's chosen equip objects (OStim saves them in its actor data)
	constexpr std::uint32_t CHOICES_VERSION = 1;
	constexpr std::uint32_t VOICES_RECORD = 'VOIC';  // voice sets chosen per actor base (OStim's Serialization voice sets)
	constexpr std::uint32_t VOICES_VERSION = 1;
	constexpr std::uint32_t STIM_RECORD = 'STIM';  // per actor base stimulation overrides (OStim's Serialization action / event stimulation)
	constexpr std::uint32_t STIM_VERSION = 1;
	constexpr std::uint32_t SCENES_VERSION = 6;  // 2: each actor's own scale (alignment scales them); 3: equip objects given; 4: wall spot, furniture blocked, clutter; 5: weapon taken off; 6: erect bodysuit given

	struct SavedScene
	{
		std::vector<std::uint32_t>              actors;  // role order
		std::string                             sceneID;
		std::int32_t                            speed = 0;
		std::uint32_t                           furnitureRef = 0;
		std::string                             furnitureType;
		bool                                    autoMode = false;
		std::vector<float>                      excitement;  // per actor
		std::vector<std::vector<std::uint32_t>> stripped;    // per actor
		std::vector<float>                      baseScale;   // per actor: their own scale (0 = unknown, version 1)
		std::vector<std::vector<std::uint32_t>> given;       // per actor: equip objects 4Stim gave them (version 3)
		std::optional<Furniture::Spot>          furnitureFixedSpot;  // a wall (version 4)
		bool                                    furnitureBlocked = false;  // 4Stim blocked the furniture (version 4)
		std::vector<Furniture::ClutterItem>     clutter;             // version 4
		std::vector<std::uint32_t>              weapons;             // per actor: the weapon taken off, 0 = none (version 5)
		std::vector<std::uint32_t>              suits;               // per actor: the erect bodysuit 4Stim gave, 0 = none (version 6)
	};
	std::mutex              g_savedLock;
	std::vector<SavedScene> g_loadedScenes;  // read from the save being loaded

	void WriteString(const F4SE::SerializationInterface* a_intfc, const std::string& a_text)
	{
		const auto length = static_cast<std::uint32_t>(a_text.size());
		a_intfc->WriteRecordData(length);
		a_intfc->WriteRecordData(a_text.data(), length);
	}

	bool ReadString(const F4SE::SerializationInterface* a_intfc, std::string& a_out)
	{
		std::uint32_t length = 0;
		if (a_intfc->ReadRecordData(length) != sizeof(length) || length > 4096) {
			return false;
		}
		a_out.resize(length);
		return a_intfc->ReadRecordData(a_out.data(), length) == length;
	}

	void WriteVoices(const F4SE::SerializationInterface* a_intfc);

	void WriteStimOverrides(const F4SE::SerializationInterface* a_intfc)
	{
		const auto entries = StimOverrides::All();
		if (entries.empty() || !a_intfc->OpenRecord(STIM_RECORD, STIM_VERSION)) {
			return;
		}
		a_intfc->WriteRecordData(static_cast<std::uint32_t>(entries.size()));
		for (const auto& e : entries) {
			a_intfc->WriteRecordData(e.kind);
			a_intfc->WriteRecordData(e.role);
			a_intfc->WriteRecordData(e.max);
			a_intfc->WriteRecordData(e.base);
			WriteString(a_intfc, e.id);
			a_intfc->WriteRecordData(e.value);
		}
	}

	void ReadStimOverrides(const F4SE::SerializationInterface* a_intfc)
	{
		std::uint32_t count = 0;
		a_intfc->ReadRecordData(count);
		std::vector<StimOverrides::Entry> entries;
		for (std::uint32_t i = 0; i < count && i < 100000; ++i) {
			StimOverrides::Entry e;
			if (!a_intfc->ReadRecordData(e.kind) || !a_intfc->ReadRecordData(e.role) || !a_intfc->ReadRecordData(e.max) ||
				!a_intfc->ReadRecordData(e.base) || !ReadString(a_intfc, e.id) || !a_intfc->ReadRecordData(e.value)) {
				break;
			}
			e.base = a_intfc->ResolveFormID(e.base).value_or(0);  // its plugin gone: dropped
			if (e.base) {
				entries.push_back(std::move(e));
			}
		}
		StimOverrides::Restore(entries);
		REX::INFO("Save data: {} stimulation override(s)", entries.size());
	}

	void OnGameSaved(const F4SE::SerializationInterface* a_intfc)
	{
		if (!a_intfc->OpenRecord(SCENES_RECORD, SCENES_VERSION)) {
			REX::WARN("Save data: couldn't write the scenes record");
			return;
		}
		const auto count = static_cast<std::uint32_t>(g_activeScenes.size());
		a_intfc->WriteRecordData(count);
		for (const auto& active : g_activeScenes) {
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(active.actors.size()));
			for (std::size_t role = 0; role < active.actors.size(); ++role) {
				const auto id = active.actors[role];
				a_intfc->WriteRecordData(id);
				a_intfc->WriteRecordData(role < active.baseScale.size() ? active.baseScale[role] : 1.0F);
				a_intfc->WriteRecordData(std::max(Excitement::Get(id), 0.0F));
				const auto items = Undress::Stripped(id);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(items.size()));
				for (const auto item : items) {
					a_intfc->WriteRecordData(item);
				}
				const auto given = EquipObjects::Given(id);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(given.size()));
				for (const auto item : given) {
					a_intfc->WriteRecordData(item);
				}
				a_intfc->WriteRecordData(SceneWeaponOf(id));  // version 5
				a_intfc->WriteRecordData(Erection::Given(id));  // version 6
			}
			WriteString(a_intfc, active.sceneID);
			a_intfc->WriteRecordData(static_cast<std::int32_t>(active.speed));
			a_intfc->WriteRecordData(active.furnitureRef);
			WriteString(a_intfc, active.furnitureType);
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(active.autoMode.on ? 1 : 0));
			// Version 4.
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(active.furnitureFixedSpot ? 1 : 0));
			if (active.furnitureFixedSpot) {
				const auto& spot = *active.furnitureFixedSpot;
				a_intfc->WriteRecordData(spot.position.x);
				a_intfc->WriteRecordData(spot.position.y);
				a_intfc->WriteRecordData(spot.position.z);
				a_intfc->WriteRecordData(spot.heading);
			}
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(FurnitureBlocked(active.furnitureRef) ? 1 : 0));
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(active.clutter.size()));
			for (const auto& item : active.clutter) {
				a_intfc->WriteRecordData(item.ref);
				a_intfc->WriteRecordData(item.position.x);
				a_intfc->WriteRecordData(item.position.y);
				a_intfc->WriteRecordData(item.position.z);
				a_intfc->WriteRecordData(item.angle.x);
				a_intfc->WriteRecordData(item.angle.y);
				a_intfc->WriteRecordData(item.angle.z);
			}
		}
		if (count > 0) {
			REX::INFO("Save data: {} running scene(s) saved", count);
		}
		// The equip objects chosen for actors (the HUD's options,
		// FourActor.SetEquipObjectID).
		const auto chosen = EquipObjects::AllChosen();
		if (!chosen.empty() && a_intfc->OpenRecord(CHOICES_RECORD, CHOICES_VERSION)) {
			a_intfc->WriteRecordData(static_cast<std::uint32_t>(chosen.size()));
			for (const auto& [id, types] : chosen) {
				a_intfc->WriteRecordData(id);
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(types.size()));
				for (const auto& [type, object] : types) {
					WriteString(a_intfc, type);
					WriteString(a_intfc, object);
				}
			}
		}
		WriteVoices(a_intfc);
		WriteStimOverrides(a_intfc);
	}

	// The VOICES_RECORD: actor base -> voice set key. A key that's a form ID
	// (OStim's: the voice set's target) is written as one, so a changed load
	// order still finds it; a name (4Stim's sets without a target) as text.
	void WriteVoices(const F4SE::SerializationInterface* a_intfc)
	{
		const auto voices = Sounds::AllVoiceChoices();
		if (voices.empty() || !a_intfc->OpenRecord(VOICES_RECORD, VOICES_VERSION)) {
			return;
		}
		a_intfc->WriteRecordData(static_cast<std::uint32_t>(voices.size()));
		for (const auto& [base, key] : voices) {
			a_intfc->WriteRecordData(base);
			std::uint32_t form = 0;
			bool          isForm = !key.empty() && std::ranges::all_of(key, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == '-'; });
			if (isForm) {
				try {
					form = static_cast<std::uint32_t>(std::stol(key));
				} catch (...) {
					isForm = false;
				}
			}
			a_intfc->WriteRecordData(static_cast<std::uint8_t>(isForm ? 1 : 0));
			if (isForm) {
				a_intfc->WriteRecordData(form);
			} else {
				WriteString(a_intfc, key);
			}
		}
	}

	void ReadVoices(const F4SE::SerializationInterface* a_intfc)
	{
		std::vector<std::pair<std::uint32_t, std::string>> voices;
		std::uint32_t count = 0;
		a_intfc->ReadRecordData(count);
		for (std::uint32_t i = 0; i < count && i < 65536; ++i) {
			std::uint32_t base = 0;
			std::uint8_t  isForm = 0;
			a_intfc->ReadRecordData(base);
			a_intfc->ReadRecordData(isForm);
			std::string key;
			if (isForm) {
				std::uint32_t form = 0;
				a_intfc->ReadRecordData(form);
				const auto resolved = a_intfc->ResolveFormID(form).value_or(0);
				key = resolved ? std::to_string(static_cast<std::int32_t>(resolved)) : std::string{};
			} else if (!ReadString(a_intfc, key)) {
				REX::WARN("Save data: the voice set choices are damaged, the rest skipped");
				break;
			}
			const auto resolvedBase = a_intfc->ResolveFormID(base).value_or(0);
			if (resolvedBase && !key.empty()) {
				voices.emplace_back(resolvedBase, std::move(key));
			}
		}
		REX::INFO("Save data: voice sets chosen for {} actor(s)", voices.size());
		Sounds::RestoreVoiceChoices(std::move(voices));
	}

	// The CHOICES_RECORD: actor -> type -> object id.
	void ReadChoices(const F4SE::SerializationInterface* a_intfc)
	{
		std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> chosen;
		std::uint32_t actors = 0;
		a_intfc->ReadRecordData(actors);
		for (std::uint32_t a = 0; a < actors && a < 65536; ++a) {
			std::uint32_t id = 0, types = 0;
			a_intfc->ReadRecordData(id);
			a_intfc->ReadRecordData(types);
			// OStim's per-sex pseudo IDs (0x0 men, 0x1 women) aren't forms.
			const auto resolved = id <= EquipObjects::FEMALE ? std::optional<std::uint32_t>(id) : a_intfc->ResolveFormID(id);
			for (std::uint32_t t = 0; t < types && t < 256; ++t) {
				std::string type, object;
				if (!ReadString(a_intfc, type) || !ReadString(a_intfc, object)) {
					REX::WARN("Save data: the equip object choices are damaged, the rest skipped");
					EquipObjects::RestoreChosen(std::move(chosen));
					return;
				}
				if (resolved) {
					chosen[*resolved][type] = object;
				}
			}
		}
		REX::INFO("Save data: equip objects chosen for {} actor(s)", chosen.size());
		EquipObjects::RestoreChosen(std::move(chosen));
	}

	void OnGameLoadedData(const F4SE::SerializationInterface* a_intfc)
	{
		std::vector<SavedScene> scenes;
		std::uint32_t type = 0, version = 0, length = 0;
		auto resolve = [&](std::uint32_t a_id) { return a_intfc->ResolveFormID(a_id).value_or(0); };
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type == CHOICES_RECORD && version == CHOICES_VERSION) {
				ReadChoices(a_intfc);
				continue;
			}
			if (type == VOICES_RECORD && version == VOICES_VERSION) {
				ReadVoices(a_intfc);
				continue;
			}
			if (type == STIM_RECORD && version == STIM_VERSION) {
				ReadStimOverrides(a_intfc);
				continue;
			}
			if (type != SCENES_RECORD || version < 1 || version > SCENES_VERSION) {
				continue;
			}
			std::uint32_t count = 0;
			a_intfc->ReadRecordData(count);
			for (std::uint32_t i = 0; i < count && i < 64; ++i) {
				SavedScene saved;
				std::uint32_t actors = 0;
				a_intfc->ReadRecordData(actors);
				bool ok = actors > 0 && actors <= 16;
				for (std::uint32_t a = 0; ok && a < actors; ++a) {
					std::uint32_t id = 0, items = 0;
					float         excitement = 0.0F, baseScale = 0.0F;
					a_intfc->ReadRecordData(id);
					if (version >= 2) {
						a_intfc->ReadRecordData(baseScale);
					}
					a_intfc->ReadRecordData(excitement);
					a_intfc->ReadRecordData(items);
					std::vector<std::uint32_t> stripped;
					for (std::uint32_t n = 0; n < items && n < 64; ++n) {
						std::uint32_t item = 0;
						a_intfc->ReadRecordData(item);
						if (const auto resolved = resolve(item)) {
							stripped.push_back(resolved);
						}
					}
					std::vector<std::uint32_t> given;
					if (version >= 3) {
						std::uint32_t givenCount = 0;
						a_intfc->ReadRecordData(givenCount);
						for (std::uint32_t n = 0; n < givenCount && n < 64; ++n) {
							std::uint32_t item = 0;
							a_intfc->ReadRecordData(item);
							if (const auto resolved = resolve(item)) {
								given.push_back(resolved);
							}
						}
					}
					std::uint32_t weapon = 0;
					if (version >= 5) {
						a_intfc->ReadRecordData(weapon);
						weapon = weapon ? resolve(weapon) : 0;
					}
					saved.weapons.push_back(weapon);
					std::uint32_t suit = 0;
					if (version >= 6) {
						a_intfc->ReadRecordData(suit);
						suit = suit ? resolve(suit) : 0;
					}
					saved.suits.push_back(suit);
					saved.given.push_back(std::move(given));
					saved.actors.push_back(resolve(id));  // 0 if its plugin is gone
					saved.excitement.push_back(excitement);
					saved.stripped.push_back(std::move(stripped));
					saved.baseScale.push_back(baseScale);
				}
				std::int32_t speed = 0;
				std::uint32_t furniture = 0;
				std::uint8_t  autoMode = 0;
				ok = ok && ReadString(a_intfc, saved.sceneID);
				a_intfc->ReadRecordData(speed);
				a_intfc->ReadRecordData(furniture);
				ok = ok && ReadString(a_intfc, saved.furnitureType);
				a_intfc->ReadRecordData(autoMode);
				if (ok && version >= 4) {
					std::uint8_t fixed = 0, blocked = 0;
					a_intfc->ReadRecordData(fixed);
					if (fixed) {
						Furniture::Spot spot;
						a_intfc->ReadRecordData(spot.position.x);
						a_intfc->ReadRecordData(spot.position.y);
						a_intfc->ReadRecordData(spot.position.z);
						a_intfc->ReadRecordData(spot.heading);
						saved.furnitureFixedSpot = spot;
					}
					a_intfc->ReadRecordData(blocked);
					saved.furnitureBlocked = blocked != 0;
					std::uint32_t items = 0;
					a_intfc->ReadRecordData(items);
					ok = items <= 4096;
					for (std::uint32_t n = 0; ok && n < items; ++n) {
						Furniture::ClutterItem item;
						a_intfc->ReadRecordData(item.ref);
						a_intfc->ReadRecordData(item.position.x);
						a_intfc->ReadRecordData(item.position.y);
						a_intfc->ReadRecordData(item.position.z);
						a_intfc->ReadRecordData(item.angle.x);
						a_intfc->ReadRecordData(item.angle.y);
						a_intfc->ReadRecordData(item.angle.z);
						if ((item.ref = resolve(item.ref)) != 0) {
							saved.clutter.push_back(item);
						}
					}
				}
				if (!ok) {
					REX::WARN("Save data: the scenes record is damaged, scenes from it skipped");
					break;
				}
				saved.speed = speed;
				saved.furnitureRef = furniture ? resolve(furniture) : 0;
				saved.autoMode = autoMode != 0;
				scenes.push_back(std::move(saved));
			}
		}
		if (!scenes.empty()) {
			REX::INFO("Save data: {} scene(s) were running in this save", scenes.size());
		}
		std::scoped_lock lock(g_savedLock);
		g_loadedScenes = std::move(scenes);
	}

	void OnGameReverted(const F4SE::SerializationInterface*)
	{
		EquipObjects::ClearChosen();  // before a save's are read, or a new game
		Sounds::ClearVoiceChoices();
		StimOverrides::Clear();
		g_positionBefore.clear();  // spots from the game being left (a furniture scene's player spot among them)
		std::scoped_lock lock(g_savedLock);
		g_loadedScenes.clear();
	}

	// Lets everyone in a saved scene go: unrestrained, interaction back, and
	// what was taken off back on. Main thread.
	void ReleaseSavedScene(const SavedScene& a_saved, const char* a_why)
	{
		REX::INFO("Save data: \"{}\" not started again ({}): letting its actors go", a_saved.sceneID, a_why);
		// Its furniture free again, its clutter back.
		if (a_saved.furnitureBlocked && a_saved.furnitureRef) {
			{
				std::scoped_lock lock(g_blockedFurnitureLock);
				g_blockedFurniture.insert(a_saved.furnitureRef);
			}
			UnblockFurniture(a_saved.furnitureRef);
		}
		if (g_settings.resetClutter && !a_saved.clutter.empty()) {
			Furniture::RestoreClutter(a_saved.clutter);
		}
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			const auto id = a_saved.actors[i];
			const auto actor = id ? RE::TESForm::GetFormByID<RE::Actor>(id) : nullptr;
			if (!actor) {
				continue;
			}
			if (a_saved.baseScale[i] > 0.0F) {
				SetActorScale(actor, a_saved.baseScale[i]);  // undo the alignment's size
			}
			EquipObjects::TakeBack(id, a_saved.given[i]);  // before dressing again
			if (!a_saved.stripped[i].empty()) {
				Undress::NoteStripped(id, a_saved.stripped[i]);
				Undress::Redress(id, true);
			}
			if (i < a_saved.weapons.size() && a_saved.weapons[i] && g_vm) {
				NoteSceneWeapon(id, a_saved.weapons[i]);
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "RearmByID"sv, callback, static_cast<std::int32_t>(id));
			}
			if (g_vm) {
				// The player too (OStim's freeActor(byGameLoad) unlocks
				// everyone); their controls, camera and AI driving are
				// ResetPlayerSceneState's.
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, "ReleaseAfterLoad"sv, callback, static_cast<std::int32_t>(id));
			}
		}
	}

	// Starts a saved scene again (or lets its actors go). Main thread.
	void ResumeSavedScene(const SavedScene& a_saved)
	{
		// Erections from before the save go first (LooksMenu keeps morphs in
		// its own save data, and a bodysuit 4Stim gave is still in the
		// inventory); the scene, if it starts again, puts them back.
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			if (a_saved.actors[i] && RE::TESForm::GetFormByID<RE::Actor>(a_saved.actors[i])) {
				Erection::AfterLoad(a_saved.actors[i], i < a_saved.suits.size() ? a_saved.suits[i] : 0);
			}
		}
		const auto scene = SceneRegistry::Find(a_saved.sceneID);
		bool       everyone = true;
		bool       free = true;
		for (const auto id : a_saved.actors) {
			const auto actor = id ? RE::TESForm::GetFormByID<RE::Actor>(id) : nullptr;
			everyone = everyone && actor && !actor->IsDead(false) && actor->Get3D();
			free = free && !FindActiveScene(id);
		}
		if (!g_settings.resumeScenes || !scene || !everyone || !free || !g_vm) {
			ReleaseSavedScene(a_saved, !g_settings.resumeScenes ? "bResumeScenes=0" : !scene ? "scene no longer loaded" :
			                           !everyone ? "an actor is missing" : !free ? "an actor is in another scene" : "no script engine");
			return;
		}
		auto sorted = a_saved.actors;
		std::ranges::sort(sorted);
		for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
			if (!a_saved.stripped[i].empty()) {
				Undress::NoteStripped(a_saved.actors[i], a_saved.stripped[i]);
			}
			if (i < a_saved.weapons.size() && a_saved.weapons[i]) {
				NoteSceneWeapon(a_saved.actors[i], a_saved.weapons[i]);  // back on when the scene ends
			}
			EquipObjects::TakeBack(a_saved.actors[i], a_saved.given[i]);  // the scene puts them on again
			// Their own size back first: the scene measures it again when it starts.
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_saved.actors[i]);
			if (actor && a_saved.baseScale[i] > 0.0F) {
				SetActorScale(actor, a_saved.baseScale[i]);
			}
		}
		{
			std::scoped_lock lock(g_pendingResumeLock);
			PendingResume resume{ sorted, a_saved.speed, a_saved.autoMode, {} };
			for (std::size_t i = 0; i < a_saved.actors.size(); ++i) {
				resume.excitement.emplace_back(a_saved.actors[i], a_saved.excitement[i]);
			}
			g_pendingResume.Put(sorted, std::move(resume));
		}
		if (a_saved.furnitureRef || a_saved.furnitureFixedSpot) {
			std::scoped_lock lock(g_pendingFurnitureLock);
			PendingFurniture pending{ a_saved.furnitureRef, a_saved.furnitureType, sorted };
			pending.offset = scene->furnitureOffset;
			pending.floorScene = scene->furniture.empty();
			pending.fixedSpot = a_saved.furnitureFixedSpot;
			g_pendingFurniture.Put(sorted, std::move(pending));
		}
		// Still blocked from before the save: 4Stim's to unblock when the
		// scene leaves it (FourStimFurniture.Block leaves blocked furniture be).
		if (a_saved.furnitureBlocked && a_saved.furnitureRef) {
			std::scoped_lock lock(g_blockedFurnitureLock);
			g_blockedFurniture.insert(a_saved.furnitureRef);
		}
		// The clutter as it lay before the scene, not as the scene left it.
		if (!a_saved.clutter.empty()) {
			std::scoped_lock lock(g_pendingClutterLock);
			g_pendingClutter.Put(sorted, PendingClutter{ sorted, a_saved.clutter, true });
		}
		const bool   withPlayer = std::ranges::any_of(a_saved.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		std::vector<std::int32_t> ids(a_saved.actors.begin(), a_saved.actors.end());
		const auto   anchor = withPlayer ? 0 : ids.front();
		REX::INFO("Save data: starting \"{}\" again ({} actor(s))", a_saved.sceneID, ids.size());
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "StartPickedCast"sv, callback, a_saved.sceneID, ids, anchor);
	}

	// After a save is loaded: once the loading screen and fade are gone, the
	// saved scenes one at a time (each takes the furniture slot and a few
	// seconds of Papyrus to set up).
	void ResumeSavedScenes()
	{
		std::vector<SavedScene> scenes;
		{
			std::scoped_lock lock(g_savedLock);
			scenes = std::move(g_loadedScenes);
			g_loadedScenes.clear();
		}
		if (scenes.empty()) {
			return;
		}
		// A second, then until the loading screen and fades are gone (checked
		// every 100 ms, a minute at most), 1.5 s more, then one scene every 4 s.
		Scheduler::After(std::chrono::seconds(1), [scenes = std::move(scenes)]() {
			Scheduler::WhileThenOnMain(std::chrono::milliseconds(100), 600, []() { return LoadingOrFading(); }, std::chrono::milliseconds(1500), [scenes]() {
				for (std::size_t i = 0; i < scenes.size(); ++i) {
					Scheduler::AfterOnMain(std::chrono::seconds(4) * static_cast<int>(i), [saved = scenes[i]]() { ResumeSavedScene(saved); });
				}
			});
		});
	}
}
