#include "Physics.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Bridge.h"
#include "SceneRegistry.h"

// CommonLibF4 declares this but doesn't define it; needed to derive a
// callback (the game never calls it on its own functors through us).
RE::BSScript::IStackCallbackFunctor::~IStackCallbackFunctor() = default;

namespace Physics
{
	namespace
	{
		constexpr auto SWAP_FOLDER = "Data/F4SE/Plugins/4Stim/Physics";

		struct Swap
		{
			std::string           id;
			SceneRegistry::Sex    sex = SceneRegistry::Sex::kAny;  // whose body it is
			std::string           from;          // the physics file the body normally uses
			std::string           to;            // during a scene
			std::string           toReceiving;   // during a scene, in a role other than the first ("" = to)
			std::vector<std::string> sceneTags;  // only scenes with one of these tags ("" = every scene)
		};

		struct Applied
		{
			std::string from;
			std::string to;
		};

		std::mutex                                g_lock;
		std::vector<Swap>                         g_swaps;
		bool                                      g_loaded = false;
		std::unordered_map<std::uint32_t, Applied> g_applied;  // actor -> what's swapped in

		std::string Lower(std::string_view a_str)
		{
			std::string out(a_str);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_swaps.clear();
			std::error_code                    ec;
			std::vector<std::filesystem::path> files;
			for (const auto& e : std::filesystem::directory_iterator(SWAP_FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					files.push_back(e.path());
				}
			}
			std::ranges::sort(files);
			for (const auto& path : files) {
				const auto     file = path.filename().string();
				nlohmann::json root;
				try {
					std::ifstream in(path);
					root = nlohmann::json::parse(in, nullptr, true, true);
				} catch (const std::exception& ex) {
					REX::WARN("Physics: {}: not valid JSON ({})", file, ex.what());
					continue;
				}
				const auto swaps = root.find("swaps");
				if (swaps == root.end() || !swaps->is_array()) {
					REX::WARN("Physics: {}: no \"swaps\" list", file);
					continue;
				}
				for (const auto& entry : *swaps) {
					Swap swap;
					swap.id = entry.value("id", file);
					const auto sex = Lower(entry.value("sex", std::string{ "any" }));
					swap.sex = sex == "male" ? SceneRegistry::Sex::kMale : sex == "female" ? SceneRegistry::Sex::kFemale : SceneRegistry::Sex::kAny;
					swap.from = entry.value("from", std::string{});
					swap.to = entry.value("to", std::string{});
					swap.toReceiving = entry.value("toReceiving", std::string{});
					if (const auto tags = entry.find("sceneTags"); tags != entry.end() && tags->is_array()) {
						for (const auto& tag : *tags) {
							if (tag.is_string()) {
								swap.sceneTags.push_back(Lower(tag.get<std::string>()));
							}
						}
					}
					if (swap.from.empty() || swap.to.empty()) {
						REX::WARN("Physics: {}: swap \"{}\" needs \"from\" and \"to\", skipped", file, swap.id);
						continue;
					}
					g_swaps.push_back(std::move(swap));
				}
			}
			REX::INFO("Physics: {} swap(s) from {} file(s)", g_swaps.size(), files.size());
		}

		// Logs what SwapPhysicsFile returned: false means FSMP found no active
		// physics system on the actor using that file.
		class SwapResult : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			SwapResult(std::uint32_t a_actor, std::string a_from, std::string a_to) :
				_actor(a_actor), _from(std::move(a_from)), _to(std::move(a_to)) {}

			void CallQueued() override {}
			void CallCanceled() override { REX::WARN("Physics: {:08X} swap to \"{}\" canceled", _actor, _to); }
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				const bool ok = a_result.is<bool>() && RE::BSScript::get<bool>(a_result);
				if (ok) {
					REX::INFO("Physics: {:08X} now uses \"{}\"", _actor, _to);
				} else {
					REX::WARN("Physics: {:08X} not swapped: FSMP found no active physics using \"{}\" on it (not loaded, beyond maxActiveActors in FSMP's configs.xml, or a different file)", _actor, _from);
				}
			}

		private:
			std::uint32_t _actor;
			std::string   _from;
			std::string   _to;
		};

		bool CallSwap(RE::Actor* a_actor, const std::string& a_from, const std::string& a_to)
		{
			const auto vm = FourStim::GetVM();
			if (!vm || !a_actor) {
				return false;
			}
			// FSMP: bool SwapPhysicsFile(Actor, String oldFile, String newFile, bool persist, bool verbose) global native
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new SwapResult(a_actor->GetFormID(), a_from, a_to) };
			const bool ok = vm->DispatchStaticCall("DynamicHDT"sv, "SwapPhysicsFile"sv, callback, a_actor, a_from, a_to, true, true);
			REX::INFO("Physics: {:08X} \"{}\" -> \"{}\" (dispatch={})", a_actor->GetFormID(), a_from, a_to, ok);
			if (!ok) {
				REX::WARN("Physics: couldn't call DynamicHDT.SwapPhysicsFile: is Fallout 4 FSMP installed?");
			}
			return ok;
		}
	}

	void SceneStarted(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		if (g_swaps.empty()) {
			return;
		}
		const auto scene = SceneRegistry::Find(a_sceneID);
		for (std::size_t role = 0; role < a_actors.size(); ++role) {
			const auto id = a_actors[role];
			if (g_applied.contains(id)) {
				continue;  // already swapped (a new scene right after another)
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				continue;
			}
			const auto sex = SceneRegistry::SexOf(actor);
			for (const auto& swap : g_swaps) {
				if (swap.sex != SceneRegistry::Sex::kAny && swap.sex != sex) {
					continue;
				}
				if (!swap.sceneTags.empty()) {
					const bool tagged = scene && std::ranges::any_of(scene->tags, [&](const std::string& tag) {
						return std::ranges::find(swap.sceneTags, Lower(tag)) != swap.sceneTags.end();
					});
					if (!tagged) {
						continue;
					}
				}
				const auto& to = role > 0 && !swap.toReceiving.empty() ? swap.toReceiving : swap.to;
				if (CallSwap(actor, swap.from, to)) {
					g_applied[id] = { swap.from, to };
				}
				break;  // one swap per actor
			}
		}
	}

	void SceneEnded(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_actors) {
			const auto it = g_applied.find(id);
			if (it == g_applied.end()) {
				continue;
			}
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id)) {
				CallSwap(actor, it->second.to, it->second.from);
			}
			g_applied.erase(it);
		}
	}
}
