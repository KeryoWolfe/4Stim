#include "Excitement.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "Actions.h"

namespace Excitement
{
	namespace
	{
		struct State
		{
			SceneRegistry::Sex sex = SceneRegistry::Sex::kAny;
			float excitement = 0.0F;
			float base = 0.0F;      // per second from the actions, with speed (before multipliers)
			float ceiling = 0.0F;   // what the scene's actions allow
			float cooldown = 0.0F;  // decay grace left
			float multiplier = 1.0F;
			int   climaxes = 0;
			bool  stalled = false;
			bool  climaxing = false;  // reported by Tick, waiting for Climaxed
		};

		Config                                  g_config;
		std::mutex                              g_lock;
		std::unordered_map<std::uint32_t, State> g_states;

		float SexMult(SceneRegistry::Sex a_sex)
		{
			return a_sex == SceneRegistry::Sex::kFemale ? g_config.femaleMult : g_config.maleMult;
		}

		float Rate(const State& a_state)
		{
			return a_state.base * SexMult(a_state.sex) * a_state.multiplier;
		}

		State* Find(std::uint32_t a_id)
		{
			const auto it = g_states.find(a_id);
			return it != g_states.end() ? &it->second : nullptr;
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void Enter(const std::vector<std::uint32_t>& a_ids, const std::vector<SceneRegistry::Sex>& a_sexes,
		const SceneRegistry::Scene& a_scene, int a_speed)
	{
		const auto speeds = std::max<std::size_t>(a_scene.speeds.size(), 1);
		const float speedMod = 1.0F + static_cast<float>(std::clamp(a_speed, 0, static_cast<int>(speeds) - 1)) / static_cast<float>(speeds);

		std::scoped_lock lock(g_lock);
		for (std::size_t role = 0; role < a_ids.size(); ++role) {
			auto& state = g_states[a_ids[role]];
			if (role < a_sexes.size()) {
				state.sex = a_sexes[role];
			}

			// Every side of every action this role is on: actor, target,
			// performer (masturbation is actor and target at once).
			std::vector<float> stimulations;
			float              ceiling = 0.0F;
			for (const auto& action : a_scene.actions) {
				auto take = [&](std::size_t a_role, const Actions::Side& a_side) {
					if (a_role == role && a_side.stimulation != 0.0F) {
						stimulations.push_back(a_side.stimulation);
						ceiling = std::max(ceiling, a_side.maxStimulation);
					}
				};
				take(action.actor, action.type->actor);
				take(action.target, action.type->target);
				take(action.performer, action.type->performer);
			}
			std::ranges::sort(stimulations, std::greater<>());
			float base = 0.0F;
			for (std::size_t i = 0; i < stimulations.size(); ++i) {
				base += i == 0 ? stimulations[i] : stimulations[i] * 0.1F;
			}
			state.base = base * speedMod;
			state.ceiling = std::min(ceiling, 100.0F);
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			g_states.erase(id);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}

	std::vector<std::uint32_t> Tick(float a_seconds)
	{
		std::vector<std::uint32_t> climaxing;
		if (!g_config.enabled) {
			return climaxing;
		}
		std::scoped_lock lock(g_lock);
		for (auto& [id, s] : g_states) {
			if (s.climaxing) {
				continue;
			}
			if (s.excitement > s.ceiling) {
				// Above what this scene gives: fall back after the grace.
				if (s.cooldown > 0.0F) {
					s.cooldown -= a_seconds;
				} else {
					s.excitement = std::max(s.excitement - g_config.decayRate * a_seconds, s.ceiling);
				}
			} else if (const float rate = Rate(s); rate > 0.0F) {
				s.excitement = std::min(s.excitement + rate * a_seconds, s.ceiling);
				s.cooldown = g_config.decayGrace;
			}
			if (s.excitement >= 100.0F) {
				s.excitement = 100.0F;
				if (!s.stalled) {
					s.climaxing = true;
					climaxing.push_back(id);
				}
			}
		}
		return climaxing;
	}

	void Climaxed(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto s = Find(a_id);
		if (!s) {
			return;
		}
		s->climaxing = false;
		++s->climaxes;
		s->excitement = s->sex == SceneRegistry::Sex::kFemale ?
		                    std::min(g_config.postClimax * static_cast<float>(s->climaxes), g_config.postClimaxMax) :
		                    0.0F;
		s->cooldown = g_config.decayGrace;
	}

	float Get(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto s = Find(a_id);
		return s ? s->excitement : -1.0F;
	}

	void Set(std::uint32_t a_id, float a_value)
	{
		std::scoped_lock lock(g_lock);
		if (const auto s = Find(a_id)) {
			s->excitement = std::clamp(a_value, 0.0F, 100.0F);
			s->cooldown = g_config.decayGrace;
		}
	}

	void Add(std::uint32_t a_id, float a_value, bool a_useMultiplier)
	{
		std::scoped_lock lock(g_lock);
		if (const auto s = Find(a_id)) {
			const float mult = a_useMultiplier ? SexMult(s->sex) * s->multiplier : 1.0F;
			s->excitement = std::clamp(s->excitement + a_value * mult, 0.0F, 100.0F);
			s->cooldown = g_config.decayGrace;
		}
	}

	int TimesClimaxed(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto s = Find(a_id);
		return s ? s->climaxes : 0;
	}

	float Multiplier(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto s = Find(a_id);
		return s ? s->multiplier : 1.0F;
	}

	void SetMultiplier(std::uint32_t a_id, float a_multiplier)
	{
		std::scoped_lock lock(g_lock);
		if (const auto s = Find(a_id)) {
			s->multiplier = std::max(a_multiplier, 0.0F);
		}
	}

	void SetStalled(std::uint32_t a_id, bool a_stalled)
	{
		std::scoped_lock lock(g_lock);
		if (const auto s = Find(a_id)) {
			s->stalled = a_stalled;
		}
	}

	bool IsStalled(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto s = Find(a_id);
		return s && s->stalled;
	}

	float TimeUntilClimax(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto s = Find(a_id);
		const float rate = s ? Rate(*s) : 0.0F;
		if (!s || s->ceiling < 100.0F || rate <= 0.0F || s->stalled) {
			return -1.0F;
		}
		return std::max(100.0F - s->excitement, 0.0F) / rate;
	}
}
