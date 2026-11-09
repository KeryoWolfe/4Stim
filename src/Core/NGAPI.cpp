#include "Core/Core.h"

namespace Core::API
{
	// OStim's Scene API starts a thread from its actors, furniture and starting scene.
	NG::Scene::APIResult StartNGScene(const char* a_plugin, RE::TESObjectREFR* a_furniture, const char* a_scene,
		std::vector<RE::Actor*> a_actors, std::uint32_t* a_threadID)
	{
		if (a_actors.empty() || std::ranges::any_of(a_actors, [](RE::Actor* a) { return a == nullptr; })) {
			return NG::Scene::APIResult::Invalid;
		}
		const auto builder = BuilderCreate(std::move(a_actors));
		if (builder < 0) {
			return NG::Scene::APIResult::Failed;
		}
		if (a_furniture) {
			Natives::FourThreadBuilder::SetFurniture({}, builder, a_furniture);
		}
		if (a_scene && SceneRegistry::Find(a_scene)) {
			Natives::FourThreadBuilder::SetStartingAnimation({}, builder, a_scene);
		}
		const auto id = Natives::FourThreadBuilder::Start({}, builder);
		if (id < 0) {
			return NG::Scene::APIResult::Failed;
		}
		if (a_threadID) {
			*a_threadID = static_cast<std::uint32_t>(id);
		}
		REX::INFO("Scene API: thread {} started by {}", id, a_plugin ? a_plugin : "a plugin");
		return NG::Scene::APIResult::OK;
	}

	// The scene a navigation ends up at (past its transitions).
	std::shared_ptr<const SceneRegistry::Scene> NavDestination(const std::string& a_to)
	{
		const auto scene = SceneRegistry::Find(a_to);
		return scene ? SceneRegistry::Settled(scene) : nullptr;
	}

	FourStim::FocusedScene AsFocused(const ActiveScene& a_active)
	{
		FourStim::FocusedScene f;
		f.role0 = a_active.actors.empty() ? 0 : a_active.actors[0];
		f.role1 = a_active.actors.size() > 1 ? a_active.actors[1] : 0;
		if (a_active.actors.size() > 2) {
			f.more.assign(a_active.actors.begin() + 2, a_active.actors.end());
		}
		f.sceneID = a_active.sceneID;
		f.speed = a_active.speed;
		f.furniture = a_active.furnitureType;
		return f;
	}

	NGThreadInterface* NGThreads()
	{
		static NGThreadInterface threads;
		static AlignmentInterfaceImpl alignment;
		threads.alignment = &alignment;
		return &threads;
	}

	NGSceneInterface* NGScenes()
	{
		static NGSceneInterface scenes;
		return &scenes;
	}

	void NotifyNG(FourStimNG_API::Thread::ThreadEvent a_event, std::uint32_t a_threadID)
	{
		NGThreads()->Notify(a_event, a_threadID);
	}

	// OStim's ThreadManager::migrateThread: the old thread ends, and once
	// its actors are free (and a_delayMs more), a new one starts with
	// a_actors on the same furniture, auto mode as it was, the actors'
	// excitement carried over. a_onComplete gets the new ID or -1.
	std::int32_t NGThreadInterface::MigrateThread(std::uint32_t a_id, const std::uint32_t* a_actors, std::uint32_t a_count,
		void (*a_onComplete)(std::int32_t, void*), void* a_context, int a_delayMs) noexcept
	{
		const auto fail = [&]() {
			if (a_onComplete) {
				a_onComplete(-1, a_context);
			}
			return -1;
		};
		const auto oldID = static_cast<std::int32_t>(a_id);
		const auto active = FindThread(oldID);
		if (!active || !a_actors || a_count == 0) {
			return fail();
		}
		std::vector<std::uint32_t> cast(a_actors, a_actors + a_count);
		if (std::ranges::any_of(cast, [](std::uint32_t a) { return !RE::TESForm::GetFormByID<RE::Actor>(a); })) {
			return fail();
		}
		if (!a_onComplete && std::this_thread::get_id() == g_mainThread) {
			REX::WARN("Thread API: MigrateThread without a callback on the main thread would wait on itself: refused");
			return -1;
		}
		const auto furniture = active->furnitureRef;
		const bool autoMode = active->autoMode.on;
		std::vector<std::pair<std::uint32_t, float>> excitement;
		for (const auto id : cast) {
			excitement.emplace_back(id, Excitement::Get(id));
		}
		auto promise = std::make_shared<std::promise<std::int32_t>>();
		auto future = promise->get_future();
		const auto done = [a_onComplete, a_context, promise](std::int32_t a_newID) {
			if (a_onComplete) {
				a_onComplete(a_newID, a_context);
			} else {
				promise->set_value(a_newID);
			}
		};
		WithThread(oldID, [](ActiveScene& a) { EndActiveScene(a, "moved to a new thread by another plugin (Thread API MigrateThread)"); });
		std::thread([oldID, cast, furniture, autoMode, excitement, done, a_delayMs]() {
			// Wait for the old thread's actors to be released (10 s at most).
			for (int i = 0; i < 100; ++i) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				auto free = std::make_shared<std::promise<bool>>();
				auto answer = free->get_future();
				F4SE::GetTaskInterface()->AddTask([free, oldID, cast]() {
					free->set_value(!FindThread(oldID) && std::ranges::none_of(cast, [](std::uint32_t a) { return FindActiveScene(a) != nullptr; }));
				});
				if (answer.wait_for(std::chrono::seconds(2)) == std::future_status::ready && answer.get()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(std::max(a_delayMs, 0)));
					F4SE::GetTaskInterface()->AddTask([cast, furniture, autoMode, excitement, done]() {
						std::vector<RE::Actor*> actors;
						for (const auto id : cast) {
							actors.push_back(RE::TESForm::GetFormByID<RE::Actor>(id));
						}
						const auto builder = std::ranges::any_of(actors, [](RE::Actor* a) { return !a; }) ? -1 : BuilderCreate(actors);
						if (builder < 0) {
							done(-1);
							return;
						}
						if (const auto ref = furniture ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(furniture) : nullptr) {
							Natives::FourThreadBuilder::SetFurniture({}, builder, ref);
						}
						if (!autoMode) {
							Natives::FourThreadBuilder::NoAutoMode({}, builder);
						}
						const auto id = Natives::FourThreadBuilder::Start({}, builder);
						if (id >= 0) {
							for (const auto& [actor, value] : excitement) {
								Excitement::Set(actor, value);
							}
							if (autoMode) {
								Natives::FourThread::StartAutoMode({}, id);
							}
						}
						REX::INFO("Thread API: MigrateThread: new thread {}", id);
						done(id);
					});
					return;
				}
			}
			REX::WARN("Thread API: MigrateThread: thread {}'s actors weren't released in time", oldID);
			done(-1);
		}).detach();
		if (a_onComplete) {
			return 0;
		}
		return future.get();
	}

	// FourStimTest.PluginAPI's check: 4Stim's own exports, found the way
	// another plugin finds them (GetProcAddress on 4Stim.dll), and the
	// player's thread read through them. The event and control callbacks
	// it registers log to 4Stim.log from then on.
	void LogNGEvent(NG::Thread::ThreadEvent a_event, std::uint32_t a_thread, void*)
	{
		REX::INFO("Thread API test: event {} on thread {}", static_cast<int>(a_event), a_thread);
	}
	void LogNGControl(NG::Thread::Controls a_control, std::uint32_t a_thread, void*)
	{
		REX::INFO("Thread API test: control {} on thread {}", static_cast<int>(a_control), a_thread);
	}

	std::string SelfTest()
	{
		std::string out;
		const auto  line = [&](bool a_ok, const std::string& a_text) {
			out += std::format("{} {}\n", a_ok ? "ok" : "FAIL", a_text);
			REX::INFO("Thread API test: {} {}", a_ok ? "ok" : "FAIL", a_text);
		};
		const auto module = REX::W32::GetModuleHandleA("4Stim.dll");
		const auto mapFn = module ? reinterpret_cast<FourStimAPI::InterfaceMap* (*)()>(REX::W32::GetProcAddress(module, "requestInterfaceMap")) : nullptr;
		const auto sceneFn = module ? reinterpret_cast<NG::Scene::_RequestPluginAPI_Scene>(REX::W32::GetProcAddress(module, "RequestPluginAPI_Scene")) : nullptr;
		const auto threadFn = module ? reinterpret_cast<NG::Thread::_RequestPluginAPI_Thread>(REX::W32::GetProcAddress(module, "RequestPluginAPI_Thread")) : nullptr;
		line(module != nullptr, "4Stim.dll found");
		line(mapFn && mapFn() && mapFn()->queryInterface(FourStimAPI::ThreadInterface::NAME), "requestInterfaceMap: \"Threads\" in it");
		const auto scenes = sceneFn ? sceneFn(NG::Scene::InterfaceVersion::V1, "4StimSelfTest", { 1, 0, 0, 0 }) : nullptr;
		const auto threads = threadFn ? threadFn(NG::Thread::InterfaceVersion::V1, "4StimSelfTest", { 1, 0, 0, 0 }) : nullptr;
		line(scenes != nullptr, "RequestPluginAPI_Scene");
		line(threads != nullptr, "RequestPluginAPI_Thread");
		if (!threads || !scenes) {
			return out;
		}
		threads->UnregisterEventCallback(LogNGEvent);
		threads->RegisterEventCallback(LogNGEvent, nullptr);
		threads->UnregisterControlCallback(LogNGControl);
		threads->RegisterControlCallback(LogNGControl, nullptr);
		NG::Thread::KeyData keys{};
		threads->GetKeyData(&keys);
		line(true, std::format("GetKeyData: end {:X}, search {:X}, align {:X}, up {:X}", keys.keyEnd, keys.keySearch, keys.keyAlignment, keys.keyUp));
		const std::uint32_t id = threads->GetPlayerThreadID();
		if (!threads->IsThreadValid(id)) {
			line(true, "no player thread: start a scene and run it again for the rest");
			return out;
		}
		const auto sceneID = threads->GetCurrentSceneID(id);
		line(sceneID != nullptr, std::format("GetCurrentSceneID: {}", sceneID ? sceneID : "(none)"));
		const auto name = threads->GetCurrentNodeName(id);
		line(name != nullptr, std::format("GetCurrentNodeName: {}", name ? name : "(none)"));
		NG::Thread::ActorData actors[8]{};
		const auto count = threads->GetActors(id, actors, 8);
		line(count == threads->GetActorCount(id) && count > 0, std::format("GetActors: {} (excitement {:.0f}, female {}, schlong {})", count, actors[0].excitement, actors[0].isFemale, actors[0].hasSchlong));
		line(threads->GetActorPosition(id, actors[0].formID) == 0 && threads->IsActorInAnyThread(actors[0].formID), "GetActorPosition / IsActorInAnyThread");
		std::vector<std::uint32_t> ids;
		for (std::uint32_t i = 0; i < count; ++i) {
			ids.push_back(actors[i].formID);
		}
		line(threads->HasCompatibleNode(id, ids.data(), count), "HasCompatibleNode (the same actors)");
		NG::Thread::NavigationData navs[32]{};
		const auto navCount = threads->GetNavigationOptions(id, navs, 32);
		line(navCount == threads->GetNavigationCount(id), std::format("GetNavigationOptions: {}{}", navCount, navCount ? std::format(" (first: {} -> {}, \"{}\")", navs[0].sceneId, navs[0].destinationId, navs[0].description) : ""));
		line(true, std::format("speed {} of {}, transition {}, sequence {}, auto mode {}, player control off {}", threads->GetCurrentSpeed(id), threads->GetMaxSpeed(id),
			threads->IsTransition(id), threads->IsInSequence(id), threads->IsAutoMode(id), threads->IsPlayerControlDisabled(id)));
		bool autoMode = false;
		line(scenes->TryGetAutoMode("4StimSelfTest", id, &autoMode) == NG::Scene::APIResult::OK && autoMode == threads->IsAutoMode(id), "Scene API TryGetAutoMode");
		NG::Thread::ActorAlignmentData align{};
		const bool aligned = threads->GetActorAlignment(id, 0, &align);  // before the text: argument order isn't left to right
		line(aligned, std::format("GetActorAlignment 0: {:.1f} {:.1f} {:.1f}, scale {:.2f}, rotation {:.1f}, bend {:.1f}", align.offsetX, align.offsetY, align.offsetZ, align.scale, align.rotation, align.sosBend));
		NG::Thread::SceneSearchResult found[8]{};
		const auto hits = threads->SearchScenes("a", found, 8);
		line(true, std::format("SearchScenes \"a\": {}{}", hits, hits ? std::format(" (first: {})", found[0].name) : ""));
		NG::Thread::SceneSearchResult info{};
		line(sceneID && threads->GetSceneInfo(sceneID, &info) && info.actorCount == count, "GetSceneInfo (the current scene)");
		NG::Thread::OptionsMenuItem items[32]{};
		const auto itemCount = threads->GetOptionsItems(items, 32);
		line(itemCount == threads->GetOptionsItemCount() + 1 && std::string_view(items[0].id) == "-1", std::format("GetOptionsItems: back / exit + {}, at root {}", itemCount - 1, threads->IsOptionsAtRoot()));
		return out;
	}

	// A navigation-type key pressed in a scene the player is in or
	// watches: OStim's UIState::HandleControl hands it to the thread
	// API's control callbacks. Main thread.
	void NotifyNGControl(FourStimNG_API::Thread::Controls a_control)
	{
		const auto focused = GetPlayerScene();
		const auto active = focused.Active() ? FindActiveScene(focused.role0) : nullptr;
		if (active) {
			NGThreads()->NotifyControl(a_control, static_cast<std::uint32_t>(active->threadID));
		}
	}
}

namespace Core
{
	// FourStim.TestPluginAPI (FourStimTest.PluginAPI): the C++ API's self-check.
	std::string TestPluginAPI(std::monostate) { return API::SelfTest(); }
}
