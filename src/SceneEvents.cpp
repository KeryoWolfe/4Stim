#include <algorithm>
#include <mutex>

#include "Bridge.h"
#include "Physics.h"
#include "SceneEvents.h"

namespace SceneEvents
{
	namespace
	{
		std::mutex                 g_lock;
		std::vector<std::uint64_t> g_receivers;  // VM object handles

		std::uint64_t HandleFor(RE::TESForm* a_form)
		{
			const auto vm = FourStim::GetVM();
			if (!vm || !a_form) {
				return 0;
			}
			auto&      policy = vm->GetObjectHandlePolicy();
			const auto handle = policy.GetHandleForObject(static_cast<std::uint32_t>(a_form->GetFormType()), a_form);
			return handle == policy.EmptyHandle() ? 0 : static_cast<std::uint64_t>(handle);
		}

		// Calls a_function on every script attached to every registered form.
		// SendEvent, unlike DispatchMethodCall, doesn't need the receiving
		// script's name: it runs the function on whichever attached scripts
		// have it, and skips the rest.
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
				vm->SendEvent(
					handle, name,
					[&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
						a_out = RE::BSScript::detail::PackVariables(a_args...);
						return true;
					},
					[](const RE::BSTSmartPointer<RE::BSScript::Object>&) { return true; },
					callback);
			}
		}
	}

	bool Register(RE::TESForm* a_receiver)
	{
		const auto handle = HandleFor(a_receiver);
		if (!handle) {
			REX::WARN("RegisterForSceneEvents: no script handle for {:08X}", a_receiver ? a_receiver->GetFormID() : 0);
			return false;
		}
		std::scoped_lock lock(g_lock);
		if (std::ranges::find(g_receivers, handle) == g_receivers.end()) {
			g_receivers.push_back(handle);
			REX::INFO("Scene events: {:08X} registered", a_receiver->GetFormID());
		}
		return true;
	}

	void Unregister(RE::TESForm* a_receiver)
	{
		if (const auto handle = HandleFor(a_receiver)) {
			std::scoped_lock lock(g_lock);
			std::erase(g_receivers, handle);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_receivers.clear();
	}

	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		Physics::SceneStarted(a_actors, a_sceneID);
		Send("FourStim_OnSceneStart"sv, FourStim::ResolveActors(a_actors), a_sceneID);
	}

	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_oldID, const std::string& a_newID)
	{
		Send("FourStim_OnSceneChange"sv, FourStim::ResolveActors(a_actors), a_oldID, a_newID);
	}

	void SpeedChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, int a_level, int a_count)
	{
		Send("FourStim_OnSpeedChange"sv, FourStim::ResolveActors(a_actors), a_sceneID,
			static_cast<std::int32_t>(a_level), static_cast<std::int32_t>(a_count));
	}

	void SceneEnded(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		Physics::SceneEnded(a_actors);
		Send("FourStim_OnSceneEnd"sv, FourStim::ResolveActors(a_actors), a_sceneID);
	}
}
