#include "Core/Core.h"

namespace Core
{
	// ---- Animation events of scene actors ----
	// A transition idle played with the one-shot dyn_Activation event sends
	// "IdleStop" when its clip ends (and the actor starts blending back to
	// its base pose). If the timer hasn't moved the scene on by then, this
	// does, at once. With bLogAnimEvents=1, every event a scene actor gets
	// in the 5 s after a scene change is logged too (for animation authors).
	// Hooks ProcessEvent of the BSTEventSink<BSAnimationGraphEvent> base
	// (vtable 3, at +0x38) of Actor and PlayerCharacter.

	struct AnimWatch
	{
		std::string                           sceneID;
		std::chrono::steady_clock::time_point start;
	};
	std::mutex                                    g_animWatchLock;
	std::unordered_map<std::uint32_t, AnimWatch>  g_animWatch;

	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		std::scoped_lock lock(g_animWatchLock);
		const auto now = std::chrono::steady_clock::now();
		for (const auto id : a_actors) {
			g_animWatch[id] = { a_sceneID, now };
		}
	}

	struct AnimEventHook
	{
		using func_t = RE::BSEventNotifyControl (*)(RE::BSTEventSink<RE::BSAnimationGraphEvent>*, const RE::BSAnimationGraphEvent&, RE::BSTEventSource<RE::BSAnimationGraphEvent>*);
		static inline func_t originalActor = nullptr;
		static inline func_t originalPlayer = nullptr;

		static void Log(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event)
		{
			const auto ref = reinterpret_cast<RE::TESObjectREFR*>(reinterpret_cast<std::uintptr_t>(a_this) - 0x38);
			const char* tag = a_event.tag.c_str();
			if (tag && _strnicmp(tag, "4Stim", 5) == 0) {
				AnimCues::EngineForwarded(tag);  // the game sent one itself: 4Stim's own cue timing stands down
			}
			Handle(ref->GetFormID(), tag ? tag : "", a_event.payload.c_str() ? a_event.payload.c_str() : "", true);
		}

		// An animation event (a_fromEngine) or an annotation cue 4Stim read
		// from the animation file and timed itself (AnimCues).
		static void Handle(std::uint32_t id, const std::string& a_tag, const std::string& a_payload, bool a_fromEngine)
		{
			// 4Stim's annotations count for as long as the actor is in a scene
			// (checked on the main thread); the rest only shortly after a
			// scene change.
			const char* tag = a_tag.c_str();
			const bool  ours = tag && _strnicmp(tag, "4Stim", 5) == 0;
			std::string sceneID;
			float       seconds = 0.0F;
			{
				std::scoped_lock lock(g_animWatchLock);
				const auto it = g_animWatch.find(id);
				if (it == g_animWatch.end()) {
					seconds = 1000.0F;
				} else {
					seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - it->second.start).count();
					if (seconds > 60.0F) {  // long enough for any transition
						g_animWatch.erase(it);
					} else {
						sceneID = it->second.sceneID;
					}
				}
			}
			if (sceneID.empty() && !ours) {
				return;
			}
			if (g_settings.logAnimEvents && (seconds <= 5.0F || !a_fromEngine)) {
				REX::INFO("AnimEvent: {:08X} +{:.2f}s in \"{}\": \"{}\" ({}){}", id, seconds, sceneID,
					a_tag.c_str(), a_payload.c_str(), a_fromEngine ? "" : ", timed by 4Stim from the animation file");
			}
			if (_stricmp(a_tag.c_str(), "IdleStop") == 0) {
				// Animation threads: hand it to the main thread.
				F4SE::GetTaskInterface()->AddTask([id, sceneID]() {
					const auto active = FindActiveScene(id);
					if (!active || active->remaining < 0.0F || _stricmp(active->sceneID.c_str(), sceneID.c_str()) != 0) {
						return;
					}
					const auto scene = SceneRegistry::Find(active->sceneID);
					if (!scene || !scene->IsTransition()) {
						return;
					}
					REX::INFO("Transition \"{}\": clip ended ({:.2f}s before the timer), moving on now", scene->id, active->remaining);
					active->remaining = -1.0F;
					AdvanceAutoplay(*active);
				});
			} else if (_stricmp(a_tag.c_str(), "4StimClimax") == 0) {
				// The climax annotation (OStim's OStimClimax): this actor
				// climaxes now, as OStim's climax() does when its annotation
				// comes, whether or not they were waiting for it.
				F4SE::GetTaskInterface()->AddTask([id]() {
					if (const auto active = FindActiveScene(id)) {
						REX::INFO("Climax: {:08X} 4StimClimax annotation", id);
						ClimaxNow(*active, id);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimEvent") == 0) {
				// OStim's OStimEvent: "4StimEvent.<event>,<target>" (this actor
				// did it to role <target>) or "4StimEvent.<event>,<actor>,
				// <target>,<performer>" (roles).
				std::string payload = a_payload.c_str();
				F4SE::GetTaskInterface()->AddTask([id, payload]() {
					const auto active = FindActiveScene(id);
					if (!active) {
						return;
					}
					std::vector<std::string> parts;
					for (std::size_t start = 0;;) {
						const auto comma = payload.find(',', start);
						parts.push_back(payload.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
						if (comma == std::string::npos) {
							break;
						}
						start = comma + 1;
					}
					const int self = static_cast<int>(std::ranges::find(active->actors, id) - active->actors.begin());
					try {
						if (parts.size() == 2) {
							FireEvent(*active, parts[0], self, std::stoi(parts[1]), self);
						} else if (parts.size() == 4) {
							FireEvent(*active, parts[0], std::stoi(parts[1]), std::stoi(parts[2]), std::stoi(parts[3]));
						} else {
							REX::WARN("Events: 4StimEvent.{} on {:08X}: expected <event>,<target> or <event>,<actor>,<target>,<performer>", payload, id);
						}
					} catch (const std::exception&) {
						REX::WARN("Events: 4StimEvent.{} on {:08X}: roles must be numbers", payload, id);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimPeak") == 0) {
				// OStim's OStimPeak: "4StimPeak.<action index>", for actions
				// with "peaksAnnotated".
				std::string payload = a_payload.c_str();
				F4SE::GetTaskInterface()->AddTask([id, payload]() {
					try {
						Peaks::Annotated(id, std::stoi(payload));
					} catch (const std::exception&) {
						REX::WARN("Peaks: 4StimPeak.{} on {:08X}: the action must be a number", payload, id);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimPlayExpression") == 0) {
				// OStim's OStimPlayExpression: the event expression, no sound.
				std::string payload = a_payload.c_str();
				F4SE::GetTaskInterface()->AddTask([id, payload]() {
					if (FindActiveScene(id)) {
						std::string event = payload;
						std::ranges::transform(event, event.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						Expressions::SetEvent(id, event, true);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimBendSoS") == 0) {
				// OStim's OStimBendSoS: "<steps>" on top of the scene's bend.
				try {
					PenisBend::Offset(id, std::stoi(a_payload.c_str()));
				} catch (const std::exception&) {
					REX::WARN("Penis bend: 4StimBendSoS.{} on {:08X}: the bend must be a number", a_payload.c_str(), id);
				}
			} else if (_stricmp(a_tag.c_str(), "4StimResetSoS") == 0) {
				PenisBend::Offset(id, 0);  // OStim's OStimResetSoS
			} else if (_stricmp(a_tag.c_str(), "4StimPenisAim") == 0) {
				PenisBend::Aim(id, true);  // AnimBridge aimed this animation's penis: no scene bend on top
			} else if (_stricmp(a_tag.c_str(), "4StimUndress") == 0 || _stricmp(a_tag.c_str(), "4StimRedress") == 0) {
				// OStim's OStimUndress / OStimRedress: everything in the undress
				// slots off, or everything 4Stim took off back on.
				const bool undress = _stricmp(a_tag.c_str(), "4StimUndress") == 0;
				F4SE::GetTaskInterface()->AddTask([id, undress]() {
					if (!FindActiveScene(id)) {
						return;
					}
					if (undress) {
						Undress::StripAll(id);
					} else {
						Undress::Redress(id, true);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimUndressPartial") == 0 || _stricmp(a_tag.c_str(), "4StimRedressPartial") == 0) {
				// OStim's OStimUndressPartial / OStimRedressPartial: "<hex slot
				// mask>". 4Stim: Fallout 4's slots (bit 0 = slot 30), as
				// OActor.UndressPartial; OStim's masks are Skyrim's, so an
				// animation converted from OStim needs its mask mapped (EVENTS.md).
				const bool  undress = _stricmp(a_tag.c_str(), "4StimUndressPartial") == 0;
				std::string payload = a_payload.c_str();
				try {
					const auto mask = static_cast<std::uint32_t>(std::stoul(payload, nullptr, 16));
					F4SE::GetTaskInterface()->AddTask([id, undress, mask]() {
						if (!FindActiveScene(id)) {
							return;
						}
						if (undress) {
							Undress::StripMask(id, mask);
						} else {
							Undress::RedressMask(id, mask);
						}
					});
				} catch (const std::exception&) {
					REX::WARN("Undress: {}.{} on {:08X}: the slot mask must be hexadecimal", a_tag.c_str(), payload, id);
				}
			} else if (_stricmp(a_tag.c_str(), "4StimRemoveWeapons") == 0 || _stricmp(a_tag.c_str(), "4StimAddWeapons") == 0) {
				// OStim's OStimRemoveWeapons / OStimAddWeapons: the weapon off
				// (remembered, as at a scene's start) or back on.
				const std::string_view function = _stricmp(a_tag.c_str(), "4StimRemoveWeapons") == 0 ? "HolsterByID"sv : "RearmByID"sv;
				F4SE::GetTaskInterface()->AddTask([id, function]() {
					if (g_vm && FindActiveScene(id)) {
						RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
						Compat::DispatchStaticCall(g_vm, "FourStimScene"sv, function, callback, static_cast<std::int32_t>(id));
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimSetLooking") == 0) {
				// OStim's OStimSetLooking: "<left>" or "<left>,<up>" (-100 to
				// 100; - is right / down), over the scene's look until the next
				// scene or 4StimResetLooking.
				std::string payload = a_payload.c_str();
				try {
					const auto  comma = payload.find(',');
					const float left = static_cast<float>(std::stoi(payload.substr(0, comma)));
					const float up = comma != std::string::npos ? static_cast<float>(std::stoi(payload.substr(comma + 1))) : 0.0F;
					EyeLook::Set(id, std::clamp(up, -100.0F, 100.0F), std::clamp(left, -100.0F, 100.0F));
				} catch (const std::exception&) {
					REX::WARN("Eyes: 4StimSetLooking.{} on {:08X}: expected <left> or <left>,<up>", payload, id);
				}
			} else if (_stricmp(a_tag.c_str(), "4StimUnsetLooking") == 0) {
				EyeLook::Set(id, 0.0F, 0.0F);  // OStim's OStimUnsetLooking: their eyes their own
			} else if (_stricmp(a_tag.c_str(), "4StimResetLooking") == 0) {
				// OStim's OStimResetLooking: the scene's look again (none: as Unset).
				F4SE::GetTaskInterface()->AddTask([id]() {
					const auto active = FindActiveScene(id);
					const auto scene = active ? SceneRegistry::RoleScene(SceneRegistry::Find(active->sceneID)) : nullptr;
					const auto role = active ? static_cast<std::size_t>(std::ranges::find(active->actors, id) - active->actors.begin()) : 0;
					if (scene && role < scene->actors.size()) {
						EyeLook::Set(id, scene->actors[role].lookUp, scene->actors[role].lookLeft);
					} else {
						EyeLook::Set(id, 0.0F, 0.0F);
					}
				});
			} else if (_stricmp(a_tag.c_str(), "4StimEquipObject") == 0 || _stricmp(a_tag.c_str(), "4StimUnequipObject") == 0) {
				// OStim's OStimEquipObject / OStimUnequipObject: "<type>".
				const bool  equip = _stricmp(a_tag.c_str(), "4StimEquipObject") == 0;
				std::string payload = a_payload.c_str();
				if (equip) {
					EquipObjects::Equip(id, payload);
				} else {
					EquipObjects::Unequip(id, payload);
				}
			}
		}

		static RE::BSEventNotifyControl ThunkActor(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			Log(a_this, a_event);
			return originalActor(a_this, a_event, a_source);
		}

		static RE::BSEventNotifyControl ThunkPlayer(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			Log(a_this, a_event);
			return originalPlayer(a_this, a_event, a_source);
		}

		// After game data loads (the player exists then): checks the player's
		// own vtable pointer at +0x38 against the one to patch first, so a
		// wrong table index can't patch the wrong function.
		static void Install()
		{
			static bool installed = false;
			if (installed) {
				return;
			}
			installed = true;
			REL::Relocation<std::uintptr_t> check{ RE::VTABLE::PlayerCharacter[3] };
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto actual = player ? *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(player) + 0x38) : 0;
			if (actual != check.address()) {
				REX::WARN("Animation event log: vtable check failed ({:X} vs {:X}), not installed", actual, check.address());
				return;
			}
			REL::Relocation<std::uintptr_t> actorVtbl{ RE::VTABLE::Actor[3] };
			originalActor = reinterpret_cast<func_t>(actorVtbl.write_vfunc(1, ThunkActor));
			REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE::PlayerCharacter[3] };
			originalPlayer = reinterpret_cast<func_t>(playerVtbl.write_vfunc(1, ThunkPlayer));
			REX::INFO("Animation event log hook installed");
		}
	};

	void InstallAnimEventHook()
	{
		AnimEventHook::Install();
	}

	void HandleTimedCue(const AnimCues::Due& a_due)
	{
		AnimEventHook::Handle(a_due.actor, a_due.tag, a_due.payload, false);
	}
}
