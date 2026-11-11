#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Mod events (docs/API.md, "Mod events"): SKSE's RegisterForModEvent /
// SendModEvent, which OStim sends all its events through
// (GameAPI/GameUtil.cpp, SKSE::ModCallbackEvent), built for Fallout 4. Fallout
// 4's Papyrus and F4SE have no mod events, so 4Stim keeps the registrations
// itself and calls the callbacks through the script engine, as SKSE does.
//
// A script registers by name with a callback function of OStim's shape:
//
//   FourModEvents.RegisterForModEvent(self, "4stim_thread_start", "OnThreadStart")
//   Function OnThreadStart(string asEventName, string asStrArg, float afNumArg, Form akSender)
//
// Every script attached to the registered form gets the call if it has that
// function. Registrations last until the game is loaded again, as SKSE's:
// scripts register again after each load.
//
// For now only 4Stim and its add-ons use it (4Stim's events, and SendModEvent
// from Papyrus); a version other mods share is for later.
namespace ModEvents
{
	// a_receiver's scripts get a_callback for a_eventName (case-insensitive).
	// Registering the same event again replaces the callback.
	// a_receiver is the VM handle of the script object that registered: a
	// form's, or (as SKSE's) a quest alias's or an active magic effect's.
	bool Register(std::uint64_t a_receiver, std::string_view a_eventName, std::string_view a_callback);
	void Unregister(std::uint64_t a_receiver, std::string_view a_eventName);
	void UnregisterAll(std::uint64_t a_receiver);
	void Clear();

	// Sends a_eventName to its registered callbacks with OStim's four
	// arguments (event name, strArg, numArg, sender). Any thread.
	void Send(std::string_view a_eventName, const std::string& a_strArg, float a_numArg, RE::TESForm* a_sender);

	// 4Stim's events, with OStim's names and arguments ("ostim_" becomes
	// "4stim_"). a_threadID: 0 is the player's scene, as OStim's main thread,
	// which also gets OStim's older main-thread events.
	// As OStim's Thread: a start sends the start event, then its first
	// scene's speed and scene events (Thread::ChangeNode); a scene change
	// sends the speed event, then the scene event; a speed change, the speed
	// event. a_speed is the speed's index (0 = slowest), as OStim's.
	void ThreadStarted(std::int32_t a_threadID, const std::string& a_sceneID, int a_speed);
	void SceneChanged(std::int32_t a_threadID, const std::string& a_sceneID, int a_speed);
	void SpeedChanged(std::int32_t a_threadID, const std::string& a_sceneID, int a_speed);
	void ThreadEnded(std::int32_t a_threadID, const std::string& a_sceneID, const std::vector<std::uint32_t>& a_actors,
		const std::vector<std::string>& a_metadata, std::string_view a_originator);
	void ActorClimaxed(std::int32_t a_threadID, const std::string& a_sceneID, int a_index, std::uint32_t a_actor);
	void FurnitureChanged(std::int32_t a_threadID, std::uint32_t a_furniture, const std::string& a_type);
	// "4stim_event" has its own arguments, as OStim's "ostim_event":
	// Function OnEvent(int aiThreadID, string asType, Form akActor, Form akTarget, Form akPerformer)
	void EventFired(std::int32_t a_threadID, const std::string& a_type, std::uint32_t a_actor, std::uint32_t a_target, std::uint32_t a_performer);
	// 4Stim's own (OStim has no sequence event): a sequence played to its end.
	void SequenceEnded(std::int32_t a_threadID, const std::string& a_sequenceID);

	// OStim's end-event JSON (GameEvents::sendEndEvent): {"scene", "actors":
	// [{"mod", "formid"}], "metadata", "originator"}; and FourJSON's readers.
	std::string EndJSON(const std::string& a_sceneID, const std::vector<std::uint32_t>& a_actors,
		const std::vector<std::string>& a_metadata, std::string_view a_originator);
	std::vector<RE::Actor*> JSONActors(const std::string& a_json);
	std::string JSONScene(const std::string& a_json);
	std::vector<std::string> JSONMetadata(const std::string& a_json);
}
