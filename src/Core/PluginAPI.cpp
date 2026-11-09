#include "Core/Core.h"

namespace Core::API
{
	ActorImpl* ActorFor(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_apiLock);
		auto& slot = g_apiActors[a_id];
		if (!slot) {
			slot = std::make_unique<ActorImpl>(a_id);
		}
		return slot.get();
	}

	ThreadImpl* ThreadFor(std::int32_t a_id)
	{
		std::scoped_lock lock(g_apiLock);
		auto& slot = g_apiThreads[a_id];
		if (!slot) {
			slot = std::make_unique<ThreadImpl>(a_id);
		}
		return slot.get();
	}

	// The thread object for a running scene, brought up to date.
	ThreadImpl* ThreadOf(const ActiveScene& a_active)
	{
		const auto thread = ThreadFor(a_active.threadID);
		thread->Refresh(a_active);
		return thread;
	}

	InterfaceMapImpl* Map()
	{
		static InterfaceMapImpl map;
		static bool             filled = false;
		if (!filled) {
			filled = true;
			static ThreadInterfaceImpl    threads;
			static AlignmentInterfaceImpl alignment;
			map.addInterface(FourStimAPI::ThreadInterface::NAME, &threads);
			map.addInterface(FourStimAPI::AlignmentInterface::NAME, &alignment);
			map.addInterface(FourStimAPI::SexToyInterface::NAME, SexToys::Interface());
		}
		return &map;
	}

	// Another plugin asks for the map (OStim's UnspecificedSenderMessageHandler).
	void OnPluginMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (a_msg && a_msg->type == FourStimAPI::InterfaceExchangeMessage::MESSAGE_TYPE && a_msg->data &&
			a_msg->dataLen >= sizeof(FourStimAPI::InterfaceExchangeMessage)) {
			static_cast<FourStimAPI::InterfaceExchangeMessage*>(a_msg->data)->interfaceMap = Map();
			REX::INFO("Plugin interface: handed to {}", a_msg->sender ? a_msg->sender : "a plugin");
		}
	}

	// A game loads: no thread runs any more.
	void ForgetThreads()
	{
		std::scoped_lock lock(g_apiLock);
		for (auto& [id, t] : g_apiThreads) {
			t->running = false;
		}
	}

	void OnSceneEvent(SceneEvents::Kind a_kind, const std::vector<std::uint32_t>& a_actors, std::uint32_t a_actor)
	{
		// The NG thread API's events: the player's thread only, as OStim's
		// (ThreadManager's listeners); the player's is thread 0.
		if (std::ranges::any_of(a_actors, [](std::uint32_t a) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a)); })) {
			using E = FourStimNG_API::Thread::ThreadEvent;
			switch (a_kind) {
			case SceneEvents::Kind::kStart: NotifyNG(E::ThreadStarted, 0); break;
			case SceneEvents::Kind::kChange: NotifyNG(E::NodeChanged, 0); break;
			case SceneEvents::Kind::kEnd: NotifyNG(E::ThreadEnded, 0); break;
			default: break;
			}
		}
		ThreadImpl* thread = nullptr;
		if (a_kind == SceneEvents::Kind::kEnd) {
			// The scene's gone already: the thread with these actors.
			std::scoped_lock lock(g_apiLock);
			for (auto& [id, t] : g_apiThreads) {
				if (t->running && t->actors == a_actors) {
					thread = t.get();
					break;
				}
			}
			if (!thread) {
				return;  // never asked for, so never known: nobody can be holding it
			}
		} else {
			const auto active = a_actors.empty() ? nullptr : FindActiveScene(a_actors.front());
			if (!active) {
				return;
			}
			thread = ThreadOf(*active);
		}
		ThreadListeners                                     listeners;
		std::vector<FourStimAPI::ThreadActorEventListener*> climax;
		{
			std::scoped_lock lock(g_apiLock);
			switch (a_kind) {
			case SceneEvents::Kind::kStart:
				listeners = g_onStart;
				break;
			case SceneEvents::Kind::kChange:
				listeners = g_onNode;
				break;
			case SceneEvents::Kind::kSpeed:
				listeners = g_onSpeed;
				break;
			case SceneEvents::Kind::kEnd:
				listeners = g_onStop;
				break;
			case SceneEvents::Kind::kClimax:
				climax = g_onClimax;
				break;
			}
		}
		for (const auto listener : listeners) {
			listener->listen(thread);
		}
		for (const auto listener : climax) {
			listener->listen(thread, ActorFor(a_actor));
		}
		if (a_kind == SceneEvents::Kind::kEnd) {
			thread->running = false;
		}
	}
}
