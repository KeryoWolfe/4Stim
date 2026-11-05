#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SceneRegistry.h"

// OStim's API forms (docs/API.md, "Factions, form lists and keywords"), in
// 4Stim.esl (tools/make-api-plugin.py, OStim's form IDs): what add-ons'
// dialogue, voice-set and quest conditions read. As OStim's (Util/APITable,
// ThreadActor's API functions, Thread's furniture factions):
//
//   - in a scene: 4StimExcitementFaction (rank = excitement),
//     4StimTimesClimaxedFaction (rank = climaxes this scene),
//     4StimTimeUntilClimaxFaction (-1), 4StimActorCountFaction (rank = the
//     scene's actor count), 4StimSchlongifiedFaction (has a penis);
//   - the actions' "faction"s for the actor's side of what they do now
//     (4StimVaginalSexActorFaction...), and the furniture type's
//     (4StimBedFaction...);
//   - stats that stay: an action's "statFaction" +1 per scene they did it in
//     (OStat...), "climaxStatFaction" +1 per climax at it, the partner's and
//     with-the-player variants, and the form lists of who did what with the
//     player (OStat...List), OStatActorList (everyone ever in a scene) and
//     OStatPlayerPartnerList.
//
// Factions are changed through Papyrus (FourStimFactions.psc: CommonLibF4
// has no native AddToFaction / SetFactionRank), one call per actor and
// change, so the game saves them as it saves any faction rank. Without
// 4Stim.esl this does nothing (one log line). Main thread.
namespace ApiForms
{
	// a_actors in role order; a_playerThread: the player is in it (OStim's
	// player thread); a_furnitureType "" for none.
	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, bool a_playerThread, const std::string& a_furnitureType);
	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, bool a_playerThread);
	void FurnitureChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_oldType, const std::string& a_newType);
	// a_actor climaxed for the a_times-th time in this scene.
	void Climaxed(std::uint32_t a_actor, const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, int a_times, bool a_playerThread);
	void SceneEnded(const std::vector<std::uint32_t>& a_actors);
	// The excitement ranks, at most once a second (the scene clock).
	void Tick(float a_seconds);
	// A game is loading: forget the scene state (the factions come from the save).
	void Clear();
}
