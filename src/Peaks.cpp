#include "Peaks.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "Actions.h"
#include "SexToys.h"
#include "Sounds.h"

namespace Peaks
{
	namespace
	{
		// OStim waits this long after a scene change before measuring bones:
		// the blend from the last animation throws the distances off.
		constexpr float INITIAL_COOLDOWN = 1.0F;
		// OStim: the sound engine takes 50-100 ms to start a sound, so a loop
		// sound waits this much longer before its next one.
		constexpr float LOOP_SOUND_LEAD = 0.15F;

		using Bones = std::vector<RE::NiPointer<RE::NiAVObject>>;

		Bones FindBones(std::uint32_t a_actorID, const std::vector<std::string>& a_names)
		{
			Bones out;
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actorID);
			const auto root = actor ? actor->Get3D(false) : nullptr;
			if (!root) {
				return out;
			}
			for (const auto& name : a_names) {
				if (const auto bone = root->GetObjectByName(RE::BSFixedString(name))) {
					out.emplace_back(bone);
				}
			}
			return out;
		}

		// The shortest distance between any of a_from and any of a_to
		// (OStim's MultiBoneDistancePeakHandler).
		float Distance(const Bones& a_from, const Bones& a_to)
		{
			float best = std::numeric_limits<float>::quiet_NaN();
			for (const auto& a : a_from) {
				for (const auto& b : a_to) {
					const auto& p = a->world.translate;
					const auto& q = b->world.translate;
					const float dx = p.x - q.x;
					const float dy = p.y - q.y;
					const float dz = p.z - q.z;
					const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
					if (std::isnan(best) || d < best) {
						best = d;
					}
				}
			}
			return best;
		}

		// Tracks the smallest and largest distance seen, as OStim's distance
		// handlers do, so any animation's range works.
		struct Range
		{
			float minDistance = -1.0F;
			float maxDistance = -1.0F;

			// Returns false until the range is at least a_minRange: bones that
			// barely move (an actor swaying in a scene with no real thrusting)
			// give no peaks. 4Stim's: OStim's Skyrim genital bones always
			// move far enough; Fallout 4's pelvises can sway a little.
			bool Add(float a_distance, float a_minRange)
			{
				if (minDistance < 0.0F || a_distance < minDistance) {
					minDistance = a_distance;
				}
				if (maxDistance < 0.0F || a_distance > maxDistance) {
					maxDistance = a_distance;
				}
				return maxDistance - minDistance >= std::max(a_minRange, 0.001F);
			}
			[[nodiscard]] float At(float a_fraction) const { return minDistance + (maxDistance - minDistance) * a_fraction; }
		};

		// OStim's peak handlers, one per action with a "peak".
		struct PeakHandler
		{
			std::size_t          action = 0;
			Actions::Peak::Kind  kind = Actions::Peak::Kind::kNone;
			// loop
			float                interval = 1.0F;
			float                countdown = 1.0F;
			float                minRange = 3.0F;
			// bonedistance (OStim's DistancePeakHandler)
			bool                 inverse = false;
			Bones                actorBones;
			Bones                targetBones;
			float                cooldown = INITIAL_COOLDOWN;
			Range                range;
			bool                 movingIn = false;
		};

		// OStim's sound players, one per action sound.
		struct SoundPlayer
		{
			std::size_t                 action = 0;
			const Actions::ActionSound* sound = nullptr;  // kept alive by the scene's action type
			std::uint32_t               actor = 0;        // plays on the action actor
			std::uint32_t               target = 0;       // the action's target (muteWithTarget)
			// bonedistance (OStim's BoneDistanceSoundPlayer)
			Bones                       actorBones;
			Bones                       targetBones;
			float                       cooldown = INITIAL_COOLDOWN;
			Range                       range;
			bool                        inThreshold = false;
			float                       sinceLast = 0.0F;
			// loop
			float                       delay = 0.0F;
			Sounds::Playing             playing;
		};

		struct SceneState
		{
			std::vector<std::uint32_t>                   actors;
			std::shared_ptr<const SceneRegistry::Scene> scene;
			float                                        speedScale = 1.0F;  // OStim: the speed's displaySpeed
			std::vector<PeakHandler>                     peaks;
			std::vector<SoundPlayer>                     sounds;
		};

		std::vector<SceneState> g_scenes;

		float SpeedScale(const SceneRegistry::Scene& a_scene, int a_speed)
		{
			if (a_speed >= 0 && static_cast<std::size_t>(a_speed) < a_scene.speedInfo.size()) {
				const float display = a_scene.speedInfo[static_cast<std::size_t>(a_speed)].displaySpeed;
				if (display > 0.0F) {
					return display;
				}
			}
			return 1.0F;  // no displaySpeed given: the interval as written
		}

		SceneState* Find(std::uint32_t a_actorID)
		{
			const auto it = std::ranges::find_if(g_scenes, [&](const SceneState& s) { return std::ranges::find(s.actors, a_actorID) != s.actors.end(); });
			return it != g_scenes.end() ? &*it : nullptr;
		}

		void Play(const SoundPlayer& a_player, Sounds::Playing* a_out = nullptr)
		{
			// OStim's muteWithActor / muteWithTarget: silent while that actor is muted.
			if ((a_player.sound->muteWithActor && Sounds::IsMuted(a_player.actor)) || (a_player.sound->muteWithTarget && Sounds::IsMuted(a_player.target))) {
				return;
			}
			auto playing = Sounds::Play(a_player.sound->sound, RE::TESForm::GetFormByID<RE::Actor>(a_player.actor), Sounds::Settings().soundVolume);
			if (a_out) {
				*a_out = playing;
			}
		}

		// OStim's NodeHandler::sendPeak: the action's "peak" sounds.
		void SendPeak(SceneState& a_state, std::size_t a_action)
		{
			SexToys::Peak(a_state.actors, a_action);  // OStim's ToyThread peak listener
			for (const auto& player : a_state.sounds) {
				if (player.action == a_action && player.sound->kind == Actions::ActionSound::Kind::kPeak) {
					Play(player);
				}
			}
		}

		// The scene's peak handlers and sound players (OStim's nodeChanged).
		void Build(SceneState& a_state, bool a_playerScene)
		{
			a_state.peaks.clear();
			a_state.sounds.clear();
			const auto& scene = *a_state.scene;
			for (std::size_t i = 0; i < scene.actions.size(); ++i) {
				const auto& action = scene.actions[i];
				if (action.actor >= a_state.actors.size() || action.target >= a_state.actors.size()) {
					continue;
				}
				const auto  actorID = a_state.actors[action.actor];
				const auto  targetID = a_state.actors[action.target];
				const auto& peak = action.type->peak;
				if (peak.kind != Actions::Peak::Kind::kNone && action.doPeaks && !action.peaksAnnotated) {
					PeakHandler handler;
					handler.action = i;
					handler.kind = peak.kind;
					handler.interval = peak.interval;
					handler.countdown = peak.interval;
					handler.inverse = peak.inverse;
					handler.minRange = peak.minRange;
					if (peak.kind == Actions::Peak::Kind::kBoneDistance) {
						handler.actorBones = FindBones(actorID, peak.actorBones);
						handler.targetBones = FindBones(targetID, peak.targetBones);
						if (handler.actorBones.empty() || handler.targetBones.empty()) {
							REX::INFO("Peaks: \"{}\" action {} ({}): bones not found on its actors, no peaks", scene.id, i, action.type->id);
							continue;
						}
					}
					a_state.peaks.push_back(std::move(handler));
				}
				if (action.muted) {
					continue;
				}
				for (const auto& sound : action.type->sounds) {
					if (sound.playerThreadOnly && !a_playerScene) {
						continue;
					}
					SoundPlayer player;
					player.action = i;
					player.sound = &sound;
					player.actor = actorID;
					player.target = targetID;
					player.delay = sound.delay;
					if (sound.kind == Actions::ActionSound::Kind::kBoneDistance) {
						player.actorBones = FindBones(actorID, sound.actorBones);
						player.targetBones = FindBones(targetID, sound.targetBones);
						if (player.actorBones.empty() || player.targetBones.empty()) {
							continue;  // as OStim: no bones, no player
						}
					}
					a_state.sounds.push_back(std::move(player));
				}
			}
			if (!a_state.peaks.empty() || !a_state.sounds.empty()) {
				REX::INFO("Peaks: \"{}\": {} peak finder(s), {} action sound(s)", scene.id, a_state.peaks.size(), a_state.sounds.size());
			}
		}

		void TickPeak(SceneState& a_state, PeakHandler& a_peak, float a_seconds)
		{
			if (a_peak.kind == Actions::Peak::Kind::kLoop) {
				// OStim's LoopPeakHandler: the interval divided by the speed's displaySpeed.
				a_peak.countdown -= a_seconds * a_state.speedScale;
				if (a_peak.countdown > 0.0F) {
					return;
				}
				SendPeak(a_state, a_peak.action);
				a_peak.countdown = a_peak.interval;
				return;
			}
			// OStim's DistancePeakHandler: a peak as the bones come within 10%
			// of their closest (or, inverse, as they leave it).
			if (a_peak.cooldown > 0.0F) {
				a_peak.cooldown -= a_seconds;
				return;
			}
			const float distance = Distance(a_peak.actorBones, a_peak.targetBones);
			if (std::isnan(distance) || !a_peak.range.Add(distance, a_peak.minRange)) {
				return;
			}
			if (a_peak.movingIn) {
				if (distance < a_peak.range.At(0.1F)) {
					if (!a_peak.inverse) {
						SendPeak(a_state, a_peak.action);
					}
					a_peak.movingIn = false;
				}
			} else if (distance > a_peak.range.At(0.9F)) {
				if (a_peak.inverse) {
					SendPeak(a_state, a_peak.action);
				}
				a_peak.movingIn = true;
			}
		}

		void TickSound(SoundPlayer& a_player, float a_seconds)
		{
			const auto& sound = *a_player.sound;
			if (sound.kind == Actions::ActionSound::Kind::kLoop) {
				// OStim's LoopSoundPlayer: the next one a delay after this one ends.
				if (a_player.playing.IsPlaying()) {
					return;
				}
				if ((a_player.delay -= a_seconds) > 0.0F) {
					return;
				}
				a_player.delay = sound.delay + LOOP_SOUND_LEAD;
				Play(a_player, &a_player.playing);
				return;
			}
			if (sound.kind != Actions::ActionSound::Kind::kBoneDistance) {
				return;  // peak sounds wait for peaks
			}
			// OStim's BoneDistanceSoundPlayer: plays as the bones leave the
			// closest 10% of their range (inverse: the furthest 10%), not
			// sooner than minInterval or later than maxInterval after the last.
			if (a_player.cooldown > 0.0F) {
				a_player.cooldown -= a_seconds;
				return;
			}
			const float distance = Distance(a_player.actorBones, a_player.targetBones);
			if (std::isnan(distance) || !a_player.range.Add(distance, sound.minRange)) {
				a_player.sinceLast += a_seconds;
				return;
			}
			const float threshold = a_player.range.At(sound.inverse ? 0.9F : 0.1F);
			const bool  inThreshold = sound.inverse ? distance > threshold : distance < threshold;
			if (a_player.inThreshold && !inThreshold) {
				if (a_player.sinceLast >= sound.minInterval && (sound.maxInterval <= 0.0F || a_player.sinceLast <= sound.maxInterval)) {
					Play(a_player);
				}
				a_player.sinceLast = 0.0F;
			}
			a_player.inThreshold = inThreshold;
			a_player.sinceLast += a_seconds;
		}
	}

	void Enter(const std::vector<std::uint32_t>& a_actors, const std::shared_ptr<const SceneRegistry::Scene>& a_scene, int a_speed, bool a_playerScene)
	{
		if (a_actors.empty() || !a_scene) {
			return;
		}
		// One state per running scene: drop any other that shares an actor
		// (a scene replaced by another).
		const auto shares = [&](const SceneState& s) {
			return std::ranges::any_of(s.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
		};
		bool kept = false;
		std::erase_if(g_scenes, [&](const SceneState& s) {
			if (!shares(s)) {
				return false;
			}
			if (!kept && std::ranges::find(s.actors, a_actors.front()) != s.actors.end()) {
				kept = true;
				return false;
			}
			return true;
		});
		auto state = Find(a_actors.front());
		if (!state) {
			state = &g_scenes.emplace_back();
		}
		const bool changed = state->scene != a_scene || state->actors != a_actors;
		state->actors = a_actors;
		state->speedScale = SpeedScale(*a_scene, a_speed);
		if (changed) {
			state->scene = a_scene;
			Build(*state, a_playerScene);
		}
	}

	void Annotated(std::uint32_t a_actorID, int a_action)
	{
		const auto state = Find(a_actorID);
		if (!state || a_action < 0 || static_cast<std::size_t>(a_action) >= state->scene->actions.size()) {
			return;
		}
		// As OStim's handlePeakAnnotation: only for actions that say so.
		const auto& action = state->scene->actions[static_cast<std::size_t>(a_action)];
		if (!action.doPeaks || !action.peaksAnnotated) {
			return;
		}
		SendPeak(*state, static_cast<std::size_t>(a_action));
	}

	void Tick(float a_seconds)
	{
		for (auto& state : g_scenes) {
			for (auto& peak : state.peaks) {
				TickPeak(state, peak, a_seconds);
			}
			for (auto& player : state.sounds) {
				TickSound(player, a_seconds);
			}
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_actors)
	{
		std::erase_if(g_scenes, [&](SceneState& s) {
			const bool gone = std::ranges::any_of(s.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
			if (gone) {
				for (auto& player : s.sounds) {
					player.playing.Stop();
				}
			}
			return gone;
		});
	}

	void Clear()
	{
		g_scenes.clear();
	}
}
