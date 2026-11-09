#include "Core/Core.h"

namespace Core
{
	// A thread that's been asked for but hasn't started in this long has
	// failed somewhere the plugin didn't hear about: dropped.
	constexpr auto STARTING_TIMEOUT = std::chrono::seconds(30);

	// g_threadsLock held.
	void PruneStartingLocked()
	{
		const auto now = std::chrono::steady_clock::now();
		std::erase_if(g_threads, [&](const auto& a_entry) {
			const auto& r = a_entry.second;
			if (r.status == ThreadRecord::Status::kStarting && r.flow == 0 && now - r.since > STARTING_TIMEOUT) {
				REX::WARN("Thread {}: never started (its actors weren't placed within {} s), dropped", r.id, STARTING_TIMEOUT.count());
				return true;
			}
			return false;
		});
	}

	// The thread a_id, if it's starting or running. g_threadsLock held.
	ThreadRecord* ThreadRecordLocked(std::int32_t a_id)
	{
		const auto it = g_threads.find(a_id);
		return it != g_threads.end() ? &it->second : nullptr;
	}

	// Whether a_actor is in a thread that's starting (not running yet).
	bool InStartingThread(std::uint32_t a_actor)
	{
		std::scoped_lock lock(g_threadsLock);
		PruneStartingLocked();
		return std::ranges::any_of(g_threads, [&](const auto& a_entry) {
			return a_entry.second.status == ThreadRecord::Status::kStarting && std::ranges::find(a_entry.second.actors, a_actor) != a_entry.second.actors.end();
		});
	}

	// A start that failed after the thread was handed out: it never runs.
	void DropStartingThread(std::int32_t a_id, std::string_view a_why)
	{
		std::scoped_lock lock(g_threadsLock);
		const auto it = g_threads.find(a_id);
		if (it != g_threads.end() && it->second.status == ThreadRecord::Status::kStarting) {
			REX::WARN("Thread {}: not started ({})", a_id, a_why);
			g_threads.erase(it);
		}
	}

	// The same, by its actors (FourStimMenu.ReleaseFailedStart knows only those).
	void DropStartingThreadOf(std::vector<std::uint32_t> a_actors, std::string_view a_why)
	{
		std::ranges::sort(a_actors);
		std::scoped_lock lock(g_threadsLock);
		std::erase_if(g_threads, [&](const auto& a_entry) {
			if (a_entry.second.status == ThreadRecord::Status::kStarting && a_entry.second.actors == a_actors) {
				REX::WARN("Thread {}: not started ({})", a_entry.first, a_why);
				return true;
			}
			return false;
		});
	}

	// AnimCues: a_actor started a_idle as a scene's animation (a_sceneID's).
	void CuePlayed(std::uint32_t a_actor, const RE::TESIdleForm* a_idle, const std::string& a_sceneID)
	{
		if (!g_settings.timeAnimCues) {
			return;
		}
		const auto scene = SceneRegistry::Find(a_sceneID);
		AnimCues::Played(a_actor, a_idle, !scene || !scene->IsTransition());
	}

	// ---- OStim's thread API (docs/API.md): FourThread, FourThreadBuilder ----
	// A thread is a running scene; its ID is ActiveScene::threadID (0 = the
	// player's, as OStim's player thread). Reads happen where Papyrus calls
	// them, as the other natives here do; changes go to the main thread.

	ActiveScene* FindThread(std::int32_t a_threadID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) { return a_scene.threadID == a_threadID; });
		return it != g_activeScenes.end() ? &*it : nullptr;
	}

	std::int32_t ThreadIDOf(std::uint32_t a_actorID)
	{
		const auto active = FindActiveScene(a_actorID);
		return active ? active->threadID : -1;
	}

	std::string LowerCopy(std::string a_text)
	{
		std::ranges::transform(a_text, a_text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return a_text;
	}

	std::vector<std::string> SplitCSV(const std::string& a_csv)
	{
		std::vector<std::string> out;
		std::string              item;
		for (const char ch : a_csv + ",") {
			if (ch == ',') {
				const auto b = item.find_first_not_of(" \t");
				const auto e = item.find_last_not_of(" \t");
				if (b != std::string::npos) {
					out.push_back(item.substr(b, e - b + 1));
				}
				item.clear();
			} else {
				item += ch;
			}
		}
		return out;
	}

	// The scenes a new scene for a_cast can start with on a_furniture ("" =
	// off furniture), any role order that fits (a thread builder's actors
	// get OStim's order when it fits, else another).
	std::vector<std::shared_ptr<const SceneRegistry::Scene>> ScenesForActors(const std::vector<std::uint32_t>& a_cast, const std::string& a_furniture,
		std::optional<std::size_t> a_pinned)
	{
		SceneRegistry::ListFilter filter;
		filter.actors = ConditionsOf(a_cast);
		filter.pinned = a_pinned;
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> out;
		for (const auto& summary : SceneRegistry::List(a_cast.size(), filter)) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && SceneFitsFurniture(*scene, a_furniture)) {
				out.push_back(scene);
			}
		}
		return out;
	}

	// Why a_actor isn't eligible for scenes at all, or "" if they are:
	// OStim's isEligible (Core.cpp: not disabled, deleted, a child or dead,
	// with an actor type, in no thread), which OThreadBuilder.Create checks.
	std::string NotEligible(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return "no actor";
		}
		if (a_actor->IsDisabled()) {
			return "disabled";
		}
		if (a_actor->IsDeleted()) {
			return "deleted";
		}
		if (a_actor->IsChild()) {
			return "children are never eligible";
		}
		if (a_actor->IsDead(false)) {
			return "dead";
		}
		if (ActorProperties::Type(a_actor).empty()) {
			return "no actor type (docs/ACTORS.md)";
		}
		if (FindActiveScene(a_actor->GetFormID())) {
			return "already in a scene";
		}
		if (InStartingThread(a_actor->GetFormID())) {
			return "already in a scene that's starting";
		}
		return {};
	}

	// Why a_actor can't be in a new scene now, or "" if they can (as
	// FourStimScene.CanUseActor): eligible, loaded (OStim's ThreadStarter),
	// and (4Stim) not in combat.
	std::string CantUse(RE::Actor* a_actor)
	{
		if (auto why = NotEligible(a_actor); !why.empty()) {
			return why;
		}
		if (a_actor->IsInCombat()) {
			return "in combat";
		}
		if (!a_actor->Get3D()) {
			return "not loaded";  // OStim's ThreadStarter: unloaded actors can't start
		}
		return {};
	}
	std::int32_t                                      g_nextBuilderID = 1;

	std::int32_t BuilderCreate(std::vector<RE::Actor*> a_actors)
	{
		ThreadBuilder builder;
		for (const auto actor : a_actors) {
			// As OStim's Create: -1 for an actor who isn't eligible.
			if (const auto why = NotEligible(actor); !why.empty()) {
				REX::WARN("ThreadBuilder.Create: {:08X} isn't eligible ({})", actor ? actor->GetFormID() : 0, why);
				return -1;
			}
			builder.actors.push_back(actor->GetFormID());
		}
		if (builder.actors.empty()) {
			REX::WARN("ThreadBuilder.Create: no actors");
			return -1;
		}
		std::scoped_lock lock(g_buildersLock);
		const auto id = g_nextBuilderID++;
		g_builders.emplace(id, std::move(builder));
		return id;
	}

	// OStim's Node::getRoute: the scenes from a_from to a_to through
	// navigations (transitions on the way included), at most a_steps
	// navigations, each fitting a_cast in role order; transitions play for
	// their length, other scenes on the way half a second (as OStim's 500
	// ms). Empty if there's no way.
	std::vector<SceneRegistry::SequenceEntry> RouteBetween(const std::shared_ptr<const SceneRegistry::Scene>& a_from, const std::shared_ptr<const SceneRegistry::Scene>& a_to,
		std::span<const SceneRegistry::ActorCondition> a_cast, int a_steps)
	{
		using Route = std::vector<std::shared_ptr<const SceneRegistry::Scene>>;
		std::vector<std::string>                                                   visited{ a_from->id };
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> frontier{ { a_from, {} } };
		for (int step = 0; step < a_steps && !frontier.empty(); ++step) {
			std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> next;
			for (const auto& [scene, route] : frontier) {
				for (const auto& nav : scene->navigations) {
					auto  dest = SceneRegistry::Find(nav.to);
					Route path = route;
					for (int hop = 0; dest && hop < 8; ++hop) {
						path.push_back(dest);
						if (!dest->IsTransition()) {
							break;
						}
						dest = SceneRegistry::Find(dest->destination);
					}
					if (!dest || dest->IsTransition() || !SceneRegistry::Fits(dest->actors, a_cast) || std::ranges::find(visited, dest->id) != visited.end()) {
						continue;
					}
					visited.push_back(dest->id);
					if (dest->id == a_to->id) {
						std::vector<SceneRegistry::SequenceEntry> out;
						for (const auto& s : path) {
							out.push_back({ s->id, s->IsTransition() ? std::max(s->length, 0.5F) : 0.5F, 0 });
						}
						return out;
					}
					next.emplace_back(dest, std::move(path));
				}
			}
			frontier = std::move(next);
		}
		return {};
	}

	// A starting entry with no time plays for its scene's length (OStim's
	// animationLengthMs; FourThreadBuilder.AddStartingAnimation's duration
	// is optional). A scene without a length gets this.
	constexpr float STARTING_NO_LENGTH = 10.0F;

	// OStim's ActorUtil::sort, before the player's place: the dominant
	// actors first, then those with a penis, otherwise as given.
	void SortLikeOStim(std::vector<std::uint32_t>& a_cast, const std::vector<std::uint32_t>& a_dominant)
	{
		std::ranges::stable_sort(a_cast, [&](std::uint32_t a, std::uint32_t b) {
			const bool domA = std::ranges::find(a_dominant, a) != a_dominant.end();
			const bool domB = std::ranges::find(a_dominant, b) != a_dominant.end();
			if (domA != domB) {
				return domA;
			}
			return ActorProperties::Cached(a).penis && !ActorProperties::Cached(b).penis;
		});
	}

	// What OStim's start flow decided for a player thread (StartFlow below).
	struct StartChoice
	{
		bool                            placeDecided = false;  // piece is the answer (none = no furniture)
		std::optional<Furniture::Found> piece;
		bool                            sorted = false;        // the cast is in role order already
		std::optional<std::size_t>      pin;                   // this actor keeps their place (the player's)
	};

	// The rest of a builder's start, on the main thread: the order, the
	// place, the scene, then Papyrus places the actors.
	void LaunchThread(const ThreadBuilder& builder, std::int32_t threadID, bool withPlayer, const StartChoice& a_choice)
	{
		{
			// Out of the start flow: the usual time to be placed from here.
			std::scoped_lock lock(g_threadsLock);
			if (const auto record = ThreadRecordLocked(threadID)) {
				record->flow = 0;
				record->since = std::chrono::steady_clock::now();
			}
		}
		auto cast = builder.actors;
		auto starting = builder.starting;
		// As OStim's: with starting scenes the caller's order is the role
		// order; if the first doesn't fit the actors in that order, a start
		// scene is picked instead (ThreadStarter.cpp).
		if (!starting.empty()) {
			const auto first = SceneRegistry::Find(starting.front().scene);
			if (!first || !SceneRegistry::Fits(first->actors, ConditionsOf(cast))) {
				REX::WARN("ThreadBuilder: thread {}: \"{}\" doesn't fit its actors in the order given; picking a start scene instead", threadID, starting.front().scene);
				starting.clear();
			}
		}
		if (starting.empty() && !a_choice.sorted) {
			SortLikeOStim(cast, builder.dominant);
		}

		// Where: the furniture given, else (unless NoFurniture) the nearest
		// that has a scene for them (with a starting scene: one that takes
		// it), as OStim's thread starters.
		std::optional<Furniture::Found> piece;
		if (builder.furniture) {
			if (const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(builder.furniture)) {
				const auto type = Furniture::Classify(ref);
				if (type.empty()) {
					REX::WARN("ThreadBuilder: {:08X} isn't furniture of any type, starting without it", builder.furniture);
				} else if (FurnitureBlocked(builder.furniture) || std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a) { return a.furnitureRef == builder.furniture; })) {
					REX::WARN("ThreadBuilder: {:08X} is in use, starting without it", builder.furniture);
				} else {
					piece = Furniture::Found{ type, {}, builder.furniture, 0.0F, std::nullopt };
				}
			}
		} else if (a_choice.placeDecided) {
			piece = a_choice.piece;  // the start flow's choice
		} else if (!builder.noFurniture && g_settings.useFurniture) {
			const auto firstActor = RE::TESForm::GetFormByID<RE::Actor>(cast.front());
			const auto firstScene = starting.empty() ? nullptr : SceneRegistry::Find(starting.front().scene);
			if (firstActor) {
				for (const auto& found : FurnitureNear(firstActor->data.location, firstActor)) {
					if (firstScene ? (!firstScene->furniture.empty() && SceneFitsFurniture(*firstScene, found.type)) : !ScenesForActors(cast, found.type).empty()) {
						piece = found;
						break;
					}
				}
			}
		}
		const std::string here = piece ? piece->type : std::string{};

		std::shared_ptr<const SceneRegistry::Scene> scene;
		std::vector<std::int32_t>                   ordered;
		if (!starting.empty()) {
			scene = SceneRegistry::Find(starting.front().scene);
			for (const auto id : cast) {
				ordered.push_back(static_cast<std::int32_t>(id));
			}
		} else {
			std::string why;
			scene = PickStartScene(ScenesForActors(cast, here, a_choice.pin), here, withPlayer, why);
			if (!scene && piece) {
				piece.reset();  // nothing fits there: right here instead
				scene = PickStartScene(ScenesForActors(cast, {}, a_choice.pin), {}, withPlayer, why);
			}
			if (!scene) {
				DropStartingThread(threadID, std::format("no scene for {} actor(s){}", cast.size(), here.empty() ? std::string{} : " on " + here));
				return;
			}
			// Who takes which role.
			const auto order = SceneRegistry::AssignRoles(scene->actors, ConditionsOf(cast), a_choice.pin);
			if (order.size() != cast.size()) {
				DropStartingThread(threadID, std::format("its actors don't fit \"{}\"'s roles", scene->id));
				return;
			}
			std::vector<std::uint32_t> roleOrder;
			for (const auto index : order) {
				ordered.push_back(static_cast<std::int32_t>(cast[index]));
				roleOrder.push_back(cast[index]);
			}
			cast = std::move(roleOrder);
		}
		{
			std::scoped_lock lock(g_threadsLock);
			if (const auto record = ThreadRecordLocked(threadID)) {
				record->roleActors = cast;
			}
		}

		if (piece) {
			PendingFurniture pending{ piece->ref, piece->type, ActorSlots<int>::Key(cast) };
			pending.offset = scene->furnitureOffset;
			pending.floorScene = scene->furniture.empty();
			pending.fixedSpot = piece->spot;
			SetPendingFurniture(std::move(pending));
		}
		// Starting scenes after the first: a sequence, as OStim's starting
		// nodes. An entry without a time plays for its scene's length;
		// navigateTo puts in the way there from the entry before. One
		// starting scene is a plain start, as OStim's (no sequence, no
		// sequence end).
		if (starting.size() > 1) {
			const auto conditions = ConditionsOf(cast);
			std::vector<SceneRegistry::SequenceEntry> entries;
			for (std::size_t i = 0; i < starting.size(); ++i) {
				auto       entry = starting[i];
				const auto at = SceneRegistry::Find(entry.scene);
				if (!at || at->actors.size() != cast.size()) {
					REX::WARN("ThreadBuilder: thread {}: starting scene \"{}\" left out (no such scene for {} actor(s))", threadID, entry.scene, cast.size());
					continue;
				}
				if (i > 0 && i < builder.navigateTo.size() && builder.navigateTo[i] && !entries.empty()) {
					const auto from = SceneRegistry::Find(entries.back().scene);
					if (from && from->id != at->id) {
						auto route = RouteBetween(from, at, conditions, g_autoConfig.maxSteps);
						if (!route.empty()) {
							route.pop_back();  // the entry itself, with its own time, below
							entries.insert(entries.end(), route.begin(), route.end());
						}
					}
				}
				if (entry.duration <= 0.0F) {
					entry.duration = at->length > 0.0F ? at->length : STARTING_NO_LENGTH;
				}
				entries.push_back(std::move(entry));
			}
			if (entries.size() > 1) {
				auto sequence = std::make_shared<SceneRegistry::Sequence>();
				sequence->id = std::format("thread {}", threadID);
				sequence->actorCount = cast.size();
				sequence->entries = std::move(entries);
				QueueSequence(std::move(sequence), cast);
			}
		}
		REX::INFO("ThreadBuilder: thread {} starting \"{}\"{}", threadID, scene->id, here.empty() ? std::string{} : " on " + here);
		if (g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			const std::int32_t anchor = withPlayer ? 0 : static_cast<std::int32_t>(cast.front());
			Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "StartPickedCast"sv, callback, scene->id, ordered, anchor);
		} else {
			DropStartingThread(threadID, "no script engine");
		}
	}

	// OStim's PlayerThreadStarter: a player thread from a script waits until
	// the player is out of dialogue, then asks (as OStim's message boxes, in
	// the picker) about furniture, adding actors near and the player's role,
	// as the settings say, and sorts the actors with PlayerAlways...; then
	// LaunchThread. Each step on the main thread.
	struct StartFlow
	{
		ThreadBuilder builder;
		std::int32_t  threadID = 0;
		std::uint64_t id = 0;
		StartChoice   choice;
	};
	using StartFlowPtr = std::shared_ptr<StartFlow>;
	std::atomic<std::uint64_t> g_nextStartFlow = 1;

	// Whether a_flow's thread is still waiting for it (not stopped, the game
	// not loaded since).
	bool FlowAlive(const StartFlow& a_flow)
	{
		std::scoped_lock lock(g_threadsLock);
		const auto record = ThreadRecordLocked(a_flow.threadID);
		return record && record->status == ThreadRecord::Status::kStarting && record->flow == a_flow.id;
	}

	// OStim's position names (PlayerThreadStarter.cpp).
	std::string PositionName(std::size_t a_index)
	{
		constexpr std::array names{ "Dom", "Sub", "Third", "Fourth", "Fifth", "Sixth", "Seventh", "Eighth", "Ninth" };
		return a_index < names.size() ? std::string(names[a_index]) : std::format("{}", a_index + 1);
	}

	void FlowLaunch(const StartFlowPtr& a_flow)
	{
		if (FlowAlive(*a_flow)) {
			LaunchThread(a_flow->builder, a_flow->threadID, true, a_flow->choice);
		}
	}

	// OStim's sortActors: the order, then the player moved to the position
	// picked (a_playerIndex), or for two, where PlayerAlways... says.
	void FlowSort(const StartFlowPtr& a_flow, int a_playerIndex)
	{
		auto&      cast = a_flow->builder.actors;
		const auto player = RE::PlayerCharacter::GetSingleton()->GetFormID();
		SortLikeOStim(cast, a_flow->builder.dominant);
		const int want = a_playerIndex >= 0 ? a_playerIndex : PlayerAlwaysIndex(cast);
		if (const auto at = std::ranges::find(cast, player); at != cast.end() && want >= 0 && static_cast<std::size_t>(want) < cast.size()) {
			cast.erase(at);
			cast.insert(cast.begin() + want, player);
			a_flow->choice.pin = static_cast<std::size_t>(want);
		}
		a_flow->choice.sorted = true;
		FlowLaunch(a_flow);
	}

	// OStim's handleActorSorting: asks the player's position, by
	// playerSelectRole... (for two: the same or not by penis, as OStim's).
	void FlowRole(const StartFlowPtr& a_flow)
	{
		if (!FlowAlive(*a_flow)) {
			return;
		}
		const auto& cast = a_flow->builder.actors;
		if (!a_flow->builder.starting.empty()) {
			FlowLaunch(a_flow);
			return;
		}
		bool select = false;
		if (cast.size() >= 3) {
			select = g_settings.playerSelectRoleThreesome;
		} else if (cast.size() == 2) {
			select = ActorProperties::Cached(cast[0]).penis == ActorProperties::Cached(cast[1]).penis ? g_settings.playerSelectRoleGay : g_settings.playerSelectRoleStraight;
		}
		if (!select) {
			FlowSort(a_flow, -1);
			return;
		}
		Question question;
		question.title = TR("$4stim_message_select_position", "Which position do you want to be in?");
		for (std::size_t i = 0; i < cast.size(); ++i) {
			question.answers.push_back({ PositionName(i), {} });
		}
		question.then = [a_flow](int a_index) { FlowSort(a_flow, a_index); };
		Ask(std::move(question));
	}

	// Whether any scene has a_count actors and plays on a_furniture ("" = off
	// furniture): OStim's GraphTable::hasNodes.
	bool HasScenesFor(std::size_t a_count, const std::string& a_furniture)
	{
		for (const auto& summary : SceneRegistry::List(a_count)) {
			const auto scene = SceneRegistry::Find(summary.id);
			if (scene && !scene->IsTransition() && SceneFitsFurniture(*scene, a_furniture)) {
				return true;
			}
		}
		return false;
	}

	// OStim's handleActorAdding (addActorsAtStart): "Add actor?" with the
	// eligible actors near the player, again after each one added, while
	// some scene takes one more on the place chosen.
	void FlowAddActors(const StartFlowPtr& a_flow)
	{
		if (!FlowAlive(*a_flow)) {
			return;
		}
		const auto& cast = a_flow->builder.actors;
		if (!a_flow->builder.starting.empty()) {
			FlowLaunch(a_flow);
			return;
		}
		const std::string here = a_flow->choice.piece ? a_flow->choice.piece->type : std::string{};
		if (!g_settings.addActorsAtStart || !HasScenesFor(cast.size() + 1, here)) {
			FlowRole(a_flow);
			return;
		}
		std::vector<Candidate> near;
		for (auto& c : FindCandidates(2000.0F)) {  // OStim's 2000
			if (std::ranges::find(cast, c.id) == cast.end() && CantUse(RE::TESForm::GetFormByID<RE::Actor>(c.id)).empty()) {
				near.push_back(std::move(c));
			}
		}
		if (near.empty()) {
			FlowRole(a_flow);
			return;
		}
		Question question;
		question.title = TR("$4stim_message_add_actor", "Add actor?");
		question.answers.push_back({ TR("$4stim_message_none", "None"), {} });
		for (const auto& c : near) {
			const char* sex = c.sex == SceneRegistry::Sex::kMale ? " (M)" : c.sex == SceneRegistry::Sex::kFemale ? " (F)" : "";
			question.answers.push_back({ c.name + sex, TRF("$4stim_meters_away", "{:.0f} m away", c.distance / 70.0F) });
		}
		question.then = [a_flow, near](int a_index) {
			if (a_index < 1 || static_cast<std::size_t>(a_index) > near.size() || !FlowAlive(*a_flow)) {
				FlowRole(a_flow);
				return;
			}
			const auto id = near[a_index - 1].id;
			if (!CantUse(RE::TESForm::GetFormByID<RE::Actor>(id)).empty()) {
				FlowAddActors(a_flow);  // busy since: ask again
				return;
			}
			a_flow->builder.actors.push_back(id);
			REX::INFO("ThreadBuilder: thread {}: {:08X} added", a_flow->threadID, id);
			{
				std::scoped_lock lock(g_threadsLock);
				if (const auto record = ThreadRecordLocked(a_flow->threadID)) {
					record->actors = ActorSlots<int>::Key(a_flow->builder.actors);
					record->roleActors = a_flow->builder.actors;
				}
			}
			FlowAddActors(a_flow);
		};
		Ask(std::move(question));
	}

	// OStim's handleFurniture. With a starting scene, OStim asks about the
	// nearest bed whatever the scene; 4Stim asks about the nearest furniture
	// the starting scene plays on (none for a floor scene), as its other
	// starts pick it.
	void FlowFurniture(const StartFlowPtr& a_flow)
	{
		if (!FlowAlive(*a_flow)) {
			return;
		}
		// As OStim's startThread, before the flow: starting scenes their
		// actors don't fit in the order given are dropped.
		if (auto& starting = a_flow->builder.starting; !starting.empty()) {
			const auto scene = SceneRegistry::Find(starting.front().scene);
			if (!scene || !SceneRegistry::Fits(scene->actors, ConditionsOf(a_flow->builder.actors))) {
				REX::WARN("ThreadBuilder: thread {}: \"{}\" doesn't fit its actors in the order given; picking a start scene instead", a_flow->threadID, starting.front().scene);
				starting.clear();
				a_flow->builder.navigateTo.clear();
			}
		}
		const auto& b = a_flow->builder;
		const auto  first = RE::TESForm::GetFormByID<RE::Actor>(b.actors.front());
		if (b.noFurniture || b.furniture || !g_settings.useFurniture || !first) {
			FlowAddActors(a_flow);
			return;
		}
		a_flow->choice.placeDecided = true;
		if (!b.starting.empty()) {
			const auto                      scene = SceneRegistry::Find(b.starting.front().scene);
			std::optional<Furniture::Found> piece;
			if (scene && !scene->furniture.empty()) {
				for (const auto& found : FurnitureNear(first->data.location, first)) {
					if (SceneFitsFurniture(*scene, found.type)) {
						piece = found;
						break;
					}
				}
			}
			if (!piece) {
				FlowAddActors(a_flow);
				return;
			}
			if (!g_settings.selectFurniture) {
				a_flow->choice.piece = piece;
				FlowAddActors(a_flow);
				return;
			}
			Question question;
			question.title = TRF("$4stim_message_use_furniture", "Do you want to use the nearby {}?", piece->name);
			question.answers.push_back({ TR("$4stim_message_yes", "Yes"), {} });
			question.answers.push_back({ TR("$4stim_message_no", "No"), {} });
			question.then = [a_flow, piece](int a_index) {
				if (a_index == 0) {
					a_flow->choice.piece = piece;
					FlowAddActors(a_flow);
				} else {
					FlowRole(a_flow);
				}
			};
			Ask(std::move(question));
			return;
		}
		// OStim's selectFurniture: the furniture near that some scene for
		// these actors plays on; asked (SetSelectFurniture), else the nearest.
		std::vector<Furniture::Found> found;
		for (auto& f : FurnitureNear(first->data.location, first)) {
			if (!ScenesForActors(b.actors, f.type).empty()) {
				found.push_back(std::move(f));
			}
		}
		if (found.empty()) {
			FlowAddActors(a_flow);
			return;
		}
		if (!g_settings.selectFurniture) {
			a_flow->choice.piece = found.front();
			FlowAddActors(a_flow);
			return;
		}
		Question question;
		question.title = TR("$4stim_message_select_furniture", "Which furniture do you want to use?");
		question.answers.push_back({ TR("$4stim_message_none", "None"), {} });
		for (const auto& f : found) {
			question.answers.push_back({ f.name, TRF("$4stim_meters_away", "{:.0f} m away", f.distance / 70.0F) });
		}
		question.then = [a_flow, found](int a_index) {
			if (a_index >= 1 && static_cast<std::size_t>(a_index) <= found.size()) {
				a_flow->choice.piece = found[a_index - 1];
			}
			FlowAddActors(a_flow);
		};
		Ask(std::move(question));
	}

	// OStim's startPlayerThread: waits (every 250 ms) while the player is in
	// dialogue, as scripts start scenes from a topic's end, while it's
	// still closing.
	void StartPlayerFlow(StartFlowPtr a_flow)
	{
		// Every 250 ms while the player is in dialogue (no limit, as before).
		Scheduler::WhileThenOnMain(
			std::chrono::milliseconds(250), std::numeric_limits<int>::max(),
			[a_flow, logged = false]() mutable {
				const bool waiting = Compat::CanCheckDialogue() && RE::IsPlayerInDialogue() && FlowAlive(*a_flow);
				if (waiting && !logged) {
					REX::INFO("ThreadBuilder: thread {} waits for the dialogue to end", a_flow->threadID);
					logged = true;
				}
				return waiting;
			},
			std::chrono::milliseconds(0), [a_flow]() { FlowFurniture(a_flow); });
	}

	// Starts a scene from a builder (OStim's Start). Returns the thread ID
	// (0 for the player's), or -1. As OStim's, the thread exists from here
	// on (FourThread.IsRunning, metadata, GetActors...), while Papyrus places
	// the actors a moment later; changes that need the scene playing (speed,
	// navigation, stop) are done once it is. A start that fails after this
	// (no scene fits, Papyrus gives up) drops the thread and logs why.
	std::int32_t StartThread(ThreadBuilder a_builder)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		bool       withPlayer = false;
		for (std::size_t i = 0; i < a_builder.actors.size(); ++i) {
			const auto id = a_builder.actors[i];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (const auto why = CantUse(actor); !why.empty()) {
				REX::WARN("ThreadBuilder.Start: {:08X} can't be in a scene ({})", id, why);
				return -1;
			}
			if (std::ranges::find(a_builder.actors.begin(), a_builder.actors.begin() + static_cast<std::ptrdiff_t>(i), id) != a_builder.actors.begin() + static_cast<std::ptrdiff_t>(i)) {
				REX::WARN("ThreadBuilder.Start: {:08X} is given twice", id);  // as OStim's ThreadStarter
				return -1;
			}
			withPlayer = withPlayer || (player && id == player->GetFormID());
		}
		std::int32_t        threadID = 0;
		const std::uint64_t flowID = g_nextStartFlow++;
		{
			std::scoped_lock lock(g_threadsLock);
			PruneStartingLocked();
			if (withPlayer && g_threads.contains(0) && g_threads.at(0).status != ThreadRecord::Status::kEnded) {
				REX::WARN("ThreadBuilder.Start: the player's scene is already starting");
				return -1;
			}
			threadID = withPlayer ? 0 : g_nextThreadID++;
			ThreadRecord record;
			record.id = threadID;
			record.actors = ActorSlots<int>::Key(a_builder.actors);
			record.roleActors = a_builder.actors;
			record.duration = a_builder.duration;
			record.undressAtStart = a_builder.undress;
			record.noAutoMode = a_builder.noAutoMode;
			record.noPlayerControl = a_builder.noPlayerControl;
			record.noUndress = a_builder.noUndress;
			record.noPostDialogue = a_builder.noPostDialogue;
			record.endAfterSequence = a_builder.endAfterSequence;
			record.metadata = a_builder.metadata;
			if (withPlayer) {
				record.flow = flowID;
			}
			g_threads.insert_or_assign(threadID, std::move(record));
		}
		if (withPlayer) {
			auto flow = std::make_shared<StartFlow>();
			flow->builder = std::move(a_builder);
			flow->threadID = threadID;
			flow->id = flowID;
			StartPlayerFlow(std::move(flow));
			return threadID;
		}
		F4SE::GetTaskInterface()->AddTask([builder = std::move(a_builder), threadID]() { LaunchThread(builder, threadID, false, {}); });
		return threadID;
	}
}
