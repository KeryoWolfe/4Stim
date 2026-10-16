#include "Lights.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Sounds.h"

#include "RE/E/ExtraLight.h"
#include "RE/N/NEW_REFR_DATA.h"

namespace Lights
{
	namespace
	{
		constexpr auto LIGHT_FOLDER = "Data/F4SE/Plugins/4Stim/Lights";  // OStim: equip objects of type "light"

		struct LightType
		{
			std::string  id;
			std::string  name;
			std::uint32_t form = 0;           // the LIGH placed; 0 = no light (OStim's nolight)
			std::string  node = "Head";       // the bone it follows
			RE::NiPoint3 offset{};            // from the bone: x right, y forward, z up (the actor's facing)
			float        brightness = 1.0F;   // the light's dimmer (OStim: a dim version of each light)
		};

		struct Lit
		{
			std::shared_ptr<const LightType> type;
			RE::ObjectRefHandle              ref;
			bool                             failed = false;  // couldn't be placed: don't retry every frame
		};

		Config                                                      g_config;
		std::mutex                                                  g_lock;
		bool                                                        g_loaded = false;
		std::unordered_map<std::string, std::shared_ptr<const LightType>> g_types;
		std::unordered_map<std::uint32_t, Lit>                      g_lit;  // actor -> their light

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_types.clear();
			std::error_code ec;
			for (const auto& e : std::filesystem::directory_iterator(LIGHT_FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					if (!root.is_object()) {
						continue;
					}
					auto type = std::make_shared<LightType>();
					type->id = Lower(e.path().stem().string());
					type->name = root.value("name", type->id);
					if (const auto light = root.find("light"); light != root.end()) {
						type->form = Sounds::ReadForm(*light);
						if (!type->form) {
							REX::WARN("Lights: {}: its \"light\" isn't loaded", e.path().filename().string());
						}
					}
					type->node = root.value("node", std::string("Head"));
					if (const auto o = root.find("offset"); o != root.end() && o->is_object()) {
						type->offset = { o->value("x", 0.0F), o->value("y", 0.0F), o->value("z", 0.0F) };
					}
					type->brightness = std::clamp(root.value("brightness", 1.0F), 0.0F, 10.0F);
					const auto id = type->id;
					g_types[id] = std::move(type);
				} catch (const std::exception& ex) {
					REX::WARN("Lights: {}: couldn't be read ({})", e.path().filename().string(), ex.what());
				}
			}
			REX::INFO("Lights: {} light(s) from {}", g_types.size(), LIGHT_FOLDER);
		}

		std::shared_ptr<const LightType> TypeFor(RE::Actor* a_actor)
		{
			if (!g_loaded) {
				LoadLocked();
			}
			const auto& id = a_actor->GetSex() == RE::SEX::kFemale ? g_config.femaleLight : g_config.maleLight;
			const auto  it = g_types.find(Lower(id));
			if (it == g_types.end()) {
				if (Lower(id) != "nolight") {
					REX::WARN("Lights: no light \"{}\" in {}", id, LIGHT_FOLDER);
				}
				return nullptr;
			}
			return it->second->form ? it->second : nullptr;
		}

		// How bright it is where a_actor stands: the game's own GetLightLevel
		// condition (what OStim's Papyrus GetLightLevel gives in Skyrim).
		// -1 if it can't be read. (The AI process's cached lightLevel isn't
		// it: it read 134 in pitch dark.)
		float LightLevel(RE::Actor* a_actor)
		{
			static const auto function = RE::SCRIPT_FUNCTION::LocateScriptCommand("GetLightLevel");
			if (!a_actor || !function || !function->conditionFunction) {
				return -1.0F;
			}
			RE::ConditionCheckParams params;
			params.actionRef = a_actor;
			params.targetRef = a_actor;
			float value = -1.0F;
			if (!function->conditionFunction(params, nullptr, nullptr, value)) {
				return -1.0F;
			}
			return value;
		}

		// Where a_type's light goes on a_actor now, or false if their model
		// or bone isn't there.
		bool PositionFor(RE::Actor* a_actor, const LightType& a_type, RE::NiPoint3& a_out)
		{
			const auto root = a_actor->Get3D(false);
			const auto bone = root ? root->GetObjectByName(RE::BSFixedString(a_type.node)) : nullptr;
			if (!bone) {
				return false;
			}
			const float heading = a_actor->GetHeading();
			const float s = std::sin(heading);
			const float c = std::cos(heading);
			const auto& at = bone->world.translate;
			// Forward is (sin, cos) in Fallout 4, right is (cos, -sin).
			a_out.x = at.x + a_type.offset.x * c + a_type.offset.y * s;
			a_out.y = at.y - a_type.offset.x * s + a_type.offset.y * c;
			a_out.z = at.z + a_type.offset.z;
			return true;
		}

		RE::ObjectRefHandle Place(RE::Actor* a_actor, const LightType& a_type)
		{
			const auto handler = RE::TESDataHandler::GetSingleton();
			const auto light = RE::TESForm::GetFormByID<RE::TESObjectLIGH>(a_type.form);
			const auto cell = a_actor->GetParentCell();
			RE::NiPoint3 at{};
			if (!handler || !light || !cell || !PositionFor(a_actor, a_type, at)) {
				return {};
			}
			RE::NEW_REFR_DATA data;
			data.location = at;
			data.object = light;
			if (cell->IsInterior()) {
				data.interior = cell;
			} else {
				data.world = cell->worldSpace;
			}
			data.initializeScripts = false;
			return handler->CreateReferenceAtLocation(data);
		}

		void Remove(Lit& a_lit)
		{
			if (const auto ref = a_lit.ref.get()) {
				ref->Disable();
				ref->SetDelete(true);
			}
			a_lit.ref = {};
		}

		// Keeps a placed light's dimmer at its brightness (the game fades
		// lights in towards wantDimmer).
		void ApplyBrightness(RE::TESObjectREFR* a_ref, float a_brightness)
		{
			const auto extra = a_ref->extraList ? a_ref->extraList->GetByType<RE::ExtraLight>() : nullptr;
			if (!extra || !extra->lightdata.light) {
				return;
			}
			extra->lightdata.light->dimmer = a_brightness;
			extra->lightdata.wantDimmer = a_brightness;
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

	void Enter(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		if (a_actors.empty() || std::ranges::all_of(a_actors, [](std::uint32_t id) { return g_lit.contains(id); })) {
			return;
		}
		// As OStim's thread start: lights only if it's dark where the first
		// actor is (with SetOnlyLightInDark), and only once per actor.
		const auto  first = RE::TESForm::GetFormByID<RE::Actor>(a_actors.front());
		const float level = LightLevel(first);
		REX::INFO("Lights: light level {} at {:08X} where the scene starts, {} at the player (dark below {}{})", level, a_actors.front(),
			LightLevel(RE::PlayerCharacter::GetSingleton()), g_config.darkLevel, g_config.onlyInDark ? "" : "; lights regardless");
		if (g_config.onlyInDark) {
			if (level >= g_config.darkLevel) {
				for (const auto id : a_actors) {
					g_lit.try_emplace(id);  // no light, and don't ask again this scene
				}
				return;
			}
		}
		for (const auto id : a_actors) {
			if (g_lit.contains(id)) {
				continue;
			}
			auto&      lit = g_lit[id];
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			lit.type = actor ? TypeFor(actor) : nullptr;
			if (lit.type) {
				REX::INFO("Lights: {:08X} gets \"{}\"", id, lit.type->id);
			}
		}
	}

	void Tick()
	{
		std::scoped_lock lock(g_lock);
		for (auto& [id, lit] : g_lit) {
			if (!lit.type || lit.failed) {
				continue;
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			auto ref = lit.ref.get();
			if (!ref) {
				RE::NiPoint3 where{};
				if (!PositionFor(actor, *lit.type, where)) {
					continue;  // their model isn't loaded (yet): try again next time
				}
				lit.ref = Place(actor, *lit.type);
				ref = lit.ref.get();
				if (!ref) {
					REX::WARN("Lights: couldn't place \"{}\" on {:08X}", lit.type->id, id);
					lit.failed = true;
					continue;
				}
				REX::INFO("Lights: \"{}\" placed on {:08X} (light {:08X} at their {})", lit.type->id, id, ref->GetFormID(), lit.type->node);
			}
			RE::NiPoint3 at{};
			if (PositionFor(actor, *lit.type, at)) {
				ref->data.location.x = at.x;
				ref->data.location.y = at.y;
				ref->data.location.z = at.z;
				ref->Update3DPosition(true);
			}
			ApplyBrightness(ref.get(), lit.type->brightness);
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_actors) {
			if (const auto it = g_lit.find(id); it != g_lit.end()) {
				Remove(it->second);
				g_lit.erase(it);
			}
		}
	}

	void RemoveAll()
	{
		std::scoped_lock lock(g_lock);
		for (auto& [id, lit] : g_lit) {
			Remove(lit);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_lit.clear();
	}
}
