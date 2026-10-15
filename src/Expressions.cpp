#include "Expressions.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Console.h"
#include "Excitement.h"

namespace Expressions
{
	namespace
	{
		constexpr auto EXPRESSION_FOLDER = "Data/F4SE/Plugins/4Stim/Facial Expressions";  // OStim: SKSE/Plugins/OStim/facial expressions
		constexpr auto MORPH_MAP_FILE = "Data/F4SE/Plugins/4Stim/Expression Morphs.json";
		constexpr std::size_t MORPHS = 54;  // Fallout 4's face morphs ("mfg morphs 0-53")

		// The slots OStim's layers replace each other in (its ExpressionType),
		// plus Fallout 4 morphs given directly.
		enum Slot : std::size_t
		{
			kEmotion,
			kMouth,
			kLids,
			kBrows,
			kMorphs,
			kSlotCount
		};

		struct Modifier
		{
			int   type = 0;
			float baseValue = 0.0F;
			float variance = 0.0F;
			float speedMultiplier = 0.0F;
			float excitementMultiplier = 0.0F;
		};

		struct GenderExpression
		{
			float                   duration = 0.5F;  // as an event expression
			int                     mask = 0;         // the slots it uses (1 << Slot)
			std::optional<Modifier> expression;
			std::vector<Modifier>   phonemes;
			std::vector<Modifier>   lids;    // Skyrim modifiers 0, 1, 12, 13
			std::vector<Modifier>   brows;   // Skyrim modifiers 2-7
			std::vector<Modifier>   morphs;  // Fallout 4 morph indices
		};

		struct Expression
		{
			std::string      id;
			GenderExpression female;
			GenderExpression male;
		};
		using ExpressionList = std::vector<std::shared_ptr<const Expression>>;

		Config g_config;

		std::mutex                                      g_lock;
		bool                                            g_loaded = false;
		std::unordered_map<std::string, ExpressionList> g_byEvent, g_bySet, g_byActionActor, g_byActionTarget;
		// The morph map: [kind][Skyrim type] -> (morph, weight). Kinds: 0
		// expression, 1 phoneme, 2 modifier.
		std::array<std::unordered_map<int, std::vector<std::pair<int, float>>>, 3> g_morphMap;

		using Values = std::array<float, MORPHS>;

		// A chosen expression for one actor: what it sets, worked out once
		// (OStim randomizes each value when it applies an expression).
		struct Layer
		{
			std::shared_ptr<const Expression> source;
			const GenderExpression*           expression = nullptr;
			std::array<Values, kSlotCount>    slots{};
		};

		struct State
		{
			bool           female = false;
			std::string    sceneID;
			float          speed = 0.0F;
			ExpressionList node;       // underlying candidates
			ExpressionList overrides;  // override candidates (empty: none)
			std::optional<Layer> underlying, event, override_;
			float                underlyingIn = 0.0F;
			float                eventIn = -1.0F;  // timed event: seconds left (< 0 = until cleared)
			float                overrideIn = -1.0F;
			bool                 dirty = true;
			// The face moves toward its target a little at a time, as OStim's
			// ExpressionUpdater (speed per 20 ms: 1 underlying, 3 event, 5
			// override, so 50, 150 and 250 points a second).
			Values               target{};
			Values               current{};
			Values               rate{};          // points per second, per morph
			Values               applied{};       // last value sent to the game
			bool                 anyApplied = false;
			float                sendIn = 0.0F;   // morphs are sent at most every SEND_INTERVAL
		};
		constexpr float SEND_INTERVAL = 0.05F;
		std::unordered_map<std::uint32_t, State> g_states;

		std::mt19937 g_random{ std::random_device{}() };

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		Modifier ReadModifier(const nlohmann::json& a_value)
		{
			Modifier m;
			auto number = [&](const char* a_key) {
				const auto it = a_value.find(a_key);
				return it != a_value.end() && it->is_number() ? it->get<float>() : 0.0F;
			};
			m.type = static_cast<int>(number("type"));
			m.baseValue = number("baseValue");
			m.variance = std::max(number("variance"), 0.0F);
			m.speedMultiplier = number("speedMultiplier");
			m.excitementMultiplier = number("excitementMultiplier");
			return m;
		}

		GenderExpression ReadGender(const nlohmann::json& a_value)
		{
			GenderExpression g;
			if (!a_value.is_object()) {
				return g;
			}
			g.duration = a_value.value("duration", 0.5F);
			if (const auto it = a_value.find("expression"); it != a_value.end() && it->is_object()) {
				g.expression = ReadModifier(*it);
				g.mask |= 1 << kEmotion;
			}
			if (const auto it = a_value.find("phonemes"); it != a_value.end() && it->is_array()) {
				for (const auto& p : *it) {
					g.phonemes.push_back(ReadModifier(p));
				}
				g.mask |= g.phonemes.empty() ? 0 : 1 << kMouth;
			}
			if (const auto it = a_value.find("modifiers"); it != a_value.end() && it->is_array()) {
				for (const auto& v : *it) {
					const auto m = ReadModifier(v);
					if (m.type == 0 || m.type == 1 || m.type == 12 || m.type == 13) {
						g.lids.push_back(m);
						g.mask |= 1 << kLids;
					} else if (m.type >= 2 && m.type <= 7) {
						g.brows.push_back(m);
						g.mask |= 1 << kBrows;
					}  // 8-11: looking, no morphs in Fallout 4
				}
			}
			if (const auto it = a_value.find("morphs"); it != a_value.end() && it->is_array()) {
				for (const auto& v : *it) {
					g.morphs.push_back(ReadModifier(v));
				}
				g.mask |= g.morphs.empty() ? 0 : 1 << kMorphs;
			}
			return g;
		}

		void LoadMorphMapLocked()
		{
			for (auto& kind : g_morphMap) {
				kind.clear();
			}
			std::ifstream in(MORPH_MAP_FILE);
			if (!in) {
				REX::WARN("Expressions: no {}: OStim-style expressions can't be shown (only \"morphs\")", MORPH_MAP_FILE);
				return;
			}
			try {
				const auto root = nlohmann::json::parse(in, nullptr, true, true);
				const char* kinds[3] = { "expressions", "phonemes", "modifiers" };
				for (std::size_t k = 0; k < 3; ++k) {
					const auto it = root.find(kinds[k]);
					if (it == root.end() || !it->is_object()) {
						continue;
					}
					for (const auto& [type, list] : it->items()) {
						auto& out = g_morphMap[k][std::stoi(type)];
						for (const auto& pair : list) {
							if (pair.is_array() && pair.size() == 2 && pair[0].is_number_integer() && pair[1].is_number()) {
								const int morph = pair[0].get<int>();
								if (morph >= 0 && static_cast<std::size_t>(morph) < MORPHS) {
									out.emplace_back(morph, pair[1].get<float>());
								}
							}
						}
					}
				}
			} catch (const std::exception& e) {
				REX::WARN("Expressions: {} couldn't be read ({})", MORPH_MAP_FILE, e.what());
			}
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_byEvent.clear();
			g_bySet.clear();
			g_byActionActor.clear();
			g_byActionTarget.clear();
			LoadMorphMapLocked();
			std::error_code ec;
			int             count = 0;
			for (const auto& e : std::filesystem::recursive_directory_iterator(EXPRESSION_FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					if (!root.is_object()) {
						continue;
					}
					auto expression = std::make_shared<Expression>();
					expression->id = Lower(e.path().stem().string());
					expression->female = ReadGender(root.value("female", nlohmann::json::object()));
					expression->male = ReadGender(root.value("male", nlohmann::json::object()));
					const std::shared_ptr<const Expression> shared = expression;
					auto file = [&](const char* a_key, std::unordered_map<std::string, ExpressionList>& a_table) {
						if (const auto it = root.find(a_key); it != root.end() && it->is_array()) {
							for (const auto& key : *it) {
								if (key.is_string()) {
									a_table[Lower(key.get<std::string>())].push_back(shared);
								}
							}
						}
					};
					file("events", g_byEvent);
					file("sets", g_bySet);
					file("actionActors", g_byActionActor);
					file("actionTargets", g_byActionTarget);
					++count;
				} catch (const std::exception& ex) {
					REX::WARN("Expressions: {}: couldn't be read ({})", e.path().filename().string(), ex.what());
				}
			}
			REX::INFO("Expressions: {} facial expression(s)", count);
		}

		void EnsureLoaded()
		{
			if (!g_loaded) {
				LoadLocked();
			}
		}

		const ExpressionList* Find(const std::unordered_map<std::string, ExpressionList>& a_table, const std::string& a_key)
		{
			const auto it = a_table.find(Lower(a_key));
			return it != a_table.end() && !it->second.empty() ? &it->second : nullptr;
		}

		// OStim's Node::getFacialExpressions for one role.
		ExpressionList NodeExpressions(const SceneRegistry::Scene& a_scene, std::size_t a_role)
		{
			const auto& part = a_scene.actors[a_role];
			if (!part.underlyingExpression.empty()) {
				if (const auto list = Find(g_bySet, part.underlyingExpression)) {
					return *list;
				}
			}
			auto fromAction = [&](const SceneRegistry::SceneAction& a_action) -> const ExpressionList* {
				if (a_action.target == a_role) {
					if (const auto list = Find(g_byActionTarget, a_action.type->id)) {
						return list;
					}
				}
				if (a_action.actor == a_role) {
					if (const auto list = Find(g_byActionActor, a_action.type->id)) {
						return list;
					}
				}
				return nullptr;
			};
			if (part.expressionAction >= 0 && static_cast<std::size_t>(part.expressionAction) < a_scene.actions.size()) {
				if (const auto list = fromAction(a_scene.actions[part.expressionAction])) {
					return *list;
				}
			}
			for (const auto& action : a_scene.actions) {
				if (const auto list = fromAction(action)) {
					return *list;
				}
			}
			if (const auto list = Find(g_bySet, "default")) {
				return *list;
			}
			return {};
		}

		float Calculate(const Modifier& a_m, float a_speed, float a_excitement)
		{
			float value = a_m.baseValue;
			if (a_m.variance > 0.0F) {
				value += std::uniform_real_distribution<float>(0.0F, a_m.variance)(g_random);
			}
			value += a_m.speedMultiplier * a_speed + a_m.excitementMultiplier * a_excitement;
			return std::clamp(value, 0.0F, 100.0F);
		}

		void AddMapped(Values& a_out, std::size_t a_kind, int a_type, float a_value)
		{
			const auto it = g_morphMap[a_kind].find(a_type);
			if (it == g_morphMap[a_kind].end()) {
				return;
			}
			for (const auto& [morph, weight] : it->second) {
				a_out[morph] = std::max(a_out[morph], std::clamp(a_value * weight, 0.0F, 100.0F));
			}
		}

		std::optional<Layer> MakeLayer(const std::shared_ptr<const Expression>& a_source, bool a_female, float a_speed, float a_excitement)
		{
			if (!a_source) {
				return std::nullopt;
			}
			Layer layer;
			layer.source = a_source;
			layer.expression = a_female ? &a_source->female : &a_source->male;
			const auto& g = *layer.expression;
			if (g.expression) {
				AddMapped(layer.slots[kEmotion], 0, g.expression->type, Calculate(*g.expression, a_speed, a_excitement));
			}
			for (const auto& p : g.phonemes) {
				AddMapped(layer.slots[kMouth], 1, p.type, Calculate(p, a_speed, a_excitement));
			}
			for (const auto& m : g.lids) {
				AddMapped(layer.slots[kLids], 2, m.type, Calculate(m, a_speed, a_excitement));
			}
			for (const auto& m : g.brows) {
				AddMapped(layer.slots[kBrows], 2, m.type, Calculate(m, a_speed, a_excitement));
			}
			for (const auto& m : g.morphs) {
				if (m.type >= 0 && static_cast<std::size_t>(m.type) < MORPHS) {
					auto& v = layer.slots[kMorphs][static_cast<std::size_t>(m.type)];
					v = std::max(v, Calculate(m, a_speed, a_excitement));
				}
			}
			return layer;
		}

		std::shared_ptr<const Expression> Pick(const ExpressionList& a_list)
		{
			if (a_list.empty()) {
				return nullptr;
			}
			return a_list[std::uniform_int_distribution<std::size_t>(0, a_list.size() - 1)(g_random)];
		}

		float RandomDuration()
		{
			const float lo = std::max(g_config.durationMin, 0.1F);
			const float hi = std::max(g_config.durationMax, lo);
			return std::uniform_real_distribution<float>(lo, hi)(g_random);
		}

		// Works out the face's target from the layers (override > event >
		// underlying, per slot), and how fast each morph moves there. Main
		// thread.
		void Retarget(State& a_state)
		{
			a_state.dirty = false;
			Values target{};
			Values rate{};
			for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
				const int   bit = 1 << slot;
				const Layer* layer = nullptr;
				float        speed = 50.0F;
				if (a_state.override_ && (a_state.override_->expression->mask & bit)) {
					layer = &*a_state.override_;
					speed = 250.0F;
				} else if (a_state.event && (a_state.event->expression->mask & bit)) {
					layer = &*a_state.event;
					speed = 150.0F;
				} else if (a_state.underlying && (a_state.underlying->expression->mask & bit)) {
					layer = &*a_state.underlying;
				}
				if (!layer) {
					continue;
				}
				for (std::size_t m = 0; m < MORPHS; ++m) {
					if (layer->slots[slot][m] > target[m]) {
						target[m] = layer->slots[slot][m];
					}
					rate[m] = std::max(rate[m], speed);
				}
			}
			for (std::size_t m = 0; m < MORPHS; ++m) {
				a_state.rate[m] = rate[m] > 0.0F ? rate[m] : 50.0F;  // morphs no layer uses go back to 0 slowly
			}
			a_state.target = target;
		}

		// Moves the face toward its target and sends the morphs whose value
		// changed, with the console's "mfg morphs". Main thread.
		void Step(std::uint32_t a_id, State& a_state, float a_seconds)
		{
			if ((a_state.sendIn -= a_seconds) > 0.0F) {
				return;
			}
			const float elapsed = SEND_INTERVAL - a_state.sendIn;
			a_state.sendIn = SEND_INTERVAL;
			RE::Actor* actor = nullptr;
			for (std::size_t m = 0; m < MORPHS; ++m) {
				const float goal = g_config.disabled ? 0.0F : a_state.target[m];
				auto&       cur = a_state.current[m];
				if (cur != goal) {
					const float step = a_state.rate[m] * elapsed;
					cur = cur < goal ? std::min(cur + step, goal) : std::max(cur - step, goal);
				}
				const int value = static_cast<int>(std::lround(cur));
				if (a_state.anyApplied && static_cast<int>(std::lround(a_state.applied[m])) == value) {
					continue;
				}
				if (!a_state.anyApplied && value == 0) {
					continue;  // nothing to undo yet
				}
				if (!actor) {
					actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
					if (!actor || !actor->Get3D()) {
						return;
					}
				}
				Console::Run(actor, std::format("mfg morphs {} {}", m, value));
				a_state.applied[m] = static_cast<float>(value);
			}
			if (actor) {
				a_state.anyApplied = true;
			}
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

	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role, float a_relativeSpeed)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		if (a_role >= a_scene.actors.size()) {
			return;
		}
		auto&      state = g_states[a_id];
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
		state.female = actor && actor->GetSex() == RE::SEX::kFemale;
		state.speed = a_relativeSpeed;
		if (state.sceneID == a_scene.id) {
			return;  // a speed change: the next pick uses the new speed
		}
		state.sceneID = a_scene.id;
		const float excitement = Excitement::Get(a_id);
		// Override (OStim's updateOverrideExpression) and underlying.
		state.overrides.clear();
		if (const auto& set = a_scene.actors[a_role].expressionOverride; !set.empty()) {
			if (const auto list = Find(g_bySet, set)) {
				state.overrides = *list;
			}
		}
		state.override_ = MakeLayer(Pick(state.overrides), state.female, state.speed, excitement);
		state.overrideIn = state.override_ ? RandomDuration() : -1.0F;
		state.node = NodeExpressions(a_scene, a_role);
		state.underlying = MakeLayer(Pick(state.node), state.female, state.speed, excitement);
		state.underlyingIn = RandomDuration();
		state.dirty = true;
	}

	bool SetEvent(std::uint32_t a_id, const std::string& a_event, bool a_timed)
	{
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		const auto it = g_states.find(a_id);
		const auto list = Find(g_byEvent, a_event);
		if (it == g_states.end() || !list) {
			return false;
		}
		auto& state = it->second;
		state.event = MakeLayer(Pick(*list), state.female, state.speed, Excitement::Get(a_id));
		state.eventIn = a_timed && state.event ? std::max(state.event->expression->duration, 0.1F) : -1.0F;
		state.dirty = true;
		return state.event.has_value();
	}

	void ClearEvent(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		if (const auto it = g_states.find(a_id); it != g_states.end() && it->second.event) {
			it->second.event.reset();
			it->second.eventIn = -1.0F;
			it->second.dirty = true;
		}
	}

	void Tick(float a_seconds)
	{
		std::scoped_lock lock(g_lock);
		for (auto& [id, state] : g_states) {
			const float excitement = Excitement::Get(id);
			if ((state.underlyingIn -= a_seconds) <= 0.0F) {
				state.underlying = MakeLayer(Pick(state.node), state.female, state.speed, excitement);
				state.underlyingIn = RandomDuration();
				state.dirty = true;
			}
			if (state.overrideIn >= 0.0F && (state.overrideIn -= a_seconds) <= 0.0F) {
				state.override_ = MakeLayer(Pick(state.overrides), state.female, state.speed, excitement);
				state.overrideIn = state.override_ ? RandomDuration() : -1.0F;
				state.dirty = true;
			}
			if (state.eventIn >= 0.0F && (state.eventIn -= a_seconds) <= 0.0F) {
				state.event.reset();
				state.eventIn = -1.0F;
				state.dirty = true;
			}
			if (state.dirty) {
				Retarget(state);
			}
			Step(id, state, a_seconds);
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			const auto it = g_states.find(id);
			if (it == g_states.end()) {
				continue;
			}
			if (it->second.anyApplied) {
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
					for (std::size_t m = 0; m < MORPHS; ++m) {
						if (it->second.applied[m] > 0.0F) {
							Console::Run(actor, std::format("mfg morphs {} 0", m));
						}
					}
				}
			}
			g_states.erase(it);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
