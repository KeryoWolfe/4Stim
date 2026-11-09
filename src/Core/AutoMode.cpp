#include "Core/Core.h"

namespace Core
{
	// Sets how long a_scene stays where it is before it moves on.
	void ArmAutoplay(ActiveScene& a_scene)
	{
		a_scene.remaining = -1.0F;
		if (a_scene.sequence) {
			const auto& entry = a_scene.sequence->entries[a_scene.step];
			a_scene.remaining = entry.duration;
		} else if (const auto scene = SceneRegistry::Find(a_scene.sceneID); scene && scene->IsTransition()) {
			// At the motion's end by default: the clip holds its last pose
			// past that, and the game starts blending a one-shot clip back to
			// the base pose ~0.35 s before the clip ends ("IdleStop", see
			// AnimEventHook, which moves on then if the timer hasn't).
			a_scene.remaining = g_settings.transitionLead >= 0.0F ?
			                        std::max(scene->length - g_settings.transitionLead, scene->length * 0.5F) :
			                        scene->length - g_settings.transitionLead;
		}
		if (a_scene.remaining >= 0.0F) {
			StartAutoplayTicks();
		}
	}

	// Plays a_sceneID at a_speed on a running scene's actors (all roles on
	// this frame), and records it: the running-scene list, the focused scene
	// if it's this one, and a SceneChanged event.
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed)
	{
		if (a_active.ending) {
			REX::INFO("Autoplay: \"{}\" not played, \"{}\" is ending", a_sceneID, a_active.sceneID);
			return false;
		}
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() != a_active.actors.size()) {
			REX::WARN("Autoplay: can't play \"{}\" on this scene's {} actor(s)", a_sceneID, a_active.actors.size());
			return false;
		}
		// a_speed -1: OStim's ChangeNode speed (MappedSpeed).
		const int  speed = a_speed < 0 ? MappedSpeed(a_active, *scene) : std::clamp(a_speed, 0, static_cast<int>(scene->speeds.size()) - 1);
		const auto& idles = scene->speeds[speed];
		std::string results;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role]);
			const auto process = actor ? actor->currentProcess : nullptr;
			const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
			if (ok) {
				CuePlayed(a_active.actors[role], idles[role], scene->id);
			}
			results += std::format("{}{:08X} {}", role ? ", " : "", a_active.actors[role], ok ? "ok" : "refused");
		}
		REX::INFO("Autoplay: \"{}\" speed {}: PlayIdle {}", scene->id, speed + 1, results);
		WatchAnimEvents(a_active.actors, scene->id);
		const auto previous = a_active.sceneID;
		const int  previousSpeed = a_active.speed;
		NoteSpeed(a_active, scene->id, speed);
		a_active.sceneID = scene->id;
		a_active.speed = speed;
		a_active.lastPlayed = std::chrono::steady_clock::now();
		a_active.checkedAfterPlay = false;
		RefreshExcitement(a_active);
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			Undress::SceneEntered(a_active.actors, *scene, false);
			EquipObjects::SceneEntered(a_active.actors, *scene, false);
			for (const auto id : a_active.actors) {
				PenisBend::Offset(id, 0);  // OStim's changeNode: the annotation's bend ends with the scene
				PenisBend::Aim(id, false);  // until the new animation's 4StimPenisAim says so
			}
			LockScene(a_active);  // this scene's alignment
		}

		auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_active.actors[0]) {
			focused.sceneID = scene->id;
			focused.speed = speed;
			SetPlayerScene(focused);
		}
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			SceneEvents::SceneChanged(a_active.actors, previous, scene->id, a_active.threadID, speed);
		} else if (speed != previousSpeed) {
			// Every speed change, whoever made it (auto mode, a climax,
			// FourThread.SetSpeed), as OStim's Thread::SetSpeed sends its event.
			SceneEvents::SpeedChanged(a_active.actors, scene->id, speed, static_cast<int>(scene->speeds.size()), a_active.threadID);
		}
		return true;
	}

	// a_active's time is up: on to the next scene.
	void AdvanceAutoplay(ActiveScene& a_active)
	{
		if (!a_active.queuedScene.empty()) {
			const auto queued = std::exchange(a_active.queuedScene, std::string{});
			if (InterruptSequence(a_active, std::format("the player picked \"{}\"", queued))) {
				return;
			}
			REX::INFO("Transition \"{}\" -> \"{}\" (picked during the transition)", a_active.sceneID, queued);
			if (PlayOnActiveScene(a_active, queued, -1)) {
				ArmAutoplay(a_active);
			} else {
				a_active.remaining = -1.0F;
			}
			return;
		}
		if (a_active.sequence) {
			const auto sequence = a_active.sequence;
			if (a_active.step + 1 < sequence->entries.size()) {
				++a_active.step;
				const auto& entry = sequence->entries[a_active.step];
				REX::INFO("Sequence \"{}\": step {}/{}: \"{}\"", sequence->id, a_active.step + 1, sequence->entries.size(), entry.scene);
				if (!PlayOnActiveScene(a_active, entry.scene, entry.speed)) {
					a_active.sequence.reset();
				}
				ArmAutoplay(a_active);
				return;
			}
			REX::INFO("Sequence \"{}\": finished on \"{}\"", sequence->id, a_active.sceneID);
			a_active.sequence.reset();
			SceneEvents::SequenceEnded(a_active.actors, sequence->id, a_active.threadID);
			HUD::OnFocusedSceneChanged();  // navigation back, as OStim's NodeChanged when its sequence ends
			if (a_active.endAfterSequence && !sequence->route) {
				EndActiveScene(a_active, "its sequence ended (EndAfterSequence)");
				a_active.remaining = -1.0F;
				return;
			}
			// Ended on a transition: let it finish moving on.
			const auto scene = SceneRegistry::Find(a_active.sceneID);
			if (!scene || !scene->IsTransition()) {
				a_active.remaining = -1.0F;
				return;
			}
		}
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene || !scene->IsTransition()) {
			a_active.remaining = -1.0F;
			return;
		}
		REX::INFO("Transition \"{}\" -> \"{}\"", scene->id, scene->destination);
		if (PlayOnActiveScene(a_active, scene->destination, -1)) {
			ArmAutoplay(a_active);
		} else {
			a_active.remaining = -1.0F;
		}
	}

	// A corner message, through Papyrus (Debug.Notification).
	void Notify(const std::string& a_text)
	{
		if (g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(g_vm, "Debug"sv, "Notification"sv, callback, a_text);
		}
	}

	// ---- Auto mode (OStim's) ----
	// Moves a scene on by itself: a random fitting scene every so often,
	// foreplay first in some scenes, then intercourse, sometimes pulling out
	// before a man's climax, and speeding up as excitement rises. Scenes are
	// reached by walking the navigation links (through transitions) when
	// they're close enough, else played directly. Main thread (scene clock).

	using AutoStage = ActiveScene::Auto::Stage;
	std::mt19937 g_autoRandom{ std::random_device{}() };

	float AutoRandom(float a_min, float a_max)
	{
		return a_max <= a_min ? a_min : std::uniform_real_distribution<float>(a_min, a_max)(g_autoRandom);
	}

	bool AutoChance(int a_percent)
	{
		return std::uniform_int_distribution<int>(1, 100)(g_autoRandom) <= a_percent;
	}

	bool HasAnyAction(const SceneRegistry::Scene& a_scene, std::initializer_list<std::string_view> a_types)
	{
		return std::ranges::any_of(a_types, [&](std::string_view a_type) { return a_scene.HasAction(a_type); });
	}

	bool IsIntercourse(const SceneRegistry::Scene& a_scene)
	{
		return HasAnyAction(a_scene, { "analsex", "tribbing", "vaginalsex" });
	}

	float MaxExcitement(const ActiveScene& a_active, bool a_menOnly)
	{
		float most = 0.0F;
		for (const auto id : a_active.actors) {
			if (a_menOnly && !ActorProperties::HasPenis(RE::TESForm::GetFormByID<RE::Actor>(id))) {  // OStim: those with a penis
				continue;
			}
			most = std::max(most, Excitement::Get(id));
		}
		return most;
	}

	// Whether a_active's actors, in their roles, may play a_scene where they are.
	bool ActiveCanPlay(const ActiveScene& a_active, const SceneRegistry::Scene& a_scene)
	{
		return a_scene.actors.size() == a_active.actors.size() && SexesFit(a_scene, a_active.actors) &&
		       SceneFitsFurniture(a_scene, a_active.furnitureType);
	}

	// Scenes reachable from a_from in up to a_steps navigations, each with
	// the scenes on the way there (transitions included), the scene last.
	std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, std::vector<std::string>>> Reachable(
		const ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_from, int a_steps)
	{
		using Route = std::vector<std::string>;
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> found;
		std::vector<std::string> visited{ a_from->id };
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> frontier{ { a_from, {} } };
		for (int step = 0; step < a_steps && !frontier.empty(); ++step) {
			std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, Route>> next;
			for (const auto& [scene, route] : frontier) {
				for (const auto& nav : scene->navigations) {
					auto dest = SceneRegistry::Find(nav.to);
					Route path = route;
					// Through transitions to where they settle.
					for (int hop = 0; dest && hop < 8; ++hop) {
						path.push_back(dest->id);
						if (!dest->IsTransition()) {
							break;
						}
						dest = SceneRegistry::Find(dest->destination);
					}
					if (!dest || dest->IsTransition() || !ActiveCanPlay(a_active, *dest) ||
						std::ranges::find(visited, dest->id) != visited.end()) {
						continue;
					}
					visited.push_back(dest->id);
					found.emplace_back(dest, path);
					next.emplace_back(dest, std::move(path));
				}
			}
			frontier = std::move(next);
		}
		return found;
	}

	// Moves a_active to a_target: along a_route if there is one (a short
	// sequence: transitions for their length, other scenes in between for
	// half a second), else directly. Returns the seconds the way takes.
	float AutoGoToNow(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route);

	// As OStim's navigateTo / warpTo: with the player in the scene, and
	// SetUseAutoFades on or the first scene marked fadeOnEntry, it fades to
	// black, moves on 0.7 s in, and fades back 0.55 s later.
	float AutoGoTo(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route)
	{
		const auto first = a_route.empty() ? a_target : SceneRegistry::Find(a_route.front());
		const bool withPlayer = std::ranges::any_of(a_active.actors, [](std::uint32_t id) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(id)); });
		if (!withPlayer || !g_vm || g_sceneFade || !first || !(g_settings.useAutoFades || first->fadeOnEntry)) {
			return AutoGoToNow(a_active, a_target, a_route);
		}
		REX::INFO("Auto mode: fading to \"{}\"", a_target->id);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeToBlack"sv, callback);
		Scheduler::AfterOnMain(std::chrono::milliseconds(700), [role0 = a_active.actors[0], target = a_target, route = a_route]() {
			if (const auto active = FindActiveScene(role0)) {
				AutoGoToNow(*active, target, route);
			}
		});
		Scheduler::After(std::chrono::milliseconds(700 + 550), []() {
			if (g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> after;
				Compat::DispatchStaticCall(g_vm, "FourStimMenu"sv, "FadeFromBlack"sv, after, 0.0F);
			}
		});
		return 1.25F;
	}

	float AutoGoToNow(ActiveScene& a_active, const std::shared_ptr<const SceneRegistry::Scene>& a_target, const std::vector<std::string>& a_route)
	{
		const int finalSpeed = a_target->defaultSpeed;
		if (a_route.size() <= 1) {
			REX::INFO("Auto mode: \"{}\" -> \"{}\"{}", a_active.sceneID, a_target->id, a_route.empty() ? " (no route, played directly)" : "");
			if (PlayOnActiveScene(a_active, a_target->id, finalSpeed)) {
				ArmAutoplay(a_active);
			}
			return 0.0F;
		}
		auto  sequence = std::make_shared<SceneRegistry::Sequence>();
		float total = 0.0F;
		sequence->id = "auto mode";
		sequence->name = "auto mode";
		sequence->route = true;
		sequence->actorCount = a_active.actors.size();
		for (std::size_t i = 0; i < a_route.size(); ++i) {
			const auto scene = SceneRegistry::Find(a_route[i]);
			if (!scene) {
				return 0.0F;
			}
			const bool last = i + 1 == a_route.size();
			const float duration = scene->IsTransition() && scene->length > 0.0F ? scene->length : last ? 0.1F : 0.5F;
			const int   speed = last ? finalSpeed : MappedSpeed(a_active, *scene);
			sequence->entries.push_back({ scene->id, duration, speed });
			total += duration;
		}
		REX::INFO("Auto mode: \"{}\" -> \"{}\" in {} step(s)", a_active.sceneID, a_target->id, a_route.size());
		const auto& first = sequence->entries.front();
		if (!PlayOnActiveScene(a_active, first.scene, first.speed)) {
			return 0.0F;
		}
		a_active.sequence = std::move(sequence);
		a_active.step = 0;
		ArmAutoplay(a_active);
		return total;
	}

	// Moves a_active on to a random scene for auto mode's stage.
	void AutoProgress(ActiveScene& a_active)
	{
		auto& state = a_active.autoMode;
		state.cooldown = AutoRandom(g_autoConfig.sceneMin, g_autoConfig.sceneMax);
		const auto current = SceneRegistry::Find(a_active.sceneID);
		if (!current) {
			return;
		}

		std::function<bool(const SceneRegistry::Scene&)> wanted;
		if (a_active.actors.size() == 1) {
			const bool male = ActorProperties::HasPenis(RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[0]));  // OStim's hasSchlong
			wanted = [male](const SceneRegistry::Scene& s) { return s.HasAction(male ? "malemasturbation" : "femalemasturbation"); };
		} else if (state.stage == AutoStage::kForeplay) {
			wanted = [](const SceneRegistry::Scene& s) { return !IsIntercourse(s) && s.HasActionTag("sexual"); };
		} else {
			wanted = [](const SceneRegistry::Scene& s) { return IsIntercourse(s); };
		}

		const auto reachable = Reachable(a_active, current, g_autoConfig.maxSteps);
		std::vector<std::pair<std::shared_ptr<const SceneRegistry::Scene>, std::vector<std::string>>> candidates;
		auto consider = [&](const std::shared_ptr<const SceneRegistry::Scene>& a_scene, std::vector<std::string> a_route) {
			if (a_scene->id != current->id && !a_scene->noRandomSelection && !a_scene->IsTransition() && wanted(*a_scene)) {
				candidates.emplace_back(a_scene, std::move(a_route));
			}
		};
		if (current->HasActionTag("sexual") && g_autoConfig.limitToNavigation) {
			for (const auto& [scene, route] : reachable) {
				consider(scene, route);
			}
		} else {
			for (const auto& summary : SceneRegistry::List(a_active.actors.size())) {
				const auto scene = SceneRegistry::Find(summary.id);
				if (!scene || !ActiveCanPlay(a_active, *scene)) {
					continue;
				}
				const auto it = std::ranges::find_if(reachable, [&](const auto& a_entry) { return a_entry.first->id == scene->id; });
				consider(scene, it != reachable.end() ? it->second : std::vector<std::string>{});
			}
		}
		// As OStim: off furniture standing scenes, on a bed lying ones, when there are any.
		if (g_autoConfig.standingOnFloor && a_active.actors.size() > 1) {
			const bool onBed = !a_active.furnitureType.empty() && Furniture::IsA(a_active.furnitureType, "bed");
			const bool offFurniture = a_active.furnitureType.empty();
			if (onBed || offFurniture) {
				auto preferred = candidates;
				std::erase_if(preferred, [&](const auto& a_entry) {
					const bool standing = std::ranges::any_of(a_entry.first->actors, [](const SceneRegistry::SceneActor& a) { return a.HasTag("standing"); });
					return offFurniture ? !standing : standing;
				});
				if (!preferred.empty()) {
					candidates = std::move(preferred);
				}
			}
		}
		if (candidates.empty()) {
			REX::INFO("Auto mode: nothing to move \"{}\" on to ({})", a_active.sceneID,
				state.stage == AutoStage::kForeplay ? "foreplay" : a_active.actors.size() == 1 ? "solo" : "intercourse");
			return;
		}
		const auto& pick = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(g_autoRandom)];
		state.cooldown += AutoGoTo(a_active, pick.first, pick.second);
	}

	// A man is about to climax: out to his "pullout" scene, else to a nearby
	// scene with no intercourse where he finishes by hand.
	bool AutoPullOut(ActiveScene& a_active)
	{
		const auto current = SceneRegistry::Find(a_active.sceneID);
		if (!current) {
			return false;
		}
		for (const auto& role : current->actors) {
			if (const auto dest = role.AutoTransition("pullout"); !dest.empty()) {
				if (const auto scene = SceneRegistry::Find(dest); scene && ActiveCanPlay(a_active, *scene)) {
					REX::INFO("Auto mode: pulling out to \"{}\"", dest);
					AutoGoTo(a_active, scene, {});
					return true;
				}
			}
		}
		auto near = Reachable(a_active, current, 3);
		std::erase_if(near, [](const auto& a_entry) {
			return a_entry.first->noRandomSelection || IsIntercourse(*a_entry.first) || !a_entry.first->HasAction("malemasturbation");
		});
		if (near.empty()) {
			return false;
		}
		const auto& pick = near[std::uniform_int_distribution<std::size_t>(0, near.size() - 1)(g_autoRandom)];
		REX::INFO("Auto mode: pulling out");
		AutoGoTo(a_active, pick.first, pick.second);
		return true;
	}

	void StartAutoMode(ActiveScene& a_active)
	{
		auto& state = a_active.autoMode;
		if (state.on) {
			return;
		}
		if (a_active.noAutoMode) {
			// OStim's Thread::startAutoMode: a thread built with NoAutoMode
			// never runs it, whoever asks (the key, FourThread, the C++ API).
			REX::INFO("Auto mode: not for \"{}\", its thread was built with NoAutoMode", a_active.sceneID);
			return;
		}
		if (state.stage == AutoStage::kNone) {
			if (a_active.actors.size() == 1) {
				state.stage = AutoStage::kMain;
			} else {
				if (AutoChance(g_autoConfig.foreplayChance)) {
					state.stage = AutoStage::kForeplay;
					state.foreplayUntil = AutoRandom(g_autoConfig.foreplayMin, g_autoConfig.foreplayMax);
				} else {
					state.stage = AutoStage::kMain;
				}
				state.pulloutAt = AutoChance(g_autoConfig.pulloutChance) ? AutoRandom(g_autoConfig.pulloutMin, g_autoConfig.pulloutMax) : 0.0F;
			}
		}
		state.on = true;
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		// Already in a sexual scene: stay a while. Otherwise move on now.
		state.cooldown = scene && scene->HasActionTag("sexual") ? AutoRandom(g_autoConfig.sceneMin, g_autoConfig.sceneMax) : 0.0F;
		state.speedCooldown = AutoRandom(g_autoConfig.speedIntervalMin, g_autoConfig.speedIntervalMax);
		REX::INFO("Auto mode: on for \"{}\" ({}{})", a_active.sceneID,
			state.stage == AutoStage::kForeplay ? std::format("foreplay to {:.0f}", state.foreplayUntil) : std::string("intercourse"),
			state.pulloutAt > 0.0F ? std::format(", pull-out at {:.0f}", state.pulloutAt) : std::string{});
		StartAutoplayTicks();
	}

	void StopAutoMode(ActiveScene& a_active)
	{
		if (a_active.autoMode.on) {
			a_active.autoMode.on = false;
			REX::INFO("Auto mode: off for \"{}\"", a_active.sceneID);
		}
	}

	// OStim's speed control (Thread::loopAutoControl): faster as excitement
	// rises, every autoSpeedControlInterval, in every NPC scene and, with
	// SetActorSpeedControl (bAutoSpeed), the player's, auto mode or not.
	// The chance is 0 at autoSpeedControlExcitementMin, certain at Max.
	void SpeedControlTick(ActiveScene& a_active, float a_seconds)
	{
		if (a_active.ending || a_active.sequence || a_active.remaining >= 0.0F || !a_active.queuedScene.empty() || a_active.endIn >= 0.0F) {
			return;  // OStim: not while its node queue moves it (a sequence, a route, a transition)
		}
		const bool player = std::ranges::any_of(a_active.actors, [](std::uint32_t a) { return IsPlayer(RE::TESForm::GetFormByID<RE::Actor>(a)); });
		if (player && !g_autoConfig.autoSpeed) {
			return;
		}
		auto& cooldown = a_active.autoMode.speedCooldown;
		if ((cooldown -= a_seconds) > 0.0F) {
			return;
		}
		cooldown = AutoRandom(g_autoConfig.speedIntervalMin, g_autoConfig.speedIntervalMax);
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene) {
			return;
		}
		const float excitement = MaxExcitement(a_active, false);
		const float span = std::max(g_autoConfig.speedExcitementMax - g_autoConfig.speedExcitementMin, 1.0F);
		const int   chance = static_cast<int>(std::clamp((excitement - g_autoConfig.speedExcitementMin) * 100.0F / span, 0.0F, 100.0F));
		if (a_active.speed + 1 < static_cast<int>(scene->speeds.size()) && AutoChance(chance)) {
			PlayOnActiveScene(a_active, scene->id, a_active.speed + 1);  // sends the speed event
		}
	}

	void AutoModeTick(ActiveScene& a_active, float a_seconds)
	{
		auto& state = a_active.autoMode;
		// Not while it's moving (a transition or a route), or ending.
		if (!state.on || a_active.sequence || a_active.remaining >= 0.0F || !a_active.queuedScene.empty() || a_active.endIn >= 0.0F) {
			return;
		}
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene) {
			return;
		}

		if (state.stage == AutoStage::kPullout) {
			return;  // until the climax (HandleClimax)
		}
		if (state.stage == AutoStage::kForeplay && MaxExcitement(a_active, false) > state.foreplayUntil) {
			REX::INFO("Auto mode: foreplay over for \"{}\"", a_active.sceneID);
			state.stage = AutoStage::kMain;
			state.cooldown = 0.0F;
		}
		if (state.stage == AutoStage::kMain && state.pulloutAt > 0.0F && IsIntercourse(*scene) && MaxExcitement(a_active, true) > state.pulloutAt) {
			state.stage = AutoStage::kPullout;
			AutoPullOut(a_active);
			return;
		}
		if ((state.cooldown -= a_seconds) < 0.0F) {
			AutoProgress(a_active);
		}
	}
}
