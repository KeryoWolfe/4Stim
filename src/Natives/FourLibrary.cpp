#include "Core/Core.h"

namespace Core
{
	// ---- OStim's OLibrary, OMetadata, OActor, OFurniture (docs/API.md) ----

	std::vector<std::uint32_t> IDsOf(const std::vector<RE::Actor*>& a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			if (actor) {
				ids.push_back(actor->GetFormID());
			}
		}
		return ids;
	}

	bool HasAnyOf(const std::vector<std::string>& a_have, const std::vector<std::string>& a_wanted)
	{
		return std::ranges::any_of(a_wanted, [&](const std::string& w) {
			return std::ranges::any_of(a_have, [&](const std::string& h) { return _stricmp(h.c_str(), w.c_str()) == 0; });
		});
	}

	bool HasAllOf(const std::vector<std::string>& a_have, const std::vector<std::string>& a_wanted)
	{
		return std::ranges::all_of(a_wanted, [&](const std::string& w) {
			return std::ranges::any_of(a_have, [&](const std::string& h) { return _stricmp(h.c_str(), w.c_str()) == 0; });
		});
	}

	// OStim's OLibrary.GetRandom...: a random scene these actors can play
	// in the order given (OStim's), on a_furniture ("" = off furniture), with
	// the scene tags, actions and (for role a_position, or -1: any role)
	// actor tags asked for; empty lists ask nothing. Not transitions or
	// scenes marked noRandomSelection, as OStim. "" if none.
	std::string RandomScene(const std::vector<RE::Actor*>& a_actors, const std::string& a_furniture, const std::vector<std::string>& a_anyTags,
		const std::vector<std::string>& a_allTags, const std::vector<std::string>& a_anyActions, const std::vector<std::string>& a_allActions,
		std::int32_t a_position, const std::vector<std::string>& a_anyActorTags, const std::vector<std::string>& a_allActorTags)
	{
		const auto cast = IDsOf(a_actors);
		if (cast.empty()) {
			return {};
		}
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
		for (const auto& scene : ScenesForActors(cast, LowerCopy(a_furniture))) {
			// As OStim's Node::fulfilledBy: the actors in the order given.
			if (scene->IsTransition() || scene->noRandomSelection || !SexesFit(*scene, cast)) {
				continue;
			}
			if ((!a_anyTags.empty() && !HasAnyOf(scene->tags, a_anyTags)) || !HasAllOf(scene->tags, a_allTags)) {
				continue;
			}
			if (!a_anyActions.empty() && std::ranges::none_of(a_anyActions, [&](const std::string& t) { return scene->HasAction(t); })) {
				continue;
			}
			if (!std::ranges::all_of(a_allActions, [&](const std::string& t) { return scene->HasAction(t); })) {
				continue;
			}
			if (!a_anyActorTags.empty() || !a_allActorTags.empty()) {
				bool fits = false;
				for (std::size_t role = 0; role < scene->actors.size() && !fits; ++role) {
					if (a_position >= 0 && static_cast<std::size_t>(a_position) != role) {
						continue;
					}
					const auto& tags = scene->actors[role].tags;
					fits = (a_anyActorTags.empty() || HasAnyOf(tags, a_anyActorTags)) && HasAllOf(tags, a_allActorTags);
				}
				if (!fits) {
					continue;
				}
			}
			pool.push_back(scene);
		}
		if (pool.empty()) {
			return {};
		}
		static std::mt19937 rng{ std::random_device{}() };
		return pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)]->id;
	}

	// ---- FourLibrary queries beyond FindRandomScene (OStim's PapyrusLibrary) ----
	// OStim builds a condition per OLibrary function and picks a random node
	// that fits the actors and meets it (GraphTable::getRandomNode: no
	// transitions, nothing marked noRandomSelection). These are the same
	// conditions over 4Stim's scenes, behind four natives the OLibrary-named
	// Papyrus functions wrap.

	// OStim's StringUtil::toTagVector (lowercase, split on ',') and
	// toTagMatrix (lists split on ';', one per actor; empty lists keep their
	// place). Items are trimmed of spaces, which OStim doesn't do.
	std::vector<std::string> TagList(const std::string& a_csv) { return SplitCSV(LowerCopy(a_csv)); }

	std::vector<std::vector<std::string>> TagMatrix(const std::string& a_text)
	{
		std::vector<std::vector<std::string>> out;
		std::string                           part;
		for (const char ch : a_text + ";") {
			if (ch == ';') {
				out.push_back(TagList(part));
				part.clear();
			} else {
				part += ch;
			}
		}
		return out;
	}

	// OStim's VectorUtil::stoim: lists of ints, ';' between lists, ',' within.
	std::vector<std::vector<int>> IntMatrix(const std::string& a_text)
	{
		std::vector<std::vector<int>> out;
		if (a_text.empty()) {
			return out;
		}
		for (const auto& list : TagMatrix(a_text)) {
			std::vector<int> ints;
			for (const auto& item : list) {
				try {
					ints.push_back(std::stoi(item));
				} catch (...) {
				}
			}
			out.push_back(std::move(ints));
		}
		return out;
	}

	bool RoleHasAnyTag(const SceneRegistry::Scene& a_scene, int a_role, const std::vector<std::string>& a_tags)
	{
		return a_role >= 0 && static_cast<std::size_t>(a_role) < a_scene.actors.size() && HasAnyOf(a_scene.actors[a_role].tags, a_tags);
	}

	bool RoleHasAllTags(const SceneRegistry::Scene& a_scene, int a_role, const std::vector<std::string>& a_tags)
	{
		return a_role >= 0 && static_cast<std::size_t>(a_role) < a_scene.actors.size() && HasAllOf(a_scene.actors[a_role].tags, a_tags);
	}

	// OStim's Node::hasOnlyListedActorTags: true past the last role.
	bool RoleHasOnlyTags(const SceneRegistry::Scene& a_scene, int a_role, const std::vector<std::string>& a_tags)
	{
		if (a_role < 0 || static_cast<std::size_t>(a_role) >= a_scene.actors.size()) {
			return true;
		}
		return HasAllOf(a_tags, a_scene.actors[a_role].tags);
	}

	bool ForAnyRole(std::size_t a_count, const std::function<bool(int)>& a_test)
	{
		for (std::size_t i = 0; i < a_count; ++i) {
			if (a_test(static_cast<int>(i))) {
				return true;
			}
		}
		return false;
	}

	bool ForAllRoles(std::size_t a_count, const std::function<bool(int)>& a_test)
	{
		for (std::size_t i = 0; i < a_count; ++i) {
			if (!a_test(static_cast<int>(i))) {
				return false;
			}
		}
		return true;
	}

	int RoleIndex(std::size_t a_role) { return a_role == SceneRegistry::SceneAction::kNoRole ? -1 : static_cast<int>(a_role); }

	// OStim's Action::isType (by the alias-resolved id).
	bool ActionIsType(const SceneRegistry::SceneAction& a_action, const std::string& a_type)
	{
		return a_action.type->id == Actions::ResolveAlias(a_type);
	}

	// A scene fitting the actors (and furniture) that meets a_condition, at
	// random, as OStim's randomScene.
	std::string RandomSceneWhere(const std::vector<RE::Actor*>& a_actors, const std::string& a_furniture,
		const std::function<bool(const SceneRegistry::Scene&)>& a_condition)
	{
		const auto cast = IDsOf(a_actors);
		if (cast.empty()) {
			return {};
		}
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> pool;
		for (const auto& scene : ScenesForActors(cast, LowerCopy(a_furniture))) {
			if (!scene->IsTransition() && !scene->noRandomSelection && SexesFit(*scene, cast) && a_condition(*scene)) {  // in the order given, as OStim's
				pool.push_back(scene);
			}
		}
		if (pool.empty()) {
			return {};
		}
		static std::mt19937 rng{ std::random_device{}() };
		return pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)]->id;
	}
}

namespace Core::Natives::FourLibrary
{
	// OLibrary's (Any|All)MultiActorTag(s)For(Any|All) and the scene tag
	// combinations of them. a_actorTags: one list per role ("a,b;c"), each
	// role needing any (a_allPerRole false) or all of its list; for any role
	// or all of them (a_forAll). a_sceneMode: 0 no scene tags, 1 any of
	// a_sceneTags, 2 all of them.
	std::string FindRandomSceneByRoleTags(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::string a_actorTags,
		bool a_allPerRole, bool a_forAll, std::string a_sceneTags, std::int32_t a_sceneMode)
	{
		const auto matrix = TagMatrix(a_actorTags);
		const auto sceneTags = TagList(a_sceneTags);
		return RandomSceneWhere(a_actors, a_furniture, [&](const SceneRegistry::Scene& a_scene) {
			if (a_sceneMode == 1 && !HasAnyOf(a_scene.tags, sceneTags)) {
				return false;
			}
			if (a_sceneMode == 2 && !HasAllOf(a_scene.tags, sceneTags)) {
				return false;
			}
			const auto test = [&](int i) {
				return a_allPerRole ? RoleHasAllTags(a_scene, i, matrix[i]) : RoleHasAnyTag(a_scene, i, matrix[i]);
			};
			return a_forAll ? ForAllRoles(matrix.size(), test) : ForAnyRole(matrix.size(), test);
		});
	}

	// OLibrary's ...Action(s)ForActor / ForTarget / ForActorAndTarget: an
	// action of a type (any of a_types, or each of them with a_all) done by
	// role a_actor (when a_byActor) to role a_target (when a_byTarget), as
	// OStim's Node::findActionForActor and its siblings.
	std::string FindRandomSceneByActionRoles(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_types,
		bool a_all, bool a_byActor, std::int32_t a_actor, bool a_byTarget, std::int32_t a_target)
	{
		for (auto& type : a_types) {
			type = LowerCopy(type);
		}
		return RandomSceneWhere(a_actors, a_furniture, [&](const SceneRegistry::Scene& a_scene) {
			const auto has = [&](const std::string& a_type) {
				return std::ranges::any_of(a_scene.actions, [&](const SceneRegistry::SceneAction& a) {
					return (!a_byActor || RoleIndex(a.actor) == a_actor) && (!a_byTarget || RoleIndex(a.target) == a_target) && ActionIsType(a, a_type);
				});
			};
			return a_all ? std::ranges::all_of(a_types, has) : std::ranges::any_of(a_types, has);
		});
	}

	// args[a_first..a_first+7]: types, actors, targets, performers, mates any,
	// mates all, participants any, participants all.
	ActionRoleLists ActionListsFrom(const std::vector<std::string>& a_args, std::size_t a_first)
	{
		ActionRoleLists lists;
		lists.types = TagList(a_args[a_first]);
		lists.actors = IntMatrix(a_args[a_first + 1]);
		lists.targets = IntMatrix(a_args[a_first + 2]);
		lists.performers = IntMatrix(a_args[a_first + 3]);
		lists.matesAny = IntMatrix(a_args[a_first + 4]);
		lists.matesAll = IntMatrix(a_args[a_first + 5]);
		lists.participantsAny = IntMatrix(a_args[a_first + 6]);
		lists.participantsAll = IntMatrix(a_args[a_first + 7]);
		return lists;
	}

	// OLibrary.GetRandomSceneSuperloadCSV: a_args holds its 44 strings after
	// FurnitureType, in OStim's order (FourLibrary.psc packs them).
	std::string FindRandomSceneSuperload(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_args)
	{
		if (a_args.size() < 44) {
			REX::WARN("FourLibrary.GetRandomSceneSuperloadCSV: {} of its 44 filter strings", a_args.size());
			a_args.resize(44);
		}
		std::vector<std::function<bool(const SceneRegistry::Scene&)>> conditions;
		const auto& a = a_args;
		if (!a[0].empty()) {
			conditions.push_back([tags = TagList(a[0])](const SceneRegistry::Scene& s) { return HasAnyOf(s.tags, tags); });
		}
		if (!a[1].empty()) {
			conditions.push_back([tags = TagList(a[1])](const SceneRegistry::Scene& s) { return HasAllOf(s.tags, tags); });
		}
		if (!a[2].empty()) {  // OStim's Node::hasOnlyTags
			conditions.push_back([tags = TagList(a[2])](const SceneRegistry::Scene& s) { return HasAllOf(tags, s.tags); });
		}
		if (!a[3].empty()) {
			conditions.push_back([tags = TagList(a[3])](const SceneRegistry::Scene& s) { return !HasAnyOf(s.tags, tags); });
		}
		// Actor tags: any for any, any for all, all for any, all for all,
		// whitelist for any / all, blacklist for any / all.
		const auto roleTags = [&](std::size_t a_index, auto a_test, bool a_forAll) {
			if (a[a_index].empty()) {
				return;
			}
			conditions.push_back([m = TagMatrix(a[a_index]), a_test, a_forAll](const SceneRegistry::Scene& s) {
				const auto each = [&](int i) { return a_test(s, i, m[i]); };
				return a_forAll ? ForAllRoles(m.size(), each) : ForAnyRole(m.size(), each);
			});
		};
		const auto anyTag = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return RoleHasAnyTag(s, i, t); };
		const auto allTags = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return RoleHasAllTags(s, i, t); };
		const auto onlyTags = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return RoleHasOnlyTags(s, i, t); };
		const auto noTag = [](const SceneRegistry::Scene& s, int i, const std::vector<std::string>& t) { return !RoleHasAnyTag(s, i, t); };
		roleTags(4, anyTag, false);
		roleTags(5, anyTag, true);
		roleTags(6, allTags, false);
		roleTags(7, allTags, true);
		roleTags(8, onlyTags, false);
		roleTags(9, onlyTags, true);
		roleTags(10, noTag, false);
		roleTags(11, noTag, true);
		if (!a[12].empty()) {
			conditions.push_back([l = ActionListsFrom(a, 12)](const SceneRegistry::Scene& s) { return l.SceneHasAny(s); });
		}
		if (!a[20].empty()) {
			conditions.push_back([l = ActionListsFrom(a, 20)](const SceneRegistry::Scene& s) { return l.SceneHasAll(s); });
		}
		if (!a[28].empty()) {
			conditions.push_back([l = ActionListsFrom(a, 28)](const SceneRegistry::Scene& s) { return l.SceneHasOnlyListed(s); });
		}
		if (!a[36].empty()) {
			conditions.push_back([l = ActionListsFrom(a, 36)](const SceneRegistry::Scene& s) { return !l.SceneHasAny(s); });
		}
		return RandomSceneWhere(a_actors, a_furniture, [&](const SceneRegistry::Scene& a_scene) {
			return std::ranges::all_of(conditions, [&](const auto& c) { return c(a_scene); });
		});
	}

	// OLibrary.GetScenesInRange (OStim's Node::getNodesInRange): the scenes
	// reachable from a_id in up to a_distance navigations whose scenes (any
	// transitions on the way included) fit a_actors, sorted by name. As in
	// OStim, a_id itself is only in it if a navigation leads back to it.
	std::vector<std::string> GetScenesInRange(std::monostate, std::string a_id, std::vector<RE::Actor*> a_actors, std::int32_t a_distance)
	{
		const auto start = SceneRegistry::Find(a_id);
		const auto cast = IDsOf(a_actors);
		if (!start || cast.empty()) {
			return {};
		}
		SceneRegistry::ListFilter filter;
		filter.actors = ConditionsOf(cast);
		std::unordered_set<std::string> fits;
		for (const auto& summary : SceneRegistry::List(cast.size(), filter)) {
			fits.insert(LowerCopy(summary.id));
		}
		// Where a navigation to a_to ends (past its transitions), or null
		// when something on the way doesn't fit the actors.
		const auto destination = [&](const std::string& a_to) -> std::shared_ptr<const SceneRegistry::Scene> {
			auto scene = SceneRegistry::Find(a_to);
			for (int hops = 0; scene && hops < 16; ++hops) {
				if (!fits.contains(LowerCopy(scene->id))) {
					return nullptr;
				}
				if (!scene->IsTransition()) {
					return scene;
				}
				scene = SceneRegistry::Find(scene->destination);
			}
			return nullptr;
		};
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> found;
		std::vector<std::shared_ptr<const SceneRegistry::Scene>> level{ start };
		for (int i = 0; i < a_distance && !level.empty(); ++i) {
			std::vector<std::shared_ptr<const SceneRegistry::Scene>> next;
			for (const auto& scene : level) {
				for (const auto& nav : scene->navigations) {
					const auto to = destination(nav.to);
					if (to && std::ranges::find(found, to) == found.end()) {
						found.push_back(to);
						next.push_back(to);
					}
				}
			}
			level = std::move(next);
		}
		std::ranges::sort(found, [](const auto& x, const auto& y) { return LowerCopy(x->name) < LowerCopy(y->name); });
		std::vector<std::string> ids;
		for (const auto& scene : found) {
			ids.push_back(scene->id);
		}
		return ids;
	}

	// FourLibrary: one native the OLibrary-named functions wrap.
	std::string FindRandomScene(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_furniture, std::vector<std::string> a_anyTags,
		std::vector<std::string> a_allTags, std::vector<std::string> a_anyActions, std::vector<std::string> a_allActions, std::int32_t a_position,
		std::vector<std::string> a_anyActorTags, std::vector<std::string> a_allActorTags)
	{
		return RandomScene(a_actors, a_furniture, a_anyTags, a_allTags, a_anyActions, a_allActions, a_position, a_anyActorTags, a_allActorTags);
	}

	std::vector<std::string> SplitCSV(std::monostate, std::string a_csv) { return Core::SplitCSV(a_csv); }

	std::vector<std::string> GetAllScenes(std::monostate)
	{
		std::vector<std::string> ids;
		for (std::size_t count = 1; count <= 16; ++count) {
			for (const auto& summary : SceneRegistry::List(count)) {
				ids.push_back(summary.id);
			}
		}
		return ids;
	}
}
