#include "Core/Core.h"

namespace Core::Natives::FourMetadata
{
	std::vector<std::string> ScenesToNames(std::monostate, std::vector<std::string> a_ids)
	{
		std::vector<std::string> out;
		for (const auto& id : a_ids) {
			const auto scene = SceneRegistry::Find(id);
			out.push_back(scene ? scene->name : std::string{});
		}
		return out;
	}

	// OStim's GetAnimationId: the "animation" of speed a_index ("" for 4Stim's
	// "idles" speeds, which have no such name).
	std::string GetAnimationId(std::monostate, std::string a_id, std::int32_t a_index)
	{
		const auto scene = SceneRegistry::Find(a_id);
		if (!scene || a_index < 0 || static_cast<std::size_t>(a_index) >= scene->speedInfo.size()) {
			return {};
		}
		return scene->speedInfo[a_index].animation;
	}

	// 4Stim's: what the role at a_position needs (its own and its actions'
	// requirements), as OStim's actor condition reads them.
	std::vector<std::string> GetActorRequirements(std::monostate, std::string a_id, std::int32_t a_position)
	{
		const auto scene = SceneRegistry::Find(a_id);
		if (!scene || a_position < 0 || static_cast<std::size_t>(a_position) >= scene->actors.size()) {
			return {};
		}
		return scene->actors[a_position].requirements;
	}

	const SceneRegistry::SceneAction* ActionAt(const SceneRegistry::Scene* a_scene, std::int32_t a_index)
	{
		return a_scene && a_index >= 0 && static_cast<std::size_t>(a_index) < a_scene->actions.size() ? &a_scene->actions[a_index] : nullptr;
	}

	std::vector<std::string> GetActionTypes(std::monostate, std::string a_id)
	{
		std::vector<std::string> out;
		if (const auto scene = SceneRegistry::Find(a_id)) {
			for (const auto& action : scene->actions) {
				out.push_back(action.type->id);
			}
		}
		return out;
	}

	// a_which: 0 actor, 1 target, 2 performer.
	std::vector<std::int32_t> ActionRolesOf(const std::string& a_id, int a_which)
	{
		std::vector<std::int32_t> out;
		if (const auto scene = SceneRegistry::Find(a_id)) {
			for (const auto& action : scene->actions) {
				out.push_back(RoleIndex(a_which == 0 ? action.actor : a_which == 1 ? action.target : action.performer));
			}
		}
		return out;
	}
	std::vector<std::int32_t> GetActionActors(std::monostate, std::string a_id) { return ActionRolesOf(a_id, 0); }
	std::vector<std::int32_t> GetActionTargets(std::monostate, std::string a_id) { return ActionRolesOf(a_id, 1); }
	std::vector<std::int32_t> GetActionPerformers(std::monostate, std::string a_id) { return ActionRolesOf(a_id, 2); }

	std::vector<std::string> GetActionTags(std::monostate, std::string a_id, std::int32_t a_index)
	{
		const auto action = ActionAt(SceneRegistry::Find(a_id).get(), a_index);
		return action ? action->type->tags : std::vector<std::string>{};
	}

	// OStim's GetAllActionsTags: every action's tags, once each, sorted.
	std::vector<std::string> GetAllActionsTags(std::monostate, std::string a_id)
	{
		std::set<std::string> tags;
		if (const auto scene = SceneRegistry::Find(a_id)) {
			for (const auto& action : scene->actions) {
				tags.insert(action.type->tags.begin(), action.type->tags.end());
			}
		}
		return { tags.begin(), tags.end() };
	}

	// List helpers for FourMetadata.psc (case-insensitive, as Papyrus strings).
	bool ListHasAny(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted) { return HasAnyOf(a_have, a_wanted); }
	bool ListHasAll(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted) { return HasAllOf(a_have, a_wanted); }
	// What of a_have is in a_wanted, lowercase, once each, sorted (OStim's tag
	// overlaps return a std::set).
	std::vector<std::string> ListOverlap(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted)
	{
		std::set<std::string> out;
		for (const auto& h : a_have) {
			if (HasAnyOf(a_wanted, { h })) {
				out.insert(LowerCopy(h));
			}
		}
		return { out.begin(), out.end() };
	}

	// "1,2" -> [1, 2] / [1.0, 2.0] (OStim's VectorUtil::stoiv / stofv: bad items skipped).
	std::vector<std::int32_t> IntsOfCSV(std::monostate, std::string a_csv)
	{
		std::vector<std::int32_t> out;
		for (const auto& item : SplitCSV(a_csv)) {
			try {
				out.push_back(std::stoi(item));
			} catch (...) {
			}
		}
		return out;
	}
	std::vector<float> FloatsOfCSV(std::monostate, std::string a_csv)
	{
		std::vector<float> out;
		for (const auto& item : SplitCSV(a_csv)) {
			try {
				out.push_back(std::stof(item));
			} catch (...) {
			}
		}
		return out;
	}
	// OStim's VectorUtil::getOverlap: the items of a_have also in a_wanted, in a_have's order.
	std::vector<std::int32_t> IntListOverlap(std::monostate, std::vector<std::int32_t> a_have, std::vector<std::int32_t> a_wanted)
	{
		std::erase_if(a_have, [&](std::int32_t v) { return std::ranges::find(a_wanted, v) == a_wanted.end(); });
		return a_have;
	}
	std::vector<float> FloatListOverlap(std::monostate, std::vector<float> a_have, std::vector<float> a_wanted)
	{
		std::erase_if(a_have, [&](float v) { return std::ranges::find(a_wanted, v) == a_wanted.end(); });
		return a_have;
	}
	std::vector<std::string> StringListOverlap(std::monostate, std::vector<std::string> a_have, std::vector<std::string> a_wanted)
	{
		std::erase_if(a_have, [&](const std::string& v) { return !HasAnyOf(a_wanted, { v }); });
		return a_have;
	}

	// OMetadata's FindAction(s)For... families: the indexes of the actions of
	// any of a_types (all actions when a_types is empty) whose roles fit
	// a_mode, as OStim's conditions: 0 any roles; 1 actor in a_positions;
	// 2 target in; 3 performer in; 4 actor in a_positions and target in
	// a_positions2; 5 actor or target in (mates any); 6 actor and target in
	// (mates all); 7 actor, target or performer in (participants any); 8 all
	// three in (participants all).
	std::vector<std::int32_t> FindActionsBy(std::monostate, std::string a_id, std::vector<std::string> a_types, std::int32_t a_mode,
		std::vector<std::int32_t> a_positions, std::vector<std::int32_t> a_positions2)
	{
		std::vector<std::int32_t> out;
		const auto scene = SceneRegistry::Find(a_id);
		if (!scene) {
			return out;
		}
		for (auto& type : a_types) {
			type = LowerCopy(type);
		}
		const auto in = [](const std::vector<std::int32_t>& a_list, int a_role) { return std::ranges::find(a_list, a_role) != a_list.end(); };
		for (std::size_t i = 0; i < scene->actions.size(); ++i) {
			const auto& a = scene->actions[i];
			if (!a_types.empty() && std::ranges::none_of(a_types, [&](const std::string& t) { return ActionIsType(a, t); })) {
				continue;
			}
			const int actor = RoleIndex(a.actor);
			const int target = RoleIndex(a.target);
			const int performer = RoleIndex(a.performer);
			bool fits = true;
			switch (a_mode) {
			case 1: fits = in(a_positions, actor); break;
			case 2: fits = in(a_positions, target); break;
			case 3: fits = in(a_positions, performer); break;
			case 4: fits = in(a_positions, actor) && in(a_positions2, target); break;
			case 5: fits = in(a_positions, actor) || in(a_positions, target); break;
			case 6: fits = in(a_positions, actor) && in(a_positions, target); break;
			case 7: fits = in(a_positions, actor) || in(a_positions, target) || in(a_positions, performer); break;
			case 8: fits = in(a_positions, actor) && in(a_positions, target) && in(a_positions, performer); break;
			default: break;
			}
			if (fits) {
				out.push_back(static_cast<std::int32_t>(i));
			}
		}
		return out;
	}

	const Actions::Side& SideOf(const SceneRegistry::SceneAction& a_action, int a_role)
	{
		return a_role == 0 ? a_action.type->actor : a_role == 1 ? a_action.type->target : a_action.type->performer;
	}

	// OMetadata.FindActionsSuperloadCSVv2's search: a_filters holds its 25
	// strings after Id, in OStim's order (FourMetadata.psc packs them).
	std::vector<std::int32_t> FindActionsSuperload(std::monostate, std::string a_id, std::vector<std::string> a_filters)
	{
		std::vector<std::int32_t> out;
		const auto scene = SceneRegistry::Find(a_id);
		if (!scene) {
			return out;
		}
		a_filters.resize(std::max<std::size_t>(a_filters.size(), 25));
		const auto& f = a_filters;
		const auto ints = [](const std::string& a_csv) {
			std::vector<int> list;
			for (const auto& item : SplitCSV(a_csv)) {
				try {
					list.push_back(std::stoi(item));
				} catch (...) {
				}
			}
			return list;
		};
		const auto in = [](const std::vector<int>& a_list, int a_role) { return std::ranges::find(a_list, a_role) != a_list.end(); };
		std::vector<std::function<bool(const SceneRegistry::SceneAction&)>> conditions;
		// Roles: actor, target, performer, mates any / all, participants any / all.
		for (int k = 0; k < 7; ++k) {
			if (f[k].empty()) {
				continue;
			}
			conditions.push_back([k, list = ints(f[k]), in](const SceneRegistry::SceneAction& a) {
				const int actor = RoleIndex(a.actor);
				const int target = RoleIndex(a.target);
				const int performer = RoleIndex(a.performer);
				switch (k) {
				case 0: return in(list, actor);
				case 1: return in(list, target);
				case 2: return in(list, performer);
				case 3: return in(list, actor) || in(list, target);
				case 4: return in(list, actor) && in(list, target);
				case 5: return in(list, actor) || in(list, target) || in(list, performer);
				default: return in(list, actor) && in(list, target) && in(list, performer);
				}
			});
		}
		if (!f[7].empty()) {
			conditions.push_back([types = TagList(f[7])](const SceneRegistry::SceneAction& a) {
				return std::ranges::any_of(types, [&](const std::string& t) { return ActionIsType(a, t); });
			});
		}
		if (!f[8].empty()) {
			conditions.push_back([tags = TagList(f[8])](const SceneRegistry::SceneAction& a) { return HasAnyOf(a.type->tags, tags); });
		}
		if (!f[9].empty()) {
			conditions.push_back([tags = TagList(f[9])](const SceneRegistry::SceneAction& a) { return HasAllOf(a.type->tags, tags); });
		}
		if (!f[10].empty()) {  // OStim's hasOnlyTags
			conditions.push_back([tags = TagList(f[10])](const SceneRegistry::SceneAction& a) { return HasAllOf(tags, a.type->tags); });
		}
		if (!f[11].empty()) {
			conditions.push_back([tags = TagList(f[11])](const SceneRegistry::SceneAction& a) { return !HasAnyOf(a.type->tags, tags); });
		}
		// Custom records: any / all keys per side ("actor keys;target keys;performer keys"),
		// for ints, floats, strings, int lists, float lists, string lists.
		const auto hasKey = [](const Actions::Side& s, int a_kind, const std::string& k) {
			switch (a_kind) {
			case 0: return s.ints.contains(k);
			case 1: return s.floats.contains(k);
			case 2: return s.strings.contains(k);
			case 3: return s.intLists.contains(k);
			case 4: return s.floatLists.contains(k);
			default: return s.stringLists.contains(k);
			}
		};
		for (int kind = 0; kind < 6; ++kind) {
			for (int all = 0; all < 2; ++all) {
				const auto& text = f[12 + kind * 2 + all];
				if (text.empty()) {
					continue;
				}
				const auto matrix = TagMatrix(text);
				for (int role = 0; role < 3 && role < static_cast<int>(matrix.size()); ++role) {
					if (matrix[role].empty()) {
						continue;
					}
					conditions.push_back([keys = matrix[role], role, kind, all, hasKey](const SceneRegistry::SceneAction& a) {
						const auto& side = SideOf(a, role);
						const auto has = [&](const std::string& k) { return hasKey(side, kind, k); };
						return all ? std::ranges::all_of(keys, has) : std::ranges::any_of(keys, has);
					});
				}
			}
		}
		for (std::size_t i = 0; i < scene->actions.size(); ++i) {
			if (std::ranges::all_of(conditions, [&](const auto& c) { return c(scene->actions[i]); })) {
				out.push_back(static_cast<std::int32_t>(i));
			}
		}
		return out;
	}

	// Custom action data (OStim's GetCustomAction{Actor,Target,Performer}...):
	// a_role 0 actor, 1 target, 2 performer; a_kind 0 int, 1 float, 2 string,
	// 3 int list, 4 float list, 5 string list.
	const Actions::Side* CustomSide(const std::string& a_id, std::int32_t a_index, std::int32_t a_role)
	{
		const auto action = ActionAt(SceneRegistry::Find(a_id).get(), a_index);
		return action && a_role >= 0 && a_role <= 2 ? &SideOf(*action, a_role) : nullptr;
	}
	bool HasCustomAction(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::int32_t a_kind, std::string a_record)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto k = LowerCopy(a_record);
		if (!side) {
			return false;
		}
		switch (a_kind) {
		case 0: return side->ints.contains(k);
		case 1: return side->floats.contains(k);
		case 2: return side->strings.contains(k);
		case 3: return side->intLists.contains(k);
		case 4: return side->floatLists.contains(k);
		default: return side->stringLists.contains(k);
		}
	}
	std::int32_t GetCustomActionInt(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, std::int32_t a_fallback)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto it = side ? side->ints.find(LowerCopy(a_record)) : decltype(side->ints)::const_iterator{};
		return side && it != side->ints.end() ? it->second : a_fallback;
	}
	float GetCustomActionFloat(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, float a_fallback)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto it = side ? side->floats.find(LowerCopy(a_record)) : decltype(side->floats)::const_iterator{};
		return side && it != side->floats.end() ? it->second : a_fallback;
	}
	std::string GetCustomActionString(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record, std::string a_fallback)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto it = side ? side->strings.find(LowerCopy(a_record)) : decltype(side->strings)::const_iterator{};
		return side && it != side->strings.end() ? it->second : a_fallback;
	}
	std::vector<std::int32_t> GetCustomActionIntList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto it = side ? side->intLists.find(LowerCopy(a_record)) : decltype(side->intLists)::const_iterator{};
		return side && it != side->intLists.end() ? std::vector<std::int32_t>(it->second.begin(), it->second.end()) : std::vector<std::int32_t>{};
	}
	std::vector<float> GetCustomActionFloatList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto it = side ? side->floatLists.find(LowerCopy(a_record)) : decltype(side->floatLists)::const_iterator{};
		return side && it != side->floatLists.end() ? it->second : std::vector<float>{};
	}
	std::vector<std::string> GetCustomActionStringList(std::monostate, std::string a_id, std::int32_t a_index, std::int32_t a_role, std::string a_record)
	{
		const auto side = CustomSide(a_id, a_index, a_role);
		const auto it = side ? side->stringLists.find(LowerCopy(a_record)) : decltype(side->stringLists)::const_iterator{};
		return side && it != side->stringLists.end() ? it->second : std::vector<std::string>{};
	}
	std::int32_t GetCustomSceneActorInt(std::monostate, std::string a_id, std::int32_t a_position, std::string a_record, std::int32_t a_op, std::int32_t a_start)
	{
		return CustomSceneActorAggregate<std::int32_t>(a_id, a_position, a_record, a_op, a_start);
	}
	float GetCustomSceneActorFloat(std::monostate, std::string a_id, std::int32_t a_position, std::string a_record, std::int32_t a_op, float a_start)
	{
		return CustomSceneActorAggregate<float>(a_id, a_position, a_record, a_op, a_start);
	}

	// FourMetadata (OMetadata)
	std::string GetName(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->name : std::string{};
	}
	bool IsTransition(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene && scene->IsTransition();
	}
	std::string GetDestination(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->destination : std::string{};
	}
	std::vector<std::string> GetSceneTags(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->tags : std::vector<std::string>{};
	}
	std::string GetFurnitureType(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->furniture : std::string{};
	}
	std::int32_t GetMaxSpeed(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? static_cast<std::int32_t>(scene->speeds.size()) - 1 : 0;  // OStim's 0 for an unknown scene
	}
	std::int32_t GetDefaultSpeed(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->defaultSpeed : 0;  // OStim's 0 for an unknown scene
	}
	std::vector<std::string> GetNavigations(std::monostate, std::string a_id)
	{
		std::vector<std::string> out;
		if (const auto scene = SceneRegistry::Find(a_id)) {
			for (const auto& nav : scene->navigations) {
				out.push_back(nav.to);
			}
		}
		return out;
	}
	std::string GetAutoTransition(std::monostate, std::string a_id, std::string a_type)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->AutoTransition(LowerCopy(a_type)) : std::string{};
	}
	std::string GetAutoTransitionForActor(std::monostate, std::string a_id, std::int32_t a_position, std::string a_type)
	{
		const auto scene = SceneRegistry::Find(a_id);
		if (!scene || a_position < 0 || static_cast<std::size_t>(a_position) >= scene->actors.size()) {
			return {};
		}
		return scene->actors[a_position].AutoTransition(LowerCopy(a_type));
	}
	bool IsNoRandomSelection(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene && scene->noRandomSelection;
	}
	std::string GetModpack(std::monostate, std::string a_id)
	{
		const auto scene = SceneRegistry::Find(a_id);
		return scene ? scene->modpack : std::string{};
	}
}

namespace Core
{
	void ForgetActorMeta(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_actorMetaLock);
		for (const auto id : a_ids) {
			g_actorMeta.erase(id);
		}
	}
}
