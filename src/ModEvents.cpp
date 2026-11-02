#include "ModEvents.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Bridge.h"
#include "Events.h"
#include "compat/ScrapFunction.h"

namespace ModEvents
{
	namespace
	{
		struct Registration
		{
			std::uint64_t     handle = 0;  // the VM's handle for the receiving form
			RE::BSFixedString callback;
		};

		std::mutex                                                 g_lock;
		std::unordered_map<std::string, std::vector<Registration>> g_registrations;  // event name, lowercase

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

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

		// The live registrations for a_eventName (forms that are gone are
		// dropped on the way).
		std::vector<Registration> Receivers(std::string_view a_eventName)
		{
			const auto vm = FourStim::GetVM();
			if (!vm) {
				return {};
			}
			std::scoped_lock lock(g_lock);
			const auto       it = g_registrations.find(Lower(a_eventName));
			if (it == g_registrations.end()) {
				return {};
			}
			auto& policy = vm->GetObjectHandlePolicy();
			std::erase_if(it->second, [&](const Registration& a_entry) { return !policy.IsHandleObjectAvailable(a_entry.handle); });
			return it->second;
		}

		// Calls every registered callback for a_eventName with a_args. As
		// SKSE's mod events: every script on the registered form that has the
		// function gets it (SendEvent).
		template <class... Args>
		void Dispatch(std::string_view a_eventName, Args... a_args)
		{
			const auto receivers = Receivers(a_eventName);
			if (receivers.empty()) {
				return;
			}
			REX::INFO("Mod events: \"{}\" to {} receiver(s)", a_eventName, receivers.size());
			const auto                                                    vm = FourStim::GetVM();
			const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			for (const auto& entry : receivers) {
				Compat::SendEvent(vm, entry.handle, entry.callback, callback, a_args...);
			}
		}

		RE::TESForm* Form(std::uint32_t a_id)
		{
			return a_id ? RE::TESForm::GetFormByID(a_id) : nullptr;
		}

		// "4stim_..." from OStim's "ostim_..." (CLAUDE.md naming rule).
		constexpr std::string_view PREFIX = "4stim_";
		std::string Name(std::string_view a_rest) { return std::string(PREFIX) + std::string(a_rest); }
	}

	bool Register(RE::TESForm* a_receiver, std::string_view a_eventName, std::string_view a_callback)
	{
		const auto handle = HandleFor(a_receiver);
		if (!handle || a_eventName.empty() || a_callback.empty()) {
			REX::WARN("Mod events: can't register {:08X} for \"{}\" (no script, or no event or callback name)", a_receiver ? a_receiver->GetFormID() : 0, a_eventName);
			return false;
		}
		std::scoped_lock lock(g_lock);
		auto&            list = g_registrations[Lower(a_eventName)];
		const auto       it = std::ranges::find_if(list, [&](const Registration& a_entry) { return a_entry.handle == handle; });
		if (it != list.end()) {
			it->callback = RE::BSFixedString(a_callback);
		} else {
			list.push_back({ handle, RE::BSFixedString(a_callback) });
		}
		REX::INFO("Mod events: {:08X} registered for \"{}\" ({})", a_receiver->GetFormID(), a_eventName, a_callback);
		return true;
	}

	void Unregister(RE::TESForm* a_receiver, std::string_view a_eventName)
	{
		const auto handle = HandleFor(a_receiver);
		if (!handle) {
			return;
		}
		std::scoped_lock lock(g_lock);
		if (const auto it = g_registrations.find(Lower(a_eventName)); it != g_registrations.end()) {
			std::erase_if(it->second, [&](const Registration& a_entry) { return a_entry.handle == handle; });
		}
	}

	void UnregisterAll(RE::TESForm* a_receiver)
	{
		const auto handle = HandleFor(a_receiver);
		if (!handle) {
			return;
		}
		std::scoped_lock lock(g_lock);
		for (auto& [name, list] : g_registrations) {
			std::erase_if(list, [&](const Registration& a_entry) { return a_entry.handle == handle; });
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_registrations.clear();
	}

	void Send(std::string_view a_eventName, const std::string& a_strArg, float a_numArg, RE::TESForm* a_sender)
	{
		Dispatch(a_eventName, std::string(a_eventName), a_strArg, a_numArg, a_sender);
	}

	// ---- 4Stim's events: OStim's GameAPI/GameEvents.cpp, renamed ----
	// OStim's sender for thread events is its main quest; 4Stim has no
	// plugin file, so they come with None.

	void ThreadStarted(std::int32_t a_threadID, const std::string& a_sceneID, int a_speed)
	{
		if (a_threadID == 0) {  // OStim's main-thread events
			Send(Name("prestart"), "", 0.0F, nullptr);
			Send(Name("start"), "", 0.0F, nullptr);
		}
		Send(Name("thread_start"), "", static_cast<float>(a_threadID), nullptr);
		// OStim's start goes into its first scene (Thread::ChangeNode).
		SceneChanged(a_threadID, a_sceneID, a_speed);
	}

	void SceneChanged(std::int32_t a_threadID, const std::string& a_sceneID, int a_speed)
	{
		// ChangeNode sets the speed (SetSpeed's event) before its scene event.
		SpeedChanged(a_threadID, a_sceneID, a_speed);
		if (a_threadID == 0) {
			Send(Name("scenechanged"), a_sceneID, 0.0F, nullptr);
			Send(Name("scenechanged_") + a_sceneID, "", 0.0F, nullptr);
		}
		Send(Name("thread_scenechanged"), a_sceneID, static_cast<float>(a_threadID), nullptr);
	}

	void SpeedChanged(std::int32_t a_threadID, const std::string& a_sceneID, int a_speed)
	{
		if (a_threadID == 0) {
			Send(Name("animationchanged"), a_sceneID, static_cast<float>(a_speed), nullptr);
		}
		Send(Name("thread_speedchanged"), std::to_string(a_speed), static_cast<float>(a_threadID), nullptr);
	}

	void ThreadEnded(std::int32_t a_threadID, const std::string& a_sceneID, const std::vector<std::uint32_t>& a_actors,
		const std::vector<std::string>& a_metadata, std::string_view a_originator)
	{
		const auto json = EndJSON(a_sceneID, a_actors, a_metadata, a_originator);
		if (a_threadID == 0) {
			Send(Name("end"), json, -1.0F, nullptr);
			Send(Name("totalend"), "", 0.0F, nullptr);
		}
		Send(Name("thread_end"), json, static_cast<float>(a_threadID), nullptr);
	}

	void ActorClimaxed(std::int32_t a_threadID, const std::string& a_sceneID, int a_index, std::uint32_t a_actor)
	{
		const auto actor = Form(a_actor);
		if (a_threadID == 0) {
			Send(Name("orgasm"), a_sceneID, static_cast<float>(a_index), actor);
		}
		Send(Name("actor_orgasm"), a_sceneID, static_cast<float>(a_threadID), actor);
	}

	void FurnitureChanged(std::int32_t a_threadID, std::uint32_t a_furniture, const std::string& a_type)
	{
		// A wall is no object: None, as a scene off furniture.
		Send(Name("furniturechanged"), a_type, static_cast<float>(a_threadID), Form(a_furniture));
	}

	void EventFired(std::int32_t a_threadID, const std::string& a_type, std::uint32_t a_actor, std::uint32_t a_target, std::uint32_t a_performer)
	{
		if (a_threadID == 0 && Events::IsChildOf(a_type, "spank")) {
			Send(Name("spank"), "", 0.0F, Form(a_target));
		}
		// OStim's ostim_event: its own arguments (OSKSE.SendOStimEvent).
		Dispatch(Name("event"), a_threadID, a_type, Form(a_actor), Form(a_target), Form(a_performer));
	}

	void SequenceEnded(std::int32_t a_threadID, const std::string& a_sequenceID)
	{
		Send(Name("thread_sequenceend"), a_sequenceID, static_cast<float>(a_threadID), nullptr);
	}

	// ---- The end event's JSON (OStim's sendEndEvent and OJSON) ----

	std::string EndJSON(const std::string& a_sceneID, const std::vector<std::uint32_t>& a_actors,
		const std::vector<std::string>& a_metadata, std::string_view a_originator)
	{
		auto json = nlohmann::json::object();
		json["scene"] = a_sceneID;
		json["actors"] = nlohmann::json::array();
		for (const auto id : a_actors) {
			// OStim's GameRecordIdentifier::toJson: the plugin and the form ID
			// within it, so the JSON stays good across load order changes.
			auto              entry = nlohmann::json::object();
			const auto        index = static_cast<std::uint8_t>(id >> 24);
			std::uint32_t     local = id & 0x00FFFFFF;
			const RE::TESFile* file = nullptr;
			if (const auto handler = RE::TESDataHandler::GetSingleton()) {
				if (index == 0xFE) {
					file = handler->LookupLoadedLightModByIndex(static_cast<std::uint16_t>(local >> 12));
					local &= 0xFFF;
				} else if (index != 0xFF) {
					file = handler->LookupLoadedModByIndex(index);
				}
			}
			entry["mod"] = file ? std::string(file->GetFilename()) : std::string{};
			entry["formid"] = std::format("{:x}", file ? local : id);
			json["actors"].push_back(std::move(entry));
		}
		json["metadata"] = a_metadata;
		json["originator"] = a_originator.empty() ? "normal" : std::string(a_originator);
		return json.dump();
	}

	namespace
	{
		std::optional<nlohmann::json> Parse(const std::string& a_json)
		{
			try {
				auto json = nlohmann::json::parse(a_json);
				if (json.is_object()) {
					return json;
				}
			} catch (const std::exception&) {
			}
			REX::WARN("FourJSON: not a 4Stim end-event JSON: {}", a_json.substr(0, 80));
			return std::nullopt;
		}
	}

	std::vector<RE::Actor*> JSONActors(const std::string& a_json)
	{
		std::vector<RE::Actor*> actors;
		const auto              json = Parse(a_json);
		if (!json || !json->contains("actors") || !(*json)["actors"].is_array()) {
			return actors;
		}
		const auto handler = RE::TESDataHandler::GetSingleton();
		for (const auto& entry : (*json)["actors"]) {
			RE::Actor* actor = nullptr;
			if (entry.is_object()) {
				const auto    mod = entry.value("mod", std::string{});
				std::uint32_t id = 0;
				try {
					id = static_cast<std::uint32_t>(std::stoul(entry.value("formid", std::string{ "0" }), nullptr, 16));
				} catch (const std::exception&) {
				}
				if (mod.empty()) {
					actor = RE::TESForm::GetFormByID<RE::Actor>(id);  // a reference made in game
				} else if (handler) {
					actor = handler->LookupForm<RE::Actor>(id, mod);
				}
			}
			actors.push_back(actor);  // None where it's gone, as OStim keeps the order
		}
		return actors;
	}

	std::string JSONScene(const std::string& a_json)
	{
		const auto json = Parse(a_json);
		return json ? json->value("scene", std::string{}) : std::string{};
	}

	std::vector<std::string> JSONMetadata(const std::string& a_json)
	{
		std::vector<std::string> metadata;
		const auto               json = Parse(a_json);
		if (json && json->contains("metadata") && (*json)["metadata"].is_array()) {
			for (const auto& entry : (*json)["metadata"]) {
				if (entry.is_string()) {
					metadata.push_back(entry.get<std::string>());
				}
			}
		}
		return metadata;
	}
}
