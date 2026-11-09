#include "Core/Core.h"

namespace Core::Natives::FourActor
{
	// The actor's thread, a starting one too (OStim's threads exist from Start).
	std::int32_t GetThreadID(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return -1;
		}
		if (const auto id = ThreadIDOf(a_actor->GetFormID()); id >= 0) {
			return id;
		}
		std::scoped_lock lock(g_threadsLock);
		for (const auto& [id, record] : g_threads) {
			if (record.status == ThreadRecord::Status::kStarting && std::ranges::find(record.actors, a_actor->GetFormID()) != record.actors.end()) {
				return id;
			}
		}
		return -1;
	}
	bool HasMetadata(std::monostate, RE::Actor* a_actor, std::string a_meta)
	{
		std::scoped_lock lock(g_actorMetaLock);
		const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
		return it != g_actorMeta.end() && std::ranges::find(it->second.tags, LowerCopy(a_meta)) != it->second.tags.end();
	}
	void AddMetadata(std::monostate, RE::Actor* a_actor, std::string a_meta)
	{
		if (!a_actor || !FindActiveScene(a_actor->GetFormID())) {
			return;
		}
		std::scoped_lock lock(g_actorMetaLock);
		auto&      tags = g_actorMeta[a_actor->GetFormID()].tags;
		const auto meta = LowerCopy(a_meta);
		if (std::ranges::find(tags, meta) == tags.end()) {
			tags.push_back(meta);
		}
	}
	std::vector<std::string> GetMetadata(std::monostate, RE::Actor* a_actor)
	{
		std::scoped_lock lock(g_actorMetaLock);
		const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
		return it != g_actorMeta.end() ? it->second.tags : std::vector<std::string>{};
	}
	bool HasMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key)
	{
		std::scoped_lock lock(g_actorMetaLock);
		const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
		return it != g_actorMeta.end() && it->second.floats.contains(LowerCopy(a_key));
	}
	float GetMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key)
	{
		std::scoped_lock lock(g_actorMetaLock);
		const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
		if (it == g_actorMeta.end()) {
			return 0.0F;
		}
		const auto f = it->second.floats.find(LowerCopy(a_key));
		return f != it->second.floats.end() ? f->second : 0.0F;
	}
	void SetMetaFloat(std::monostate, RE::Actor* a_actor, std::string a_key, float a_value)
	{
		if (!a_actor || !FindActiveScene(a_actor->GetFormID())) {
			return;
		}
		std::scoped_lock lock(g_actorMetaLock);
		g_actorMeta[a_actor->GetFormID()].floats[LowerCopy(a_key)] = a_value;
	}
	bool HasMetaString(std::monostate, RE::Actor* a_actor, std::string a_key)
	{
		std::scoped_lock lock(g_actorMetaLock);
		const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
		return it != g_actorMeta.end() && it->second.strings.contains(LowerCopy(a_key));
	}
	std::string GetMetaString(std::monostate, RE::Actor* a_actor, std::string a_key)
	{
		std::scoped_lock lock(g_actorMetaLock);
		const auto it = a_actor ? g_actorMeta.find(a_actor->GetFormID()) : g_actorMeta.end();
		if (it == g_actorMeta.end()) {
			return {};
		}
		const auto f = it->second.strings.find(LowerCopy(a_key));
		return f != it->second.strings.end() ? f->second : std::string{};
	}
	void SetMetaString(std::monostate, RE::Actor* a_actor, std::string a_key, std::string a_value)
	{
		if (!a_actor || !FindActiveScene(a_actor->GetFormID())) {
			return;
		}
		std::scoped_lock lock(g_actorMetaLock);
		g_actorMeta[a_actor->GetFormID()].strings[LowerCopy(a_key)] = std::move(a_value);
	}
	// OActor.PlayExpression: the expression's duration in seconds (-1 if
	// the event has none), as OStim's (it returned a bool before 0.5.0).
	float PlayExpression(std::monostate, RE::Actor* a_actor, std::string a_expression)
	{
		return a_actor ? Expressions::PlayEvent(a_actor->GetFormID(), LowerCopy(a_expression), true) : -1.0F;
	}
	void SetExpressionsEnabled(std::monostate, RE::Actor* a_actor, bool a_enabled, bool a_allowOverride)
	{
		if (a_actor) {
			Expressions::SetEnabled(a_actor->GetFormID(), a_enabled, a_allowOverride);
		}
	}
	bool HasExpressionOverride(std::monostate, RE::Actor* a_actor)
	{
		return a_actor && Expressions::HasOverride(a_actor->GetFormID());
	}
	void UndressPartial(std::monostate, RE::Actor* a_actor, std::int32_t a_mask)
	{
		if (a_actor && FindActiveScene(a_actor->GetFormID())) {
			Undress::StripMask(a_actor->GetFormID(), static_cast<std::uint32_t>(a_mask));
		}
	}
	void RedressPartial(std::monostate, RE::Actor* a_actor, std::int32_t a_mask)
	{
		if (a_actor && FindActiveScene(a_actor->GetFormID())) {
			Undress::RedressMask(a_actor->GetFormID(), static_cast<std::uint32_t>(a_mask));
		}
	}
	void ClearExpression(std::monostate, RE::Actor* a_actor)
	{
		if (a_actor) {
			Expressions::ClearEvent(a_actor->GetFormID());
		}
	}
}

namespace Core
{
	// FourFurniture (OFurniture): furniture near a_center (OStim's
	// FindFurniture: the pieces a scene for a_actorCount could go on, nearest
	// first; FindFurnitureOfType: the nearest of a type). a_sameFloor: how far
	// up or down (0 = 4Stim's fFurnitureHeight). Main thread: Papyrus waits
	// for the answer.
	std::vector<Furniture::Found> FurnitureAround(RE::TESObjectREFR* a_center, float a_radius, float a_sameFloor)
	{
		std::vector<Furniture::Found> found;
		if (!a_center) {
			return found;
		}
		const auto     center = a_center->data.location;
		const float    radius = a_radius > 0.0F ? a_radius : g_settings.furnitureRadius;
		const float    height = a_sameFloor > 0.0F ? a_sameFloor : g_settings.furnitureHeight;
		auto search = [&]() {
			auto list = Furniture::FindNear(center, radius, height, false, a_center);
			std::erase_if(list, [](const Furniture::Found& f) {
				return f.ref && (FurnitureBlocked(f.ref) || std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a) { return a.furnitureRef == f.ref; }));
			});
			return list;
		};
		if (std::this_thread::get_id() == g_mainThread) {
			return search();
		}
		// Shared, so a late answer after the wait gave up has somewhere to go.
		struct Answer
		{
			std::mutex                    lock;
			std::condition_variable       cv;
			bool                          ready = false;
			std::vector<Furniture::Found> found;
		};
		auto answer = std::make_shared<Answer>();
		F4SE::GetTaskInterface()->AddTask([answer, center, radius, height, centerID = a_center->GetFormID()]() {
			const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(centerID);
			auto       list = Furniture::FindNear(center, radius, height, false, ref);
			std::erase_if(list, [](const Furniture::Found& f) {
				return f.ref && (FurnitureBlocked(f.ref) || std::ranges::any_of(g_activeScenes, [&](const ActiveScene& a) { return a.furnitureRef == f.ref; }));
			});
			{
				std::scoped_lock lock(answer->lock);
				answer->found = std::move(list);
				answer->ready = true;
			}
			answer->cv.notify_one();
		});
		std::unique_lock lock(answer->lock);
		answer->cv.wait_for(lock, std::chrono::seconds(5), [&]() { return answer->ready; });
		found = std::move(answer->found);
		return found;
	}
}
