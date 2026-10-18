#pragma once

#include <string>
#include <vector>

// Lets other mods' scripts follow scenes without touching the UI. A script
// on a Quest or ObjectReference registers with
// FourStim.RegisterForSceneEvents(self); the plugin then calls these
// functions on every script attached to that form, if they exist:
//
//   Function FourStim_OnSceneStart(Actor[] akActors, String asSceneID)
//   Function FourStim_OnSceneChange(Actor[] akActors, String asOldID, String asNewID)
//   Function FourStim_OnSpeedChange(Actor[] akActors, String asSceneID, int aiLevel, int aiCount)
//   Function FourStim_OnSceneEnd(Actor[] akActors, String asSceneID)
//   Function FourStim_OnNPCSceneStart(Actor[] akActors, String asSceneID)   (scenes without the player)
//   Function FourStim_OnNPCSceneEnd(Actor[] akActors, String asSceneID)
//   Function FourStim_OnSequenceEnd(Actor[] akActors, String asSequenceID)
//   Function FourStim_OnFurnitureChange(Actor[] akActors, ObjectReference akFurniture, String asType, String asSceneID)
//   Function FourStim_OnClimax(Actor akActor, Actor[] akActors, String asSceneID, int aiTimes)
//   Function FourStim_OnEvent(String asEvent, Actor akActor, Actor akTarget, Actor akPerformer, Actor[] akActors, String asSceneID)
//
// Registrations aren't saved yet: they're cleared when a game loads, and
// scripts register again on load.
namespace SceneEvents
{
	// The same events for C++ (the plugin interface's listeners, main.cpp),
	// called before the Papyrus ones. a_actor: who climaxed (kClimax).
	enum class Kind
	{
		kStart,
		kChange,
		kSpeed,
		kEnd,
		kClimax
	};
	using NativeListener = void (*)(Kind a_kind, const std::vector<std::uint32_t>& a_actors, std::uint32_t a_actor);
	void SetNativeListener(NativeListener a_listener);

	bool Register(RE::TESForm* a_receiver);
	void Unregister(RE::TESForm* a_receiver);
	void Clear();

	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_oldID, const std::string& a_newID);
	void SpeedChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, int a_level, int a_count);
	void SceneEnded(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	// A sequence played to its end (the scene goes on, on its last scene).
	void SequenceEnded(const std::vector<std::uint32_t>& a_actors, const std::string& a_sequenceID);
	// The scene moved to other furniture (OStim's ostim_thread_furniturechanged):
	// a_furniture 0 (None) for a wall, which is no object; then a_type says.
	void FurnitureChanged(const std::vector<std::uint32_t>& a_actors, std::uint32_t a_furniture, const std::string& a_type, const std::string& a_sceneID);
	void Climaxed(std::uint32_t a_actor, const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, int a_times);
	// An event (docs/EVENTS.md) fired; 0 = no one in that part (None).
	void EventFired(const std::string& a_event, std::uint32_t a_actor, std::uint32_t a_target, std::uint32_t a_performer,
		const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
}
