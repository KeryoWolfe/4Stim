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
//
// Registrations aren't saved yet: they're cleared when a game loads, and
// scripts register again on load.
namespace SceneEvents
{
	bool Register(RE::TESForm* a_receiver);
	void Unregister(RE::TESForm* a_receiver);
	void Clear();

	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_oldID, const std::string& a_newID);
	void SpeedChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID, int a_level, int a_count);
	void SceneEnded(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
}
