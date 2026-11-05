#include "ApiForms.h"
#include "compat/Fallbacks.h"
#include "compat/ScrapFunction.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "Actions.h"
#include "ActorProperties.h"
#include "Bridge.h"
#include "Excitement.h"
#include "Furniture.h"

namespace ApiForms
{
	namespace
	{
		constexpr auto PLUGIN = "4Stim.esl";

		// OStim's Util::APITable forms (same form IDs in 4Stim.esl).
		struct Forms
		{
			std::uint32_t actorCount = 0;     // 4StimActorCountFaction 0xECA
			std::uint32_t excitement = 0;     // 4StimExcitementFaction 0xD93
			std::uint32_t schlongified = 0;   // 4StimSchlongifiedFaction 0xE9C
			std::uint32_t timesClimaxed = 0;  // 4StimTimesClimaxedFaction 0xE49
			std::uint32_t timeUntilClimax = 0;  // 4StimTimeUntilClimaxFaction 0xE4A
			std::uint32_t actorList = 0;      // OStatActorList 0x85F
			std::uint32_t playerPartnerList = 0;  // OStatPlayerPartnerList 0xA03
		};
		Forms g_forms;
		bool  g_looked = false;
		bool  g_have = false;

		bool Have()
		{
			if (g_looked) {
				return g_have;
			}
			g_looked = true;
			const auto handler = RE::TESDataHandler::GetSingleton();
			const auto find = [&](std::uint32_t a_local) {
				const auto form = handler ? handler->LookupForm(a_local, PLUGIN) : nullptr;
				return form ? form->GetFormID() : 0u;
			};
			g_forms.actorCount = find(0xECA);
			g_forms.excitement = find(0xD93);
			g_forms.schlongified = find(0xE9C);
			g_forms.timesClimaxed = find(0xE49);
			g_forms.timeUntilClimax = find(0xE4A);
			g_forms.actorList = find(0x85F);
			g_forms.playerPartnerList = find(0xA03);
			g_have = g_forms.excitement != 0;
			if (g_have) {
				REX::INFO("API forms: {} found: scene actors get OStim's factions and stats", PLUGIN);
			} else {
				REX::WARN("API forms: {} isn't loaded: no API factions, stats or lists (enable it for add-ons that read them)", PLUGIN);
			}
			return g_have;
		}

		// One actor's faction changes, sent to Papyrus together.
		struct Change
		{
			std::vector<std::int32_t> add;  // AddToFaction (rank 0 if not in it)
			std::vector<std::int32_t> set;  // SetFactionRank...
			std::vector<std::int32_t> ranks;  // ...to these
			std::vector<std::int32_t> remove;
			std::vector<std::int32_t> increment;  // OStim's incrementFaction: +1 (1 if not in it), up to 101
			std::vector<std::int32_t> lists;      // FormList.AddForm, if it isn't there

			void Set(std::uint32_t a_faction, int a_rank)
			{
				if (a_faction) {
					set.push_back(static_cast<std::int32_t>(a_faction));
					ranks.push_back(a_rank);
				}
			}
			[[nodiscard]] bool Empty() const { return add.empty() && set.empty() && remove.empty() && increment.empty() && lists.empty(); }
		};

		void Send(std::uint32_t a_actor, Change& a_change)
		{
			if (a_change.Empty()) {
				return;
			}
			const auto vm = FourStim::GetVM();
			if (!vm) {
				return;
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(vm, "FourStimFactions"sv, "Apply"sv, callback, static_cast<std::int32_t>(a_actor), std::move(a_change.add), std::move(a_change.set),
				std::move(a_change.ranks), std::move(a_change.remove), std::move(a_change.increment), std::move(a_change.lists));
			a_change = {};
		}

		void Append(std::vector<std::int32_t>& a_to, const std::vector<std::uint32_t>& a_ids)
		{
			for (const auto id : a_ids) {
				if (std::ranges::find(a_to, static_cast<std::int32_t>(id)) == a_to.end()) {
					a_to.push_back(static_cast<std::int32_t>(id));
				}
			}
		}

		struct ActorState
		{
			std::vector<std::uint32_t>        nodeFactions;       // the scene's action factions they're in now
			std::vector<std::uint32_t>        furnitureFactions;  // the furniture's
			std::unordered_set<std::uint32_t> statsDone;          // OStim's ThreadActor::statFactions: once per scene
			int                               excitementRank = -1;
		};
		std::unordered_map<std::uint32_t, ActorState> g_actors;
		float                                         g_tickWait = 0.0F;

		bool IsPlayer(std::uint32_t a_id)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			return player && player->GetFormID() == a_id;
		}

		// OStim's GraphActor merge: what each side of each action this role is on gives it.
		Actions::ApiFormRefs RoleForms(const SceneRegistry::Scene& a_scene, std::size_t a_role)
		{
			Actions::ApiFormRefs out;
			for (const auto& action : a_scene.actions) {
				if (action.actor == a_role) {
					out.Merge(action.type->actor.forms);
				}
				if (action.target == a_role) {
					out.Merge(action.type->target.forms);
				}
				if (action.performer == a_role) {
					out.Merge(action.type->performer.forms);
				}
			}
			return out;
		}

		// OStim's incrementStatFaction: once per scene per faction.
		void IncrementStat(std::uint32_t a_actor, Change& a_change, const std::vector<std::uint32_t>& a_factions)
		{
			auto& done = g_actors[a_actor].statsDone;
			for (const auto id : a_factions) {
				if (done.insert(id).second) {
					a_change.increment.push_back(static_cast<std::int32_t>(id));
				}
			}
		}

		// OStim's changeNodeAPIPre / Post for one actor: the node's factions
		// swapped, its stat factions counted, and in the player's scene what
		// they do with the player.
		void EnterNode(const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, bool a_playerThread, std::size_t a_role, Change& a_change)
		{
			const auto id = a_actors[a_role];
			auto&      state = g_actors[id];
			const auto forms = RoleForms(a_scene, a_role);
			for (const auto old : state.nodeFactions) {
				if (std::ranges::find(forms.factions, old) == forms.factions.end()) {
					a_change.remove.push_back(static_cast<std::int32_t>(old));
				}
			}
			for (const auto now : forms.factions) {
				if (std::ranges::find(state.nodeFactions, now) == state.nodeFactions.end()) {
					a_change.add.push_back(static_cast<std::int32_t>(now));
				}
			}
			state.nodeFactions = forms.factions;
			IncrementStat(id, a_change, forms.statFactions);
			if (!a_playerThread) {
				return;
			}
			const auto player = std::ranges::find_if(a_actors, IsPlayer);
			if (player == a_actors.end()) {
				return;
			}
			const auto playerRole = static_cast<std::size_t>(player - a_actors.begin());
			for (const auto& action : a_scene.actions) {
				if (action.actor == playerRole && action.target == a_role) {
					IncrementStat(id, a_change, action.type->target.forms.playerStatFactions);
					Append(a_change.lists, action.type->target.forms.playerStatLists);
				} else if (action.actor == a_role && action.target == playerRole) {
					IncrementStat(id, a_change, action.type->actor.forms.playerStatFactions);
					Append(a_change.lists, action.type->actor.forms.playerStatLists);
				}
			}
		}

		void FurnitureFactions(std::uint32_t a_actor, const std::string& a_type, Change& a_change)
		{
			auto&      state = g_actors[a_actor];
			const auto now = a_type.empty() ? std::vector<std::uint32_t>{} : Furniture::Factions(a_type);
			for (const auto old : state.furnitureFactions) {
				if (std::ranges::find(now, old) == now.end()) {
					a_change.remove.push_back(static_cast<std::int32_t>(old));
				}
			}
			for (const auto id : now) {
				if (std::ranges::find(state.furnitureFactions, id) == state.furnitureFactions.end()) {
					a_change.add.push_back(static_cast<std::int32_t>(id));
				}
			}
			state.furnitureFactions = now;
		}
	}

	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, bool a_playerThread, const std::string& a_furnitureType)
	{
		if (!Have()) {
			return;
		}
		for (std::size_t role = 0; role < a_actors.size(); ++role) {
			const auto id = a_actors[role];
			g_actors[id] = ActorState{};
			Change change;
			// OStim's ThreadActor constructor and Thread start.
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (actor && ActorProperties::HasPenis(actor)) {
				change.add.push_back(static_cast<std::int32_t>(g_forms.schlongified));
			}
			change.Set(g_forms.excitement, 0);
			change.Set(g_forms.timesClimaxed, 0);
			change.Set(g_forms.timeUntilClimax, -1);
			change.Set(g_forms.actorCount, static_cast<int>(a_actors.size()));
			if (g_forms.actorList) {
				change.lists.push_back(static_cast<std::int32_t>(g_forms.actorList));
			}
			if (a_playerThread && !IsPlayer(id) && g_forms.playerPartnerList) {
				change.lists.push_back(static_cast<std::int32_t>(g_forms.playerPartnerList));
			}
			g_actors[id].excitementRank = 0;
			FurnitureFactions(id, a_furnitureType, change);
			EnterNode(a_actors, a_scene, a_playerThread, role, change);
			std::erase(change.add, 0);
			Send(id, change);
		}
	}

	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, bool a_playerThread)
	{
		if (!Have()) {
			return;
		}
		for (std::size_t role = 0; role < a_actors.size(); ++role) {
			Change change;
			EnterNode(a_actors, a_scene, a_playerThread, role, change);
			Send(a_actors[role], change);
		}
	}

	void FurnitureChanged(const std::vector<std::uint32_t>& a_actors, const std::string&, const std::string& a_newType)
	{
		if (!Have()) {
			return;
		}
		for (const auto id : a_actors) {
			Change change;
			FurnitureFactions(id, a_newType, change);
			Send(id, change);
		}
	}

	// OStim's ThreadActor::climaxAPI.
	void Climaxed(std::uint32_t a_actor, const std::vector<std::uint32_t>& a_actors, const SceneRegistry::Scene& a_scene, int a_times, bool a_playerThread)
	{
		if (!Have()) {
			return;
		}
		const auto at = std::ranges::find(a_actors, a_actor);
		if (at == a_actors.end()) {
			return;
		}
		const auto role = static_cast<std::size_t>(at - a_actors.begin());
		std::unordered_map<std::uint32_t, Change> changes;
		auto& mine = changes[a_actor];
		mine.Set(g_forms.timesClimaxed, a_times);
		Append(mine.increment, RoleForms(a_scene, role).climaxStatFactions);  // every climax, not once
		const bool meIsPlayer = IsPlayer(a_actor);
		for (const auto& action : a_scene.actions) {
			const bool asActor = action.actor == role;
			const bool asTarget = !asActor && action.target == role;
			if (!asActor && !asTarget) {
				continue;
			}
			const auto partnerRole = asActor ? action.target : action.actor;
			if (partnerRole >= a_actors.size()) {
				continue;
			}
			const auto  partner = a_actors[partnerRole];
			const auto& partnerSide = asActor ? action.type->target.forms : action.type->actor.forms;  // the partner's side of it
			const auto& mySide = asActor ? action.type->actor.forms : action.type->target.forms;
			IncrementStat(partner, changes[partner], partnerSide.partnerClimaxStatFactions);
			if (meIsPlayer) {
				IncrementStat(partner, changes[partner], partnerSide.playerPartnerClimaxStatFactions);
				Append(changes[partner].lists, partnerSide.playerPartnerClimaxStatLists);
			} else if (IsPlayer(partner) && a_playerThread) {
				IncrementStat(a_actor, changes[a_actor], mySide.playerClimaxStatFactions);
				Append(changes[a_actor].lists, mySide.playerClimaxStatLists);
			}
		}
		for (auto& [id, change] : changes) {
			Send(id, change);
		}
	}

	void SceneEnded(const std::vector<std::uint32_t>& a_actors)
	{
		if (!Have()) {
			return;
		}
		for (const auto id : a_actors) {
			Change change;
			const auto it = g_actors.find(id);
			if (it != g_actors.end()) {
				Append(change.remove, it->second.nodeFactions);
				Append(change.remove, it->second.furnitureFactions);
				g_actors.erase(it);
			}
			// OStim's Core::free.
			for (const auto faction : { g_forms.actorCount, g_forms.excitement, g_forms.schlongified, g_forms.timesClimaxed, g_forms.timeUntilClimax }) {
				if (faction) {
					change.remove.push_back(static_cast<std::int32_t>(faction));
				}
			}
			Send(id, change);
		}
	}

	void Tick(float a_seconds)
	{
		if (g_actors.empty() || (g_tickWait -= a_seconds) > 0.0F || !Have()) {
			return;
		}
		g_tickWait = 1.0F;
		for (auto& [id, state] : g_actors) {
			const float excitement = Excitement::Get(id);
			if (excitement < 0.0F) {
				continue;
			}
			const int rank = static_cast<int>(std::clamp(excitement, 0.0F, 100.0F));
			if (rank != state.excitementRank) {
				state.excitementRank = rank;
				Change change;
				change.Set(g_forms.excitement, rank);
				Send(id, change);
			}
		}
	}

	void Clear()
	{
		g_actors.clear();
	}
}
