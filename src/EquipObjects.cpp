#include "EquipObjects.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "ActorProperties.h"
#include "Bridge.h"
#include "Sounds.h"

namespace EquipObjects
{
	namespace
	{
		constexpr auto FOLDER = "Data/F4SE/Plugins/4Stim/Equip Objects";  // OStim: SKSE\Plugins\OStim\equip objects

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		struct Object
		{
			std::string                                    id;    // the file name
			std::string                                    type;  // "strapon", "tongue"...
			std::string                                    name;
			std::uint32_t                                  item = 0;  // the armor
			std::unordered_map<std::string, std::uint32_t> variants;
		};

		// One equip object type on one actor (OStim's EquipObjectHandler).
		struct Handler
		{
			std::shared_ptr<const Object> object;
			std::uint32_t                 equipped = 0;  // the armor on now, 0 = none
			std::string                   variant;
			float                         variantLeft = 0.0F;  // seconds; 0 = for good
			std::vector<std::uint32_t>    given;  // everything put on, taken away at the end (OStim's toRemove)
		};

		struct ActorState
		{
			std::unordered_map<std::string, Handler> handlers;  // by type
			std::vector<std::string>                 sceneObjects;  // what the scene they're in asks for
		};

		Config                                                                       g_config;
		std::mutex                                                                   g_lock;
		bool                                                                         g_loaded = false;
		std::unordered_map<std::string, std::vector<std::shared_ptr<const Object>>> g_byType;
		std::unordered_map<std::string, std::shared_ptr<const Object>>              g_defaults;
		std::unordered_set<std::uint32_t>                                            g_items;
		std::unordered_map<std::uint32_t, ActorState>                                g_actors;
		// The object each actor wears of a type, chosen in the HUD's options
		// (OStim's options menu): actor -> type -> object id. Saved with the
		// game (main.cpp's co-save, as OStim's serialized equip objects).
		std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> g_chosen;

		void LoadLocked()
		{
			g_loaded = true;
			g_byType.clear();
			g_defaults.clear();
			g_items.clear();
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
					auto object = std::make_shared<Object>();
					object->id = Lower(e.path().stem().string());
					object->type = Lower(root.value("type", std::string{}));
					object->name = root.value("name", object->id);
					object->item = Sounds::ReadForm(root);
					if (object->type.empty() || !object->item || !RE::TESForm::GetFormByID<RE::TESObjectARMO>(object->item)) {
						REX::WARN("Equip objects: {}: no \"type\", or its armor isn't loaded; skipped", file);
						continue;
					}
					if (const auto variants = root.find("variants"); variants != root.end() && variants->is_object()) {
						for (const auto& [variant, form] : variants->items()) {
							if (const auto id = Sounds::ReadForm(form); id && RE::TESForm::GetFormByID<RE::TESObjectARMO>(id)) {
								object->variants[Lower(variant)] = id;
								g_items.insert(id);
							} else {
								REX::WARN("Equip objects: {}: variant \"{}\" isn't loaded", file, variant);
							}
						}
					}
					g_items.insert(object->item);
					if (root.value("default", false)) {
						g_defaults[object->type] = object;
					}
					g_byType[object->type].push_back(std::move(object));
					++count;
				} catch (const std::exception& ex) {
					REX::WARN("Equip objects: {}: couldn't be read ({})", file, ex.what());
				}
			}
			REX::INFO("Equip objects: {} from {}, {} type(s)", count, FOLDER, g_byType.size());
		}

		void EnsureLoaded()
		{
			if (!g_loaded) {
				LoadLocked();
			}
		}

		// The object of a_type a_id wears (OStim's getEquipObject): the one
		// chosen for them (the HUD's options), else the default one, else any.
		std::shared_ptr<const Object> Choose(std::uint32_t a_id, const std::string& a_type)
		{
			EnsureLoaded();
			if (const auto actor = g_chosen.find(a_id); actor != g_chosen.end()) {
				if (const auto chosen = actor->second.find(a_type); chosen != actor->second.end()) {
					if (const auto list = g_byType.find(a_type); list != g_byType.end()) {
						for (const auto& object : list->second) {
							if (object->id == chosen->second) {
								return object;
							}
						}
					}
				}
			}
			if (const auto it = g_defaults.find(a_type); it != g_defaults.end()) {
				return it->second;
			}
			const auto it = g_byType.find(a_type);
			if (it == g_byType.end() || it->second.empty()) {
				return nullptr;
			}
			static std::mt19937 rng{ std::random_device{}() };
			return it->second[std::uniform_int_distribution<std::size_t>(0, it->second.size() - 1)(rng)];
		}

		void Dispatch(std::string_view a_function, std::uint32_t a_id, std::vector<std::int32_t> a_items)
		{
			if (const auto vm = FourStim::GetVM()) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				vm->DispatchStaticCall("FourStimUndress"sv, a_function, callback, static_cast<std::int32_t>(a_id), std::move(a_items));
			}
		}

		void PutOn(std::uint32_t a_id, Handler& a_handler)
		{
			const auto it = a_handler.variant.empty() ? a_handler.object->variants.end() : a_handler.object->variants.find(a_handler.variant);
			a_handler.equipped = it != a_handler.object->variants.end() ? it->second : a_handler.object->item;
			if (std::ranges::find(a_handler.given, a_handler.equipped) == a_handler.given.end()) {
				a_handler.given.push_back(a_handler.equipped);
			}
			REX::INFO("Equip objects: {:08X} puts on \"{}\"{} ({:08X})", a_id, a_handler.object->id, a_handler.variant.empty() ? "" : " (" + a_handler.variant + ")", a_handler.equipped);
			Dispatch("WearObject"sv, a_id, { static_cast<std::int32_t>(a_handler.equipped) });
		}

		void TakeOff(std::uint32_t a_id, Handler& a_handler)
		{
			if (!a_handler.equipped) {
				return;
			}
			REX::INFO("Equip objects: {:08X} takes off \"{}\" ({:08X})", a_id, a_handler.object->id, a_handler.equipped);
			Dispatch("TakeOffObjects"sv, a_id, { static_cast<std::int32_t>(a_handler.equipped) });
			a_handler.equipped = 0;
		}

		bool EquipLocked(std::uint32_t a_id, ActorState& a_state, const std::string& a_type)
		{
			if (const auto it = a_state.handlers.find(a_type); it != a_state.handlers.end()) {
				if (!it->second.equipped) {
					PutOn(a_id, it->second);
				}
				return true;
			}
			const auto object = Choose(a_id, a_type);
			if (!object) {
				return false;
			}
			auto& handler = a_state.handlers[a_type];
			handler.object = object;
			PutOn(a_id, handler);
			return true;
		}

		void UnequipLocked(std::uint32_t a_id, ActorState& a_state, const std::string& a_type)
		{
			if (const auto it = a_state.handlers.find(a_type); it != a_state.handlers.end()) {
				TakeOff(a_id, it->second);
			}
		}

		// Changes a worn object's variant: off and on again (OStim's).
		void Rewear(std::uint32_t a_id, Handler& a_handler)
		{
			if (a_handler.equipped) {
				TakeOff(a_id, a_handler);
				PutOn(a_id, a_handler);
			}
		}

		ActorState* Find(std::uint32_t a_id)
		{
			const auto it = g_actors.find(a_id);
			return it != g_actors.end() ? &it->second : nullptr;
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}

	std::vector<std::string> Types()
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		std::vector<std::string> out;
		for (const auto& [type, list] : g_byType) {
			out.push_back(type);
		}
		std::ranges::sort(out);
		return out;
	}

	bool IsObjectItem(std::uint32_t a_formID)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		return g_items.contains(a_formID);
	}

	void SceneEntered(const std::vector<std::uint32_t>& a_ids, const SceneRegistry::Scene& a_scene, bool a_start)
	{
		for (std::size_t role = 0; role < a_ids.size() && role < a_scene.actors.size(); ++role) {
			const auto id = a_ids[role];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			// Asked before taking the lock: actor properties have their own.
			const auto fromProperties = a_start ? ActorProperties::EquipObjects(actor) : std::vector<std::string>{};
			const bool penis = ActorProperties::HasPenis(actor);
			const auto& part = a_scene.actors[role];

			std::scoped_lock lock(g_lock);
			auto&            state = g_actors[id];
			for (const auto& type : fromProperties) {
				EquipLocked(id, state, type);
			}
			// The scene's own (OStim's changeNodeEquipObjects): off what the
			// last one wore and this one doesn't, on what this one adds.
			for (const auto& type : state.sceneObjects) {
				if (std::ranges::find(part.equipObjects, type) == part.equipObjects.end()) {
					UnequipLocked(id, state, type);
				}
			}
			for (const auto& type : part.equipObjects) {
				if (std::ranges::find(state.sceneObjects, type) == state.sceneObjects.end()) {
					EquipLocked(id, state, type);
				}
			}
			state.sceneObjects = part.equipObjects;
			// Strap-ons (OStim's): on in a role that needs a penis, for
			// someone without one; off in one that needs a vagina (or any
			// that doesn't need a penis, with unequipStrapOnIfNotNeeded).
			if (!penis) {
				const auto needs = [&](std::string_view a_part) { return std::ranges::find(part.requirements, a_part) != part.requirements.end(); };
				if (needs("penis")) {
					EquipLocked(id, state, "strapon");
				} else if (g_config.unequipStrapOnIfNotNeeded || needs("vagina")) {
					UnequipLocked(id, state, "strapon");
				}
			}
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			const auto it = g_actors.find(id);
			if (it == g_actors.end()) {
				continue;
			}
			std::vector<std::int32_t> items;
			for (auto& [type, handler] : it->second.handlers) {
				for (const auto item : handler.given) {
					items.push_back(static_cast<std::int32_t>(item));
				}
			}
			if (!items.empty()) {
				REX::INFO("Equip objects: {:08X} gives back {} item(s)", id, items.size());
				Dispatch("RemoveObjects"sv, id, std::move(items));
			}
			g_actors.erase(it);
		}
	}

	std::vector<std::pair<std::string, std::string>> ObjectsOf(std::string a_type)
	{
		a_type = Lower(a_type);
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		std::vector<std::pair<std::string, std::string>> out;
		if (const auto it = g_byType.find(a_type); it != g_byType.end()) {
			for (const auto& object : it->second) {
				out.emplace_back(object->id, object->name.empty() ? object->id : object->name);
			}
		}
		return out;
	}

	std::string Chosen(std::uint32_t a_id, std::string a_type)
	{
		a_type = Lower(a_type);
		std::scoped_lock lock(g_lock);
		const auto       object = Choose(a_id, a_type);
		return object ? object->id : std::string{};
	}

	bool SetChosen(std::uint32_t a_id, std::string a_type, std::string a_objectID)
	{
		a_type = Lower(a_type);
		a_objectID = Lower(a_objectID);
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		const auto list = g_byType.find(a_type);
		if (list == g_byType.end() || std::ranges::none_of(list->second, [&](const auto& o) { return Lower(o->id) == a_objectID; })) {
			REX::WARN("Equip objects: no \"{}\" object \"{}\" to choose for {:08X}", a_type, a_objectID, a_id);
			return false;
		}
		const auto object = *std::ranges::find_if(list->second, [&](const auto& o) { return Lower(o->id) == a_objectID; });
		g_chosen[a_id][a_type] = object->id;
		REX::INFO("Equip objects: {:08X} wears \"{}\" as their {}", a_id, object->id, a_type);
		// Wearing another one of that type now: this one instead.
		if (const auto state = Find(a_id)) {
			if (const auto it = state->handlers.find(a_type); it != state->handlers.end() && it->second.object != object) {
				const bool on = it->second.equipped != 0;
				TakeOff(a_id, it->second);
				it->second.object = object;
				it->second.variant.clear();
				if (on) {
					PutOn(a_id, it->second);
				}
			}
		}
		return true;
	}

	bool Equip(std::uint32_t a_id, std::string a_type)
	{
		a_type = Lower(a_type);
		std::scoped_lock lock(g_lock);
		const auto       state = Find(a_id);
		if (!state) {
			REX::INFO("Equip objects: {:08X} isn't in a scene, no \"{}\" put on", a_id, a_type);
			return false;
		}
		if (!EquipLocked(a_id, *state, a_type)) {
			REX::INFO("Equip objects: no \"{}\" object for {:08X} (none of that type installed?)", a_type, a_id);
			return false;
		}
		return true;
	}

	void Unequip(std::uint32_t a_id, std::string a_type)
	{
		a_type = Lower(a_type);
		std::scoped_lock lock(g_lock);
		if (const auto state = Find(a_id)) {
			UnequipLocked(a_id, *state, a_type);
		}
	}

	bool IsEquipped(std::uint32_t a_id, std::string a_type)
	{
		a_type = Lower(a_type);
		std::scoped_lock lock(g_lock);
		const auto       state = Find(a_id);
		if (!state) {
			return false;
		}
		const auto it = state->handlers.find(a_type);
		return it != state->handlers.end() && it->second.equipped != 0;
	}

	bool SetVariant(std::uint32_t a_id, std::string a_type, std::string a_variant, float a_seconds)
	{
		a_type = Lower(a_type);
		a_variant = Lower(a_variant);
		std::scoped_lock lock(g_lock);
		const auto       state = Find(a_id);
		if (!state) {
			return false;
		}
		auto it = state->handlers.find(a_type);
		if (it == state->handlers.end()) {
			const auto object = Choose(a_id, a_type);
			if (!object) {
				return false;
			}
			it = state->handlers.emplace(a_type, Handler{ object }).first;
		}
		auto& handler = it->second;
		// As OStim's setVariant: the same variant again only extends it.
		if (handler.variant == a_variant) {
			if (a_seconds <= 0.0F) {
				handler.variantLeft = 0.0F;
			} else if (handler.variantLeft > 0.0F && a_seconds > handler.variantLeft) {
				handler.variantLeft = a_seconds;
			}
			return true;
		}
		if (!handler.object->variants.contains(a_variant)) {
			return false;
		}
		handler.variant = a_variant;
		handler.variantLeft = std::max(a_seconds, 0.0F);
		Rewear(a_id, handler);
		return true;
	}

	void UnsetVariant(std::uint32_t a_id, std::string a_type)
	{
		a_type = Lower(a_type);
		std::scoped_lock lock(g_lock);
		const auto       state = Find(a_id);
		if (!state) {
			return;
		}
		if (const auto it = state->handlers.find(a_type); it != state->handlers.end() && !it->second.variant.empty()) {
			it->second.variant.clear();
			it->second.variantLeft = 0.0F;
			Rewear(a_id, it->second);
		}
	}

	void Tick(float a_seconds)
	{
		std::scoped_lock lock(g_lock);
		for (auto& [id, state] : g_actors) {
			for (auto& [type, handler] : state.handlers) {
				if (handler.variantLeft > 0.0F && (handler.variantLeft -= a_seconds) <= 0.0F) {
					handler.variantLeft = 0.0F;
					handler.variant.clear();
					Rewear(id, handler);
				}
			}
		}
	}

	std::vector<std::uint32_t> Given(std::uint32_t a_id)
	{
		std::scoped_lock           lock(g_lock);
		std::vector<std::uint32_t> out;
		if (const auto state = Find(a_id)) {
			for (const auto& [type, handler] : state->handlers) {
				out.insert(out.end(), handler.given.begin(), handler.given.end());
			}
		}
		return out;
	}

	void TakeBack(std::uint32_t a_id, const std::vector<std::uint32_t>& a_items)
	{
		if (!a_items.empty()) {
			REX::INFO("Equip objects: {:08X} gives back {} item(s) from the save", a_id, a_items.size());
			Dispatch("RemoveObjects"sv, a_id, std::vector<std::int32_t>(a_items.begin(), a_items.end()));
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_actors.clear();  // choices stay: the save's are read in (ClearChosen first)
	}

	std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> AllChosen()
	{
		std::scoped_lock lock(g_lock);
		return g_chosen;
	}

	void RestoreChosen(std::unordered_map<std::uint32_t, std::unordered_map<std::string, std::string>> a_chosen)
	{
		std::scoped_lock lock(g_lock);
		g_chosen = std::move(a_chosen);
	}

	void ClearChosen()
	{
		std::scoped_lock lock(g_lock);
		g_chosen.clear();
	}
}
