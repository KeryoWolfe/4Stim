#include <algorithm>
#include <mutex>

#include "Bridge.h"
#include "compat/ScrapFunction.h"
#include "ModEvents.h"
#include "Physics.h"
#include "SceneEvents.h"

namespace SceneEvents
{
	namespace
	{
		std::mutex                 g_lock;
		std::vector<std::uint64_t> g_receivers;  // VM object handles

		// Calls a_function on every script attached to every registered form.
		// SendEvent, unlike DispatchMethodCall, doesn't need the receiving
		// script's name: it runs the function on whichever attached scripts
		// have it, and skips the rest.
		bool WithoutPlayer(const std::vector<std::uint32_t>& a_actors)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			return !player || std::ranges::find(a_actors, player->GetFormID()) == a_actors.end();
		}

		template <class... Args>
		void Send(std::string_view a_function, Args... a_args)
		{
			const auto vm = FourStim::GetVM();
			if (!vm) {
				return;
			}
			std::vector<std::uint64_t> receivers;
			{
				std::scoped_lock lock(g_lock);
				auto&            policy = vm->GetObjectHandlePolicy();
				std::erase_if(g_receivers, [&](std::uint64_t a_handle) { return !policy.IsHandleObjectAvailable(a_handle); });
				receivers = g_receivers;
			}
			if (receivers.empty()) {
				return;
			}
			const RE::BSFixedString                                  name{ a_function };
			const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			for (const auto handle : receivers) {
				Compat::SendEvent(vm, handle, name, callback, a_args...);
			}
		}
	}

	bool Register(std::uint64_t a_receiver)
	{
		const auto handle = a_receiver;
		if (!handle) {
			REX::WARN("RegisterForSceneEvents: no script");
			return false;
		}
		std::scoped_lock lock(g_lock);
		if (std::ranges::find(g_receivers, handle) == g_receivers.end()) {
			g_receivers.push_back(handle);
			REX::INFO("Scene events: script {:016X} registered", handle);
		}
		return true;
	}

	void Unregister(std::uint64_t a_receiver)
	{
		if (const auto handle = a_receiver) {
			std::scoped_lock lock(g_lock);
			std::erase(g_receivers, handle);
		}
	}

	namespace
	{
		NativeListener g_native = nullptr;

		void Native(Kind a_kind, const std::vector<std::uint32_t>& a_actors, std::uint32_t a_actor = 0)
		{
			if (g_native) {
				g_native(a_kind, a_actors, a_actor);
			}
		}
	}

	void SetNativeListener(NativeListener a_listener)
	{
		g_native = a_listener;
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_receivers.clear();
	}

	namespace
	{
		// Once a session, at the first scene: what type the scene events'
		// actor list reaches Papyrus as, packed exactly as Send packs it. The
		// physics swap once got a "type mismatch" passing an Actor from here
		// (before RE::Actor had its own TYPE_ID, xmake.lua fix 1, so actors
		// were packed as ObjectReference); "Actor[]" means the events'
		// Actor[] arguments are fine (docs/PORTING_ROADMAP.md 2.9).
		void CheckActorArrayType(const std::vector<std::uint32_t>& a_actors)
		{
			static bool checked = false;
			if (checked || !FourStim::GetVM()) {
				return;
			}
			checked = true;
			const auto actors = FourStim::ResolveActors(a_actors);
			auto       packed = RE::BSScript::detail::PackVariables(actors);
			const auto type = packed[0].GetType();
			const auto info = type.GetObjectTypeInfo();
			const auto name = info ? std::string(info->GetName()) : std::string("raw type ") + std::to_string(static_cast<std::uint32_t>(type.GetRawType()));
			const bool good = type.IsArray() && info && _stricmp(info->GetName(), "Actor") == 0;
			if (good) {
				REX::INFO("Scene events: actors reach Papyrus as {}[] ({} actor(s)): fine", name, actors.size());
			} else {
				REX::WARN("Scene events: actors reach Papyrus as {}{} ({} actor(s)), not Actor[]: scripts listening for scene events would get a type mismatch",
					name, type.IsArray() ? "[]" : "", actors.size());
			}
		}
	}

	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, std::int32_t a_threadID, int a_speed)
	{
		CheckActorArrayType(a_actors);
		Physics::SceneStarted(a_actors, a_sceneID);
		Native(Kind::kStart, a_actors);
		Send("FourStim_OnSceneStart"sv, FourStim::ResolveActors(a_actors), a_sceneID);
		if (WithoutPlayer(a_actors)) {
			Send("FourStim_OnNPCSceneStart"sv, FourStim::ResolveActors(a_actors), a_sceneID);
		}
		ModEvents::ThreadStarted(a_threadID, a_sceneID, a_speed);
	}

	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_oldID, const std::string& a_newID, std::int32_t a_threadID, int a_speed)
	{
		Physics::SceneChanged(a_actors, a_newID);
		Native(Kind::kChange, a_actors);
		Send("FourStim_OnSceneChange"sv, FourStim::ResolveActors(a_actors), a_oldID, a_newID);
		ModEvents::SceneChanged(a_threadID, a_newID, a_speed);
	}

	void SpeedChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, int a_level, int a_count, std::int32_t a_threadID)
	{
		Native(Kind::kSpeed, a_actors);
		Send("FourStim_OnSpeedChange"sv, FourStim::ResolveActors(a_actors), a_sceneID,
			static_cast<std::int32_t>(a_level), static_cast<std::int32_t>(a_count));
		ModEvents::SpeedChanged(a_threadID, a_sceneID, a_level);
	}

	void SceneStopped(const std::vector<std::uint32_t>& a_actors)
	{
		Physics::SceneEnded(a_actors);
	}

	void SceneEnded(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, std::int32_t a_threadID, const std::vector<std::string>& a_metadata)
	{
		Native(Kind::kEnd, a_actors);
		Send("FourStim_OnSceneEnd"sv, FourStim::ResolveActors(a_actors), a_sceneID);
		if (WithoutPlayer(a_actors)) {
			Send("FourStim_OnNPCSceneEnd"sv, FourStim::ResolveActors(a_actors), a_sceneID);
		}
		ModEvents::ThreadEnded(a_threadID, a_sceneID, a_actors, a_metadata, "normal");
	}

	void SequenceEnded(const std::vector<std::uint32_t>& a_actors, const std::string& a_sequenceID, std::int32_t a_threadID)
	{
		Send("FourStim_OnSequenceEnd"sv, FourStim::ResolveActors(a_actors), a_sequenceID);
		ModEvents::SequenceEnded(a_threadID, a_sequenceID);
	}

	void FurnitureChanged(const std::vector<std::uint32_t>& a_actors, std::uint32_t a_furniture, const std::string& a_type, const std::string& a_sceneID, std::int32_t a_threadID)
	{
		const auto furniture = a_furniture ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a_furniture) : nullptr;
		Send("FourStim_OnFurnitureChange"sv, FourStim::ResolveActors(a_actors), furniture, a_type, a_sceneID);
		ModEvents::FurnitureChanged(a_threadID, a_furniture, a_type);
	}

	void Climaxed(std::uint32_t a_actor, const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, int a_times, std::int32_t a_threadID)
	{
		Native(Kind::kClimax, a_actors, a_actor);
		const auto who = FourStim::ResolveActors({ a_actor });
		Send("FourStim_OnClimax"sv, who.front(), FourStim::ResolveActors(a_actors), a_sceneID, a_times);
		const auto index = std::ranges::find(a_actors, a_actor) - a_actors.begin();
		ModEvents::ActorClimaxed(a_threadID, a_sceneID, static_cast<int>(index), a_actor);
	}

	void EventFired(const std::string& a_event, std::uint32_t a_actor, std::uint32_t a_target, std::uint32_t a_performer,
		const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, std::int32_t a_threadID)
	{
		const auto who = FourStim::ResolveActors({ a_actor, a_target, a_performer });
		Send("FourStim_OnEvent"sv, a_event, who[0], who[1], who[2], FourStim::ResolveActors(a_actors), a_sceneID);
		ModEvents::EventFired(a_threadID, a_event, a_actor, a_target, a_performer);
	}
}
