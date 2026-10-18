#include "Physics.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Actions.h"
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
			std::string           toReceiving;   // during a scene, for an actor on the receiving end ("" = to)
			std::vector<std::string> sceneTags;  // only scenes with one of these tags ("" = every scene)
		};

		struct Applied
		{
			std::string from;
			std::string to;           // swapped in now
			std::string giving;       // the swap's "to"
			std::string receiving;    // its "toReceiving" ("" = giving)
			bool        pendingBack = false;  // its scene ended: swapping back AFTER_SCENE_DELAY later
		};

		// Whether role a_role is on the receiving end in a_scene: the target
		// of someone else's action that needs their anus or vagina (anal or
		// vaginal sex, pegging, fingering, rimming...). A scene with no such
		// action falls back to OStim's role order, where the first role
		// gives and the others receive.
		bool Receiving(const SceneRegistry::Scene* a_scene, std::size_t a_role)
		{
			if (!a_scene) {
				return a_role > 0;
			}
			bool penetration = false;
			for (const auto& action : a_scene->actions) {
				const auto& needs = action.type->target.requirements;
				const bool opening = std::ranges::find(needs, "anus") != needs.end() || std::ranges::find(needs, "vagina") != needs.end();
				if (!opening || action.actor == action.target) {
					continue;
				}
				penetration = true;
				if (action.target == a_role) {
					return true;
				}
			}
			return penetration ? false : a_role > 0;
		}

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
			SwapResult(std::uint32_t a_actor, std::string a_from, std::string a_to, bool a_afterScene = false) :
				_actor(a_actor), _from(std::move(a_from)), _to(std::move(a_to)), _afterScene(a_afterScene) {}

			void CallQueued() override {}
			void CallCanceled() override { REX::WARN("Physics: {:08X} swap to \"{}\" canceled", _actor, _to); }
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				const int code = a_result.is<std::int32_t>() ? RE::BSScript::get<std::int32_t>(a_result) : -3;
				if (_afterScene) {
					// The swap back is done (or failed): forget it, unless a new
					// scene took the actor first (-4: still swapped, theirs now).
					std::scoped_lock lock(g_lock);
					if (const auto it = g_applied.find(_actor); it != g_applied.end() && it->second.pendingBack && code != -4) {
						g_applied.erase(it);
					}
				}
				switch (code) {
				case -4:
					REX::INFO("Physics: {:08X} kept \"{}\": in a new scene before its swap back", _actor, _from);
					break;
				case 1:
				case 2:
				case 3:
					REX::INFO("Physics: {:08X} now uses \"{}\" (spelling {})", _actor, _to, code);
					break;
				case 0:
					REX::WARN("Physics: {:08X} not swapped: FSMP has no active physics on it (not loaded, or beyond maxActiveActors in FSMP's configs.xml)", _actor);
					break;
				case -1:
					REX::WARN("Physics: {:08X} not swapped: it has physics, but none using \"{}\" (wrong file for this body?)", _actor, _from);
					break;
				case -2:
					REX::WARN("Physics: {:08X} not swapped: actor not found", _actor);
					break;
				default:
					REX::WARN("Physics: {:08X} not swapped: no answer from FourStimPhysics.Swap (old FourStimPhysics.pex?)", _actor);
					break;
				}
			}

		private:
			std::uint32_t _actor;
			std::string   _from;
			std::string   _to;
			bool          _afterScene;
		};

		// How long after a scene ends its actors' physics are put back: by
		// then they're out of the scene's pose and in their own idle.
		constexpr float AFTER_SCENE_DELAY = 2.0F;

		// a_afterScene: the swap back at a scene's end, AFTER_SCENE_DELAY
		// later and followed by a full physics rebuild (FourStimPhysics.
		// SwapAfterScene). Swapping back while the body was still in the
		// scene's last pose (and the penis bend just undone) could rebuild the
		// floppy physics around that pose, leaving the penis stretched and
		// stuck at an angle afterwards.
		bool CallSwap(RE::Actor* a_actor, const std::string& a_from, const std::string& a_to, bool a_afterScene = false)
		{
			const auto vm = FourStim::GetVM();
			if (!vm || !a_actor) {
				return false;
			}
			// FourStimPhysics.Swap(int, String, String) wraps FSMP's
			// DynamicHDT.SwapPhysicsFile: an Actor passed straight from here
			// arrived as a "type mismatch for argument 1".
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new SwapResult(a_actor->GetFormID(), a_from, a_to, a_afterScene) };
			// The same paths spelled three ways: as given, with "Meshes\" and
			// without the "meshes\" prefix.
			auto bare = [](const std::string& a_path) {
				return Lower(a_path).starts_with("meshes\\") ? a_path.substr(7) : a_path;
			};
			const auto fromBare = bare(a_from), toBare = bare(a_to);
			const bool ok = a_afterScene ?
			                    vm->DispatchStaticCall("FourStimPhysics"sv, "SwapAfterScene"sv, callback, static_cast<std::int32_t>(a_actor->GetFormID()),
									a_from, a_to, "Meshes\\" + fromBare, "Meshes\\" + toBare, fromBare, toBare, AFTER_SCENE_DELAY) :
			                    vm->DispatchStaticCall("FourStimPhysics"sv, "Swap"sv, callback, static_cast<std::int32_t>(a_actor->GetFormID()),
									a_from, a_to, "Meshes\\" + fromBare, "Meshes\\" + toBare, fromBare, toBare);
			REX::INFO("Physics: {:08X} \"{}\" -> \"{}\" (dispatch={})", a_actor->GetFormID(), a_from, a_to, ok);
			if (!ok) {
				REX::WARN("Physics: couldn't call FourStimPhysics.Swap: is FourStimPhysics.pex installed (and FO4 Faster HDT-SMP)?");
			}
			return ok;
		}
		// Each actor whose swap has a "toReceiving": that or "to", as they give
		// or receive in a_sceneID. g_lock held.
		void SwitchForScene(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
		{
			const auto scene = SceneRegistry::Find(a_sceneID);
			for (std::size_t role = 0; role < a_actors.size(); ++role) {
				const auto it = g_applied.find(a_actors[role]);
				if (it == g_applied.end() || it->second.receiving.empty() || it->second.pendingBack) {
					continue;
				}
				auto&       applied = it->second;
				const auto& want = Receiving(scene.get(), role) ? applied.receiving : applied.giving;
				if (want == applied.to) {
					continue;
				}
				if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_actors[role]); actor && CallSwap(actor, applied.to, want)) {
					applied.to = want;
				}
			}
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
			if (const auto it = g_applied.find(id); it != g_applied.end()) {
				// Still swapped: a new scene before the last one's swap back
				// (which then leaves it alone); SceneChanged picks giving or
				// receiving for this scene below.
				it->second.pendingBack = false;
				continue;
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
				const auto& to = Receiving(scene.get(), role) && !swap.toReceiving.empty() ? swap.toReceiving : swap.to;
				if (CallSwap(actor, swap.from, to)) {
					g_applied[id] = { swap.from, to, swap.to, swap.toReceiving };
				}
				break;  // one swap per actor
			}
		}
		SwitchForScene(a_actors, a_sceneID);  // those still swapped from a scene just before
	}

	void SceneChanged(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		std::scoped_lock lock(g_lock);
		SwitchForScene(a_actors, a_sceneID);
	}

	void SceneEnded(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		std::vector<std::uint32_t> unswapped;
		for (const auto id : a_actors) {
			const auto it = g_applied.find(id);
			if (it == g_applied.end()) {
				unswapped.push_back(id);
				continue;
			}
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (actor && CallSwap(actor, it->second.to, it->second.from, true)) {
				it->second.pendingBack = true;  // forgotten when the swap back answers
			} else {
				g_applied.erase(it);
			}
		}
		// Everyone else's physics rebuilt too, once they're out of the pose
		// (a penis bend undone, breasts tangled in a pose).
		if (const auto vm = FourStim::GetVM()) {
			for (const auto id : unswapped) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				vm->DispatchStaticCall("FourStimPhysics"sv, "ResetAfterScene"sv, callback, static_cast<std::int32_t>(id), AFTER_SCENE_DELAY);
			}
		}
	}
}
