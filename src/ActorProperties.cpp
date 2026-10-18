#include "ActorProperties.h"
#include "compat/Fallbacks.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Sounds.h"
#include "Undress.h"

namespace ActorProperties
{
	namespace
	{
		constexpr auto FOLDER = "Data/F4SE/Plugins/4Stim/Actor Properties";  // OStim: SKSE\Plugins\OStim\actor properties

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		// A form named in a condition: {"mod", "formid"} (OStim's form
		// fields), or an editor ID ("ActorTypeNPC", "HumanRace"), which
		// keywords and races keep in game.
		struct FormRef
		{
			std::uint32_t id = 0;
			std::string   editorID;
		};

		// When a property applies to an actor. OStim's: a perk's condition
		// functions (and the perk's level is the priority). 4Stim's: the
		// fields below, all of which must hold (a list: any of it).
		struct Rule
		{
			std::uint32_t        perk = 0;
			std::optional<bool>  female;
			std::vector<FormRef> races, notRaces;
			std::vector<FormRef> keywords, notKeywords;  // on the actor's race or base
			std::vector<FormRef> factions, notFactions;
			std::vector<FormRef> worn, notWorn;          // armor worn
			std::vector<FormRef> wornKeywords, notWornKeywords;
			std::optional<bool>  player, child, penis;
			int                  priority = 0;
			std::string          file;
		};

		template <class T>
		struct Property
		{
			std::shared_ptr<const Rule> condition;
			T                                value;
		};

		// OStim's ActorPropertyList: by priority, the first that holds wins.
		template <class T>
		struct PropertyList
		{
			std::vector<Property<T>> entries;

			void Sort()
			{
				std::ranges::stable_sort(entries, [](const Property<T>& a, const Property<T>& b) { return a.condition->priority > b.condition->priority; });
			}
		};

		Config                                                     g_config;
		std::recursive_mutex                                       g_lock;
		bool                                                       g_loaded = false;
		PropertyList<std::string>                                  g_types;
		PropertyList<bool>                                         g_sexes;  // true = male (has a penis)
		PropertyList<std::string>                                  g_expressions;
		PropertyList<bool>                                         g_mutes;
		PropertyList<bool>                                         g_muffles;
		std::unordered_map<std::string, PropertyList<bool>>        g_requirements;
		std::unordered_map<std::string, PropertyList<bool>>        g_equipObjects;
		std::mutex                                                 g_cacheLock;
		std::unordered_map<std::uint32_t, SceneRegistry::ActorCondition> g_cache;
		thread_local bool                                          t_inPenis = false;  // a "penis" condition inside a "sex" one

		std::vector<FormRef> ReadForms(const nlohmann::json& a_condition, const char* a_key, const std::string& a_file)
		{
			std::vector<FormRef> out;
			const auto it = a_condition.find(a_key);
			if (it == a_condition.end()) {
				return out;
			}
			auto one = [&](const nlohmann::json& a_value) {
				FormRef ref;
				if (a_value.is_string()) {
					ref.editorID = a_value.get<std::string>();
					if (const auto form = RE::TESForm::GetFormByEditorID(RE::BSFixedString(ref.editorID))) {
						ref.id = form->GetFormID();
					}
				} else {
					ref.id = Sounds::ReadForm(a_value);
					if (!ref.id) {
						REX::WARN("Actor properties: {}: a form in \"{}\" isn't loaded", a_file, a_key);
						return;
					}
				}
				out.push_back(std::move(ref));
			};
			if (it->is_array()) {
				for (const auto& v : *it) {
					one(v);
				}
			} else {
				one(*it);
			}
			return out;
		}

		std::shared_ptr<const Rule> ReadCondition(const nlohmann::json& a_root, const std::string& a_file)
		{
			const auto it = a_root.find("condition");
			if (it == a_root.end() || !it->is_object()) {
				REX::WARN("Actor properties: {}: no \"condition\", skipped", a_file);
				return nullptr;
			}
			const auto& c = *it;
			auto        out = std::make_shared<Rule>();
			out->file = a_file;
			if (c.contains("formid")) {
				const auto id = Sounds::ReadForm(c);
				const auto perk = id ? RE::TESForm::GetFormByID<RE::BGSPerk>(id) : nullptr;
				if (!perk) {
					REX::WARN("Actor properties: {}: its condition perk isn't loaded, skipped", a_file);
					return nullptr;
				}
				out->perk = id;
				out->priority = perk->data.level;
			}
			if (const auto sex = Lower(c.value("sex", std::string{})); !sex.empty()) {
				out->female = sex == "female";
			}
			out->races = ReadForms(c, "races", a_file);
			out->notRaces = ReadForms(c, "notRaces", a_file);
			out->keywords = ReadForms(c, "keywords", a_file);
			out->notKeywords = ReadForms(c, "notKeywords", a_file);
			out->factions = ReadForms(c, "factions", a_file);
			out->notFactions = ReadForms(c, "notFactions", a_file);
			out->worn = ReadForms(c, "worn", a_file);
			out->notWorn = ReadForms(c, "notWorn", a_file);
			out->wornKeywords = ReadForms(c, "wornKeywords", a_file);
			out->notWornKeywords = ReadForms(c, "notWornKeywords", a_file);
			for (auto [key, field] : { std::pair{ "player", &out->player }, { "child", &out->child }, { "penis", &out->penis } }) {
				if (const auto v = c.find(key); v != c.end() && v->is_boolean()) {
					*field = v->get<bool>();
				}
			}
			if (const auto v = a_root.find("priority"); v != a_root.end() && v->is_number()) {
				out->priority = v->get<int>();
			}
			return out;
		}

		bool KeywordMatches(const RE::BGSKeywordForm* a_form, const FormRef& a_ref)
		{
			return a_form && (a_ref.id ? a_form->HasKeywordID(a_ref.id) : a_form->HasKeywordString(a_ref.editorID));
		}

		bool HasAnyKeyword(RE::Actor* a_actor, const std::vector<FormRef>& a_refs)
		{
			const auto npc = a_actor->GetNPC();
			return std::ranges::any_of(a_refs, [&](const FormRef& r) {
				return (a_actor->race && KeywordMatches(a_actor->race, r)) || (npc && KeywordMatches(npc, r));
			});
		}

		bool IsRace(RE::Actor* a_actor, const std::vector<FormRef>& a_refs)
		{
			const auto race = a_actor->race;
			return race && std::ranges::any_of(a_refs, [&](const FormRef& r) {
				return r.id ? race->GetFormID() == r.id : _stricmp(race->formEditorID.c_str(), r.editorID.c_str()) == 0;
			});
		}

		bool InFaction(RE::Actor* a_actor, const std::vector<FormRef>& a_refs)
		{
			return std::ranges::any_of(a_refs, [&](const FormRef& r) {
				const auto faction = r.id ? RE::TESForm::GetFormByID<RE::TESFaction>(r.id) : nullptr;
				return faction && a_actor->IsInFaction(faction);
			});
		}

		bool Wears(const std::vector<std::uint32_t>& a_worn, const std::vector<FormRef>& a_refs)
		{
			return std::ranges::any_of(a_refs, [&](const FormRef& r) { return r.id && std::ranges::find(a_worn, r.id) != a_worn.end(); });
		}

		bool WearsKeyword(const std::vector<std::uint32_t>& a_worn, const std::vector<FormRef>& a_refs)
		{
			return std::ranges::any_of(a_worn, [&](std::uint32_t a_id) {
				const auto armor = RE::TESForm::GetFormByID<RE::TESObjectARMO>(a_id);
				return armor && std::ranges::any_of(a_refs, [&](const FormRef& r) { return KeywordMatches(armor, r); });
			});
		}

		bool HasPenisLocked(RE::Actor* a_actor);

		bool Holds(const Rule& a_c, RE::Actor* a_actor)
		{
			if (a_c.perk) {
				const auto perk = RE::TESForm::GetFormByID<RE::BGSPerk>(a_c.perk);
				if (!perk || !perk->perkConditions.IsTrue(a_actor, a_actor)) {
					return false;
				}
			}
			if (a_c.female && (Compat::GetSex(a_actor) == RE::SEX::kFemale) != *a_c.female) {
				return false;
			}
			if (a_c.player && (a_actor == RE::PlayerCharacter::GetSingleton()) != *a_c.player) {
				return false;
			}
			if (a_c.child && a_actor->IsChild() != *a_c.child) {
				return false;
			}
			if ((!a_c.races.empty() && !IsRace(a_actor, a_c.races)) || IsRace(a_actor, a_c.notRaces)) {
				return false;
			}
			if ((!a_c.keywords.empty() && !HasAnyKeyword(a_actor, a_c.keywords)) || HasAnyKeyword(a_actor, a_c.notKeywords)) {
				return false;
			}
			if ((!a_c.factions.empty() && !InFaction(a_actor, a_c.factions)) || InFaction(a_actor, a_c.notFactions)) {
				return false;
			}
			if (!a_c.worn.empty() || !a_c.notWorn.empty() || !a_c.wornKeywords.empty() || !a_c.notWornKeywords.empty()) {
				const auto worn = Undress::WornItems(a_actor);
				if ((!a_c.worn.empty() && !Wears(worn, a_c.worn)) || Wears(worn, a_c.notWorn)) {
					return false;
				}
				if ((!a_c.wornKeywords.empty() && !WearsKeyword(worn, a_c.wornKeywords)) || WearsKeyword(worn, a_c.notWornKeywords)) {
					return false;
				}
			}
			if (a_c.penis) {
				// Inside a "sex" property's own condition, by the body.
				const bool penis = t_inPenis ? Compat::GetSex(a_actor) == RE::SEX::kMale : HasPenisLocked(a_actor);
				if (penis != *a_c.penis) {
					return false;
				}
			}
			return true;
		}

		template <class T>
		std::optional<T> Get(const PropertyList<T>& a_list, RE::Actor* a_actor)
		{
			for (const auto& p : a_list.entries) {
				if (Holds(*p.condition, a_actor)) {
					return p.value;
				}
			}
			return std::nullopt;
		}

		// What the default files (4StimNPC, 4StimFemale, 4StimPenis) say, for
		// when the folder is missing (no one could be in a scene otherwise).
		constexpr auto BUILT_IN = R"([
			{ "condition": { "keywords": [ "ActorTypeNPC" ], "child": false }, "priority": 10, "type": "npc",
			  "requirements": { "anus": true, "foot": true, "hand": true, "mouth": true, "nipple": true, "penis": true } },
			{ "condition": { "sex": "female" }, "priority": 10, "requirements": { "breast": true, "vagina": true } },
			{ "condition": { "penis": true }, "priority": 10, "requirements": { "testicles": true } }
		])";

		bool Add(const nlohmann::json& a_root, const std::string& a_file);

		void LoadLocked()
		{
			g_loaded = true;
			g_types = {};
			g_sexes = {};
			g_expressions = {};
			g_mutes = {};
			g_muffles = {};
			g_requirements.clear();
			g_equipObjects.clear();
			std::size_t     count = 0;
			std::error_code ec;
			for (const auto& e : std::filesystem::recursive_directory_iterator(FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				const auto file = e.path().filename().string();
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					if (!root.is_object()) {
						continue;
					}
					count += Add(root, file) ? 1 : 0;
				} catch (const std::exception& ex) {
					REX::WARN("Actor properties: {}: couldn't be read ({})", file, ex.what());
				}
			}
			if (count == 0) {
				REX::WARN("Actor properties: no files in {}: using the built-in ones (people, women, penises)", FOLDER);
				for (const auto& entry : nlohmann::json::parse(BUILT_IN)) {
					Add(entry, "(built in)");
				}
			}
			g_types.Sort();
			g_sexes.Sort();
			g_expressions.Sort();
			g_mutes.Sort();
			g_muffles.Sort();
			for (auto& [id, list] : g_requirements) {
				list.Sort();
			}
			for (auto& [id, list] : g_equipObjects) {
				list.Sort();
			}
			REX::INFO("Actor properties: {} file(s) from {}, {} requirement(s), {} equip object type(s)", count, FOLDER, g_requirements.size(), g_equipObjects.size());
		}

		bool Add(const nlohmann::json& a_root, const std::string& a_file)
		{
			const auto condition = ReadCondition(a_root, a_file);
			if (!condition) {
				return false;
			}
			if (const auto v = a_root.find("type"); v != a_root.end() && v->is_string()) {
				g_types.entries.push_back({ condition, Lower(v->get<std::string>()) });
			}
			if (const auto v = a_root.find("sex"); v != a_root.end() && v->is_string()) {
				g_sexes.entries.push_back({ condition, Lower(v->get<std::string>()) == "male" });
			}
			if (const auto v = a_root.find("expression"); v != a_root.end() && v->is_string()) {
				g_expressions.entries.push_back({ condition, Lower(v->get<std::string>()) });
			}
			if (const auto v = a_root.find("mute"); v != a_root.end() && v->is_boolean()) {
				g_mutes.entries.push_back({ condition, v->get<bool>() });
			}
			if (const auto v = a_root.find("muffle"); v != a_root.end() && v->is_boolean()) {
				g_muffles.entries.push_back({ condition, v->get<bool>() });
			}
			for (const auto [key, map] : { std::pair{ "requirements", &g_requirements }, { "equipObjects", &g_equipObjects } }) {
				if (const auto v = a_root.find(key); v != a_root.end() && v->is_object()) {
					for (const auto& [id, on] : v->items()) {
						if (on.is_boolean()) {
							(*map)[Lower(id)].entries.push_back({ condition, on.get<bool>() });
						}
					}
				}
			}
			return true;
		}

		void EnsureLoaded()
		{
			if (!g_loaded) {
				LoadLocked();
			}
		}

		bool HasPenisLocked(RE::Actor* a_actor)
		{
			EnsureLoaded();
			const bool outer = !t_inPenis;
			t_inPenis = true;
			const auto sex = Get(g_sexes, a_actor);
			if (outer) {
				t_inPenis = false;
			}
			return sex ? *sex : Compat::GetSex(a_actor) == RE::SEX::kMale;
		}

		SceneRegistry::ActorCondition ConditionLocked(RE::Actor* a_actor)
		{
			EnsureLoaded();
			SceneRegistry::ActorCondition out;
			if (!a_actor) {
				// As OStim: no actor meets everything.
				for (const auto& [id, list] : g_requirements) {
					out.requirements.push_back(id);
				}
				return out;
			}
			out.type = Get(g_types, a_actor).value_or(std::string{});
			for (const auto& [id, list] : g_requirements) {
				if (Get(list, a_actor).value_or(false)) {
					out.requirements.push_back(id);
				}
			}
			out.penis = HasPenisLocked(a_actor);
			switch (Compat::GetSex(a_actor)) {
			case RE::SEX::kFemale:
				out.sex = g_config.futaUseMaleRole && out.penis ? SceneRegistry::Sex::kAny : SceneRegistry::Sex::kFemale;
				break;
			case RE::SEX::kMale:
				out.sex = SceneRegistry::Sex::kMale;
				break;
			default:
				out.sex = SceneRegistry::Sex::kAny;
				break;
			}
			return out;
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void Reload()
	{
		{
			std::scoped_lock lock(g_lock);
			LoadLocked();
		}
		std::scoped_lock cache(g_cacheLock);
		g_cache.clear();
	}

	std::string Type(RE::Actor* a_actor)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return a_actor ? Get(g_types, a_actor).value_or(std::string{}) : std::string{ "npc" };
	}

	bool HasPenis(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		std::scoped_lock lock(g_lock);
		return HasPenisLocked(a_actor);
	}

	SceneRegistry::ActorCondition Condition(RE::Actor* a_actor)
	{
		std::scoped_lock lock(g_lock);
		return ConditionLocked(a_actor);
	}

	std::string Expression(RE::Actor* a_actor)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return a_actor ? Get(g_expressions, a_actor).value_or(std::string{}) : std::string{};
	}

	bool Muted(RE::Actor* a_actor)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return a_actor && Get(g_mutes, a_actor).value_or(false);
	}

	bool Muffled(RE::Actor* a_actor)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return a_actor && Get(g_muffles, a_actor).value_or(false);
	}

	std::vector<std::string> EquipObjects(RE::Actor* a_actor)
	{
		std::scoped_lock         lock(g_lock);
		std::vector<std::string> out;
		EnsureLoaded();
		if (!a_actor) {
			return out;
		}
		for (const auto& [id, list] : g_equipObjects) {
			if (Get(list, a_actor).value_or(false)) {
				out.push_back(id);
			}
		}
		return out;
	}

	bool MaleExcitement(RE::Actor* a_actor)
	{
		const bool female = a_actor && Compat::GetSex(a_actor) == RE::SEX::kFemale;
		return !female || (g_config.futaUseMaleExcitement && HasPenis(a_actor));
	}

	bool MaleClimax(RE::Actor* a_actor)
	{
		const bool female = a_actor && Compat::GetSex(a_actor) == RE::SEX::kFemale;
		return HasPenis(a_actor) && (!female || g_config.futaUseMaleClimax);
	}

	void Remember(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		auto condition = Condition(a_actor);
		std::scoped_lock lock(g_cacheLock);
		g_cache[a_actor->GetFormID()] = std::move(condition);
	}

	SceneRegistry::ActorCondition Cached(std::uint32_t a_id)
	{
		{
			std::scoped_lock lock(g_cacheLock);
			if (const auto it = g_cache.find(a_id); it != g_cache.end()) {
				return it->second;
			}
		}
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
		auto       condition = Condition(actor);
		if (actor) {
			std::scoped_lock lock(g_cacheLock);
			g_cache[a_id] = condition;
		}
		return condition;
	}
}
