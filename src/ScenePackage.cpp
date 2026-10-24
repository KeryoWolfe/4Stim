#include "ScenePackage.h"

#include "compat/Fallbacks.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace ScenePackage
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// Who's in a scene. Read by the game's AI threads (the hook), written
		// on the main thread.
		std::shared_mutex                                 g_lock;
		std::unordered_map<std::uint32_t, Clock::time_point> g_actors;  // when they were added
		std::atomic<bool>                                 g_any{ false };
		std::atomic<RE::TESPackage*>                      g_package{ nullptr };

		// Fallout4.esm's MQ101VaultTecRepDoNothingScene: procedure "Wait",
		// StopMovement, no conditions, no flags (OStim's OStimScenePackage is
		// a "Wait" with StopMovement too).
		constexpr RE::TESFormID VANILLA_PACKAGE = 0x000E7BFD;

		// The game's TESPackage::CreatePackageByType(procedure type) and the
		// set-up InitiateDoNothingPackage runs on the package after it (ID
		// 2211735: per procedure type, its default data; second argument 0
		// there). Found by reading Actor::InitiateDoNothingPackage (ID
		// 2229807) on 1.11.240: CreatePackageByType(0x1B), that, packFlags |=
		// 4 (must complete), PutCreatedPackage.
		constexpr std::int32_t DO_NOTHING_TYPE = 0x1B;
		constexpr std::uint32_t MUST_COMPLETE = 1u << 2;

		using check_t = RE::TESPackage* (*)(RE::TESObjectREFR*);
		check_t g_original = nullptr;

		// Actor::CheckForCurrentAliasPackage (vtable slot 0x5B). As OStim's
		// PackageStart thunk: the game's own runs first (whatever else it
		// does), and for an actor in a scene the scene package is the answer.
		RE::TESPackage* CheckForCurrentAliasPackage(RE::TESObjectREFR* a_this)
		{
			const auto original = g_original(a_this);
			if (!a_this || !g_any.load(std::memory_order_relaxed)) {
				return original;
			}
			const auto package = g_package.load(std::memory_order_acquire);
			if (!package) {
				return original;
			}
			std::shared_lock l(g_lock);
			return g_actors.contains(a_this->GetFormID()) ? package : original;
		}

		RE::TESPackage* MakePackage()
		{
			using create_t = RE::TESPackage* (*)(std::int32_t);
			using init_t = void (*)(RE::TESPackage*, void*);
			static REL::Relocation<create_t> create{ REL::ID{ 2211661 } };
			static REL::Relocation<init_t>   init{ REL::ID{ 2211735 } };
			const auto package = create(DO_NOTHING_TYPE);
			if (!package) {
				return nullptr;
			}
			init(package, nullptr);
			package->data.packFlags |= MUST_COMPLETE;
			return package;
		}
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE::Actor[0] };
		g_original = reinterpret_cast<check_t>(vtbl.write_vfunc(0x5B, CheckForCurrentAliasPackage));
		REX::INFO("Scene package: hook installed (Actor's alias-package check, as OStim's PackageStart)");
	}

	void Prepare()
	{
		if (g_package.load()) {
			return;
		}
		RE::TESPackage* package = nullptr;
		const char*     what = "";
		if (Compat::CanMakePackages()) {
			package = MakePackage();
			what = "made as the game's do-nothing package (procedure type 0x1B, must complete)";
		}
		if (!package) {
			package = RE::TESForm::GetFormByID<RE::TESPackage>(VANILLA_PACKAGE);
			what = "Fallout4.esm's MQ101VaultTecRepDoNothingScene";
		}
		if (!package) {
			REX::WARN("Scene package: none made or found; NPCs in a scene keep their own packages (the scene guard still takes them out of furniture)");
			return;
		}
		g_package.store(package, std::memory_order_release);
		REX::INFO("Scene package: {} ({:08X}, procedure type {:X})", what, package->GetFormID(), static_cast<int>(package->data.packType));
	}

	void Add(std::uint32_t a_id)
	{
		bool added = false;
		{
			std::unique_lock l(g_lock);
			added = !g_actors.contains(a_id);
			g_actors[a_id] = Clock::now();
			g_any.store(true, std::memory_order_relaxed);
		}
		if (added) {
			REX::INFO("Scene package: {:08X} does nothing but the scene until it ends{}", a_id, g_package.load() ? "" : " (no package: off)");
		}
	}

	void Remove(const std::vector<std::uint32_t>& a_ids)
	{
		std::unique_lock l(g_lock);
		for (const auto id : a_ids) {
			if (g_actors.erase(id) > 0) {
				REX::INFO("Scene package: {:08X} back to their own packages", id);
			}
		}
		g_any.store(!g_actors.empty(), std::memory_order_relaxed);
	}

	std::vector<std::uint32_t> Prune(const std::vector<std::uint32_t>& a_inScenes)
	{
		std::vector<std::uint32_t> pruned;
		if (!g_any.load(std::memory_order_relaxed)) {
			return pruned;
		}
		const auto now = Clock::now();
		std::unique_lock l(g_lock);
		std::erase_if(g_actors, [&](const auto& a_entry) {
			// A few seconds' grace: SuppressInteraction adds an NPC just before
			// their scene starts.
			if (now - a_entry.second < std::chrono::seconds(10) || std::ranges::find(a_inScenes, a_entry.first) != a_inScenes.end()) {
				return false;
			}
			REX::INFO("Scene package: {:08X} isn't in a scene any more: back to their own packages", a_entry.first);
			pruned.push_back(a_entry.first);
			return true;
		});
		g_any.store(!g_actors.empty(), std::memory_order_relaxed);
		return pruned;
	}

	void Clear()
	{
		std::unique_lock l(g_lock);
		g_actors.clear();
		g_any.store(false, std::memory_order_relaxed);
	}
}
