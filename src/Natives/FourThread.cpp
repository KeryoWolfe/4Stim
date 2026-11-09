#include "Core/Core.h"

namespace Core::Natives::FourThread
{
	// FourThread (OThread)
	std::int32_t QuickStart(std::monostate, std::vector<RE::Actor*> a_actors, std::string a_scene, RE::TESObjectREFR* a_furniture)
	{
		const auto id = BuilderCreate(std::move(a_actors));
		if (id < 0) {
			return -1;
		}
		if (!a_scene.empty()) {
			Natives::FourThreadBuilder::SetStartingAnimation({}, id, a_scene);
		}
		if (a_furniture) {
			Natives::FourThreadBuilder::SetFurniture({}, id, a_furniture);
		}
		return Natives::FourThreadBuilder::Start({}, id);
	}

	bool IsLive(const ThreadRecord& a_record) { return a_record.status != ThreadRecord::Status::kEnded; }

	// As OStim's: a thread exists as soon as Start returned it (its actors
	// are still being placed), until its scene ends.
	bool IsRunning(std::monostate, std::int32_t a_id)
	{
		bool live = false;
		WithRecord(a_id, [&](ThreadRecord& r) { live = IsLive(r); });
		return live;
	}
	void Stop(std::monostate, std::int32_t a_id)
	{
		ThreadDo(
			a_id, [](ActiveScene& a) { EndActiveScene(a, "asked by another mod (FourThread.Stop)"); },
			[](ThreadRecord& r) { r.stopWhenStarted = true; });
	}
	std::int32_t GetThreadCount(std::monostate)
	{
		std::scoped_lock lock(g_threadsLock);
		return static_cast<std::int32_t>(std::ranges::count_if(g_threads, [](const auto& a_entry) { return IsLive(a_entry.second); }));
	}
	std::vector<std::int32_t> GetAllThreadIDs(std::monostate)
	{
		std::vector<std::int32_t> ids;
		std::scoped_lock lock(g_threadsLock);
		for (const auto& [id, record] : g_threads) {
			if (IsLive(record)) {
				ids.push_back(id);
			}
		}
		std::ranges::sort(ids);
		return ids;
	}
	std::string GetScene(std::monostate, std::int32_t a_id)
	{
		const auto a = FindThread(a_id);
		return a ? a->sceneID : std::string{};
	}
	// NavigateTo (OStim: through transitions where it can): a transition
	// running finishes first; WarpTo goes there at once.
	void ThreadGoTo(std::int32_t a_id, std::string a_scene, bool a_warp)
	{
		ThreadDo(
			a_id,
			[a_scene, a_warp](ActiveScene& a) {
				const auto to = SceneRegistry::Find(a_scene);
				if (!to || to->actors.size() != a.actors.size()) {
					REX::WARN("FourThread: thread {} can't go to \"{}\"", a.threadID, a_scene);
					return;
				}
				const auto from = SceneRegistry::Find(a.sceneID);
				if (!a_warp && from && from->IsTransition()) {
					a.queuedScene = to->id;
					return;
				}
				if (InterruptSequence(a, "another mod moved the scene")) {
					return;
				}
				// As OStim's navigateTo: along the navigation graph when the
				// scene is within navigationDistanceMax (auto mode's way), else
				// straight there.
				if (!a_warp && from) {
					const auto reachable = Reachable(a, from, g_autoConfig.maxSteps);
					const auto it = std::ranges::find_if(reachable, [&](const auto& a_entry) { return a_entry.first->id == to->id; });
					if (it != reachable.end() && it->second.size() > 1) {
						AutoGoTo(a, to, it->second);
						return;
					}
				}
				if (PlayOnActiveScene(a, to->id, -1)) {
					ArmAutoplay(a);
				}
			},
			[a_scene](ThreadRecord& r) { r.sceneWhenStarted = a_scene; });
	}
	void NavigateTo(std::monostate, std::int32_t a_id, std::string a_scene) { ThreadGoTo(a_id, std::move(a_scene), false); }
	void WarpTo(std::monostate, std::int32_t a_id, std::string a_scene, bool) { ThreadGoTo(a_id, std::move(a_scene), true); }
	std::int32_t GetSpeed(std::monostate, std::int32_t a_id)
	{
		const auto a = FindThread(a_id);
		return a ? a->speed : -1;
	}
	void SetSpeed(std::monostate, std::int32_t a_id, std::int32_t a_speed)
	{
		ThreadDo(
			a_id,
			[a_speed](ActiveScene& a) {
				const auto scene = SceneRegistry::Find(a.sceneID);
				if (!scene || scene->speeds.empty()) {
					return;
				}
				const int speed = std::clamp(a_speed, 0, static_cast<int>(scene->speeds.size()) - 1);
				if (speed != a.speed) {
					PlayOnActiveScene(a, a.sceneID, speed);
				}
			},
			[a_speed](ThreadRecord& r) { r.speedWhenStarted = a_speed; });
	}
	// The actors in role order; while starting, in the order they were given.
	std::vector<std::uint32_t> ThreadActors(std::int32_t a_id)
	{
		if (const auto a = FindThread(a_id)) {
			return a->actors;
		}
		std::vector<std::uint32_t> out;
		WithRecord(a_id, [&](ThreadRecord& r) {
			if (IsLive(r)) {
				out = r.roleActors;
			}
		});
		return out;
	}
	std::vector<RE::Actor*> GetActors(std::monostate, std::int32_t a_id)
	{
		std::vector<RE::Actor*> out;
		for (const auto id : ThreadActors(a_id)) {
			out.push_back(RE::TESForm::GetFormByID<RE::Actor>(id));
		}
		return out;
	}
	RE::Actor* GetActor(std::monostate, std::int32_t a_id, std::int32_t a_index)
	{
		const auto actors = ThreadActors(a_id);
		return a_index >= 0 && static_cast<std::size_t>(a_index) < actors.size() ? RE::TESForm::GetFormByID<RE::Actor>(actors[a_index]) : nullptr;
	}
	std::int32_t GetActorPosition(std::monostate, std::int32_t a_id, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return -1;
		}
		const auto actors = ThreadActors(a_id);
		const auto it = std::ranges::find(actors, a_actor->GetFormID());
		return it != actors.end() ? static_cast<std::int32_t>(it - actors.begin()) : -1;
	}
	// OThread's climax stall, as OStim's Thread::stallClimax: the thread's
	// own flag, apart from each actor's (OActor.StallClimax), so permitting
	// one doesn't lift the other. PermitClimax(id, true) lifts the actors'
	// too, as OStim's permitActors.
	void StallClimax(std::monostate, std::int32_t a_id)
	{
		ThreadDo(
			a_id,
			[](ActiveScene& a) {
				a.stallClimax = true;
				for (const auto id : a.actors) {
					Excitement::SetThreadStalled(id, true);
				}
			},
			[](ThreadRecord& r) { r.stallWhenStarted = true; });
	}
	void PermitClimax(std::monostate, std::int32_t a_id, bool a_permitActors)
	{
		ThreadDo(
			a_id,
			[a_permitActors](ActiveScene& a) {
				a.stallClimax = false;
				for (const auto id : a.actors) {
					Excitement::SetThreadStalled(id, false);
					if (a_permitActors) {
						Excitement::SetStalled(id, false);
					}
				}
			},
			[a_permitActors](ThreadRecord& r) { r.stallWhenStarted = false; (void)a_permitActors; });
	}
	bool IsClimaxStalled(std::monostate, std::int32_t a_id)
	{
		bool stalled = false;
		WithRecord(a_id, [&](ThreadRecord& r) { stalled = r.status == ThreadRecord::Status::kStarting && r.stallWhenStarted.value_or(false); });
		if (stalled) {
			return true;
		}
		const auto a = FindThread(a_id);
		return a && a->stallClimax;  // OStim's: the thread's own flag only
	}
	RE::TESObjectREFR* GetFurniture(std::monostate, std::int32_t a_id)
	{
		const auto a = FindThread(a_id);
		return a && a->furnitureRef ? RE::TESForm::GetFormByID<RE::TESObjectREFR>(a->furnitureRef) : nullptr;
	}
	std::string GetFurnitureType(std::monostate, std::int32_t a_id)
	{
		const auto a = FindThread(a_id);
		return a ? a->furnitureType : std::string{};
	}
	bool IsInAutoMode(std::monostate, std::int32_t a_id)
	{
		bool queued = false;
		WithRecord(a_id, [&](ThreadRecord& r) { queued = r.status == ThreadRecord::Status::kStarting && r.autoWhenStarted.value_or(false); });
		const auto a = FindThread(a_id);
		return queued || (a && a->autoMode.on);
	}
	void StartAutoMode(std::monostate, std::int32_t a_id)
	{
		ThreadDo(
			a_id,
			[](ActiveScene& a) {
				if (!a.autoMode.on) {
					Core::StartAutoMode(a);
				}
			},
			[](ThreadRecord& r) { r.autoWhenStarted = true; });
	}
	void StopAutoMode(std::monostate, std::int32_t a_id)
	{
		ThreadDo(
			a_id,
			[](ActiveScene& a) {
				if (a.autoMode.on) {
					Core::StopAutoMode(a);
				}
			},
			[](ThreadRecord& r) { r.autoWhenStarted = false; });
	}
	// Metadata: read and written at once (OStim's are), from the record, so
	// a starting thread has it too, and an ended one until its end event.
	bool HasMetadata(std::monostate, std::int32_t a_id, std::string a_meta)
	{
		bool has = false;
		WithRecord(a_id, [&, meta = LowerCopy(a_meta)](ThreadRecord& r) { has = std::ranges::find(r.metadata, meta) != r.metadata.end(); });
		return has;
	}
	void AddMetadata(std::monostate, std::int32_t a_id, std::string a_meta)
	{
		WithRecord(a_id, [meta = LowerCopy(a_meta)](ThreadRecord& r) {
			if (std::ranges::find(r.metadata, meta) == r.metadata.end()) {
				r.metadata.push_back(meta);
			}
		});
	}
	std::vector<std::string> GetMetadata(std::monostate, std::int32_t a_id)
	{
		std::vector<std::string> out;
		WithRecord(a_id, [&](ThreadRecord& r) { out = r.metadata; });
		return out;
	}
	bool HasMetaFloat(std::monostate, std::int32_t a_id, std::string a_key)
	{
		bool has = false;
		WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) { has = r.metaFloat.contains(key); });
		return has;
	}
	float GetMetaFloat(std::monostate, std::int32_t a_id, std::string a_key)
	{
		float value = 0.0F;
		WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) {
			if (const auto it = r.metaFloat.find(key); it != r.metaFloat.end()) {
				value = it->second;
			}
		});
		return value;
	}
	void SetMetaFloat(std::monostate, std::int32_t a_id, std::string a_key, float a_value)
	{
		WithRecord(a_id, [key = LowerCopy(a_key), a_value](ThreadRecord& r) { r.metaFloat[key] = a_value; });
	}
	bool HasMetaString(std::monostate, std::int32_t a_id, std::string a_key)
	{
		bool has = false;
		WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) { has = r.metaString.contains(key); });
		return has;
	}
	std::string GetMetaString(std::monostate, std::int32_t a_id, std::string a_key)
	{
		std::string value;
		WithRecord(a_id, [&, key = LowerCopy(a_key)](ThreadRecord& r) {
			if (const auto it = r.metaString.find(key); it != r.metaString.end()) {
				value = it->second;
			}
		});
		return value;
	}
	void SetMetaString(std::monostate, std::int32_t a_id, std::string a_key, std::string a_value)
	{
		WithRecord(a_id, [key = LowerCopy(a_key), value = std::move(a_value)](ThreadRecord& r) { r.metaString[key] = value; });
	}
	bool PlaySequence(std::monostate, std::int32_t a_id, std::string a_sequence, bool, bool)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequence);
		const auto a = FindThread(a_id);
		if (!sequence || !a) {
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([actor = a->actors.front(), sequence]() { StartSequenceOnScene(actor, sequence); });
		return true;
	}
}
