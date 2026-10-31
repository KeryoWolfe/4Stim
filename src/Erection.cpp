#include "Erection.h"
#include "compat/ScrapFunction.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "ActorProperties.h"
#include "Bridge.h"
#include "PenisBend.h"
#include "Physics.h"
#include "SceneRegistry.h"
#include "Undress.h"

#include "RE/B/BSResourceNiBinaryStream.h"
#include "RE/B/BSScript_ObjectTypeInfo.h"
#include "RE/N/NiStringExtraData.h"

namespace Erection
{
	namespace
	{
		constexpr auto FOLDER = "Data/F4SE/Plugins/4Stim/Erection";

		struct Bodysuit
		{
			std::string        id;
			std::string        mod;
			std::uint32_t      local = 0;
			SceneRegistry::Sex sex = SceneRegistry::Sex::kMale;
			std::uint32_t      form = 0;  // resolved; 0: its plugin isn't loaded
		};

		struct Files
		{
			std::string              keywordMod = "Fallout4.esm";
			std::uint32_t            keywordLocal = 0x035E9B;
			std::string              detect = "Erection";  // a .tri with this morph is a morph body
			std::string              erection = "Erection";
			std::string              up = "Erection Up";
			std::string              down = "Erection Down";
			float                    value = 1.0F;
			std::vector<std::string> triFiles;  // .tri files to try when the meshes name none
			std::vector<Bodysuit>    bodysuits;
		};

		enum class Method
		{
			kUnknown,   // not worked out yet (still dressed)
			kMorph,
			kPhysics,
			kBodysuit,
			kNone,      // the penis bend only
		};

		constexpr const char* MethodName(Method a_method)
		{
			switch (a_method) {
			case Method::kMorph:
				return "LooksMenu morphs";
			case Method::kPhysics:
				return "rigid physics swap (penis bend only)";
			case Method::kBodysuit:
				return "the erect bodysuit";
			case Method::kNone:
				return "nothing (penis bend only)";
			default:
				return "not known yet";
			}
		}

		struct State
		{
			Method        method = Method::kUnknown;
			bool          flaccid = false;  // OStim's -10
			int           steps = 0;        // the bend, for the Up / Down morphs
			// Morphs: what's set now (-1: nothing set yet).
			float         erection = -1.0F, up = -1.0F, down = -1.0F;
			bool          hasNode = true;   // the skeleton has the penis bend's node
			// Bodysuit.
			std::uint32_t suit = 0;
			bool          suitTheirs = false;  // they had one before the scene: not taken away
			bool          suitBefore = false;  // they wore it before the scene: left alone
			bool          suitOn = false;      // 4Stim put it on
			std::chrono::steady_clock::time_point next{};
		};

		std::mutex                               g_lock;
		Config                                   g_config;
		Files                                    g_files;
		bool                                     g_loaded = false;
		bool                                     g_resolved = false;
		std::uint32_t                            g_keyword = 0;
		std::unordered_map<std::uint32_t, State> g_states;
		std::unordered_map<std::string, bool>    g_triCache;  // lowercase .tri path -> has the detect morph
		std::optional<bool>                      g_looksMenu;

		std::string Lower(std::string_view a_str)
		{
			std::string out(a_str);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::uint32_t Hex(const std::string& a_text)
		{
			try {
				return static_cast<std::uint32_t>(std::stoul(a_text, nullptr, 16));
			} catch (...) {
				return 0;
			}
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_resolved = false;
			g_files = {};
			std::error_code                    ec;
			std::vector<std::filesystem::path> paths;
			for (const auto& e : std::filesystem::directory_iterator(FOLDER, ec)) {
				if (e.is_regular_file() && Lower(e.path().extension().string()) == ".json") {
					paths.push_back(e.path());
				}
			}
			std::ranges::sort(paths);
			for (const auto& path : paths) {
				const auto     file = path.filename().string();
				nlohmann::json root;
				try {
					std::ifstream in(path);
					root = nlohmann::json::parse(in, nullptr, true, true);
				} catch (const std::exception& ex) {
					REX::WARN("Erection: {}: not valid JSON ({})", file, ex.what());
					continue;
				}
				// The last file that says them wins for the single values;
				// bodysuits and .tri files add up.
				if (const auto kw = root.find("keyword"); kw != root.end() && kw->is_object()) {
					g_files.keywordMod = kw->value("mod", g_files.keywordMod);
					g_files.keywordLocal = Hex(kw->value("formid", std::string{ "0" }));
				}
				if (const auto m = root.find("morphs"); m != root.end() && m->is_object()) {
					g_files.detect = m->value("detect", g_files.detect);
					g_files.erection = m->value("erection", g_files.erection);
					g_files.up = m->value("up", g_files.up);
					g_files.down = m->value("down", g_files.down);
					g_files.value = std::clamp(m->value("value", g_files.value), 0.0F, 2.0F);
					if (const auto tris = m->find("triFiles"); tris != m->end() && tris->is_array()) {
						for (const auto& tri : *tris) {
							if (tri.is_string()) {
								g_files.triFiles.push_back(tri.get<std::string>());
							}
						}
					}
				}
				if (const auto suits = root.find("bodysuits"); suits != root.end() && suits->is_array()) {
					for (const auto& entry : *suits) {
						Bodysuit suit;
						suit.id = entry.value("id", file);
						suit.mod = entry.value("mod", std::string{});
						suit.local = Hex(entry.value("formid", std::string{ "0" }));
						const auto sex = Lower(entry.value("sex", std::string{ "male" }));
						suit.sex = sex == "female" ? SceneRegistry::Sex::kFemale : sex == "any" ? SceneRegistry::Sex::kAny : SceneRegistry::Sex::kMale;
						if (suit.mod.empty() || !suit.local) {
							REX::WARN("Erection: {}: bodysuit \"{}\" needs \"mod\" and \"formid\", skipped", file, suit.id);
							continue;
						}
						g_files.bodysuits.push_back(std::move(suit));
					}
				}
			}
			REX::INFO("Erection: {} file(s): morph \"{}\" (detected by \"{}\"), {} bodysuit(s)", paths.size(), g_files.erection, g_files.detect, g_files.bodysuits.size());
		}

		// Forms, once the game's data is loaded (scenes only run then).
		void ResolveLocked()
		{
			if (g_resolved) {
				return;
			}
			g_resolved = true;
			const auto handler = RE::TESDataHandler::GetSingleton();
			auto       lookup = [&](std::uint32_t a_local, const std::string& a_mod) -> RE::TESForm* {
				if (!handler || a_mod.empty() || !a_local) {
					return nullptr;
				}
				auto form = handler->LookupForm(a_local & 0x00FFFFFF, a_mod);
				return form ? form : handler->LookupForm(a_local & 0x00000FFF, a_mod);  // a light plugin
			};
			const auto keyword = lookup(g_files.keywordLocal, g_files.keywordMod);
			g_keyword = keyword && keyword->GetFormType() == RE::ENUM_FORM_ID::kKYWD ? keyword->GetFormID() : 0;
			if (!g_keyword) {
				REX::WARN("Erection: the morphs' keyword {:06X} in {} isn't a keyword: no morphs", g_files.keywordLocal, g_files.keywordMod);
			}
			for (auto& suit : g_files.bodysuits) {
				const auto form = lookup(suit.local, suit.mod);
				suit.form = form && form->GetFormType() == RE::ENUM_FORM_ID::kARMO ? form->GetFormID() : 0;
				if (suit.form) {
					REX::INFO("Erection: bodysuit \"{}\" is {:08X}", suit.id, suit.form);
				}
			}
		}

		void EnsureLocked()
		{
			if (!g_loaded) {
				LoadLocked();
			}
			ResolveLocked();
		}

		// LooksMenu's BodyGen script is there. Main thread.
		bool HasLooksMenu()
		{
			if (!g_looksMenu) {
				const auto vm = FourStim::GetVM();
				RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
				g_looksMenu = vm && vm->GetScriptObjectType(RE::BSFixedString("BodyGen"), type) && type;
				REX::INFO("Erection: LooksMenu's BodyGen {}", *g_looksMenu ? "found: morph bodies can be used" : "not found: no morph bodies");
			}
			return *g_looksMenu;
		}

		// Whether a .tri (a path under Meshes\) has a_morph: its name is in
		// the file as text. g_lock held.
		bool TriHas(const std::string& a_path, const std::string& a_morph)
		{
			auto bare = Lower(a_path);
			std::ranges::replace(bare, '/', '\\');
			if (bare.starts_with("meshes\\")) {
				bare = bare.substr(7);
			}
			if (bare.empty() || a_morph.empty()) {
				return false;
			}
			const auto key = bare + "|" + Lower(a_morph);
			if (const auto it = g_triCache.find(key); it != g_triCache.end()) {
				return it->second;
			}
			bool                         found = false;
			RE::BSResourceNiBinaryStream stream(("Meshes\\" + bare).c_str());
			if (stream) {
				std::vector<char> chunk(1 << 16);
				std::string       carry;
				for (std::size_t total = 0; !found && total < (64u << 20);) {
					const auto read = stream.binary_read(chunk.data(), chunk.size());
					if (read == 0) {
						break;
					}
					total += read;
					std::string window = carry + std::string(chunk.data(), read);
					found = window.find(a_morph) != std::string::npos;
					carry = window.size() >= a_morph.size() ? window.substr(window.size() - a_morph.size() + 1) : window;
				}
			}
			REX::INFO("Erection: {} {}", bare, !stream ? "isn't there" : found ? std::format("has the \"{}\" morph", a_morph) : std::format("has no \"{}\" morph", a_morph));
			g_triCache.emplace(key, found);
			return found;
		}

		// The .tri files a_actor's meshes name (LooksMenu's BODYTRI).
		void CollectTris(RE::NiAVObject* a_object, int a_depth, std::vector<std::string>& a_out)
		{
			if (!a_object) {
				return;
			}
			static const RE::BSFixedString key{ "BODYTRI" };
			if (const auto extra = static_cast<RE::NiStringExtraData*>(a_object->GetExtraData(key))) {
				const std::string file = extra->GetValue().c_str();
				if (!file.empty() && std::ranges::find(a_out, file) == a_out.end()) {
					a_out.push_back(file);
				}
			}
			if (const auto node = a_depth > 0 ? a_object->IsNode() : nullptr) {
				for (const auto& child : node->children) {
					CollectTris(child.get(), a_depth - 1, a_out);
				}
			}
		}

		// A body with the erection morph: one the meshes name, else one of
		// the files' .tri files. g_lock held.
		bool MorphBody(RE::Actor* a_actor)
		{
			std::vector<std::string> tris;
			CollectTris(a_actor->Get3D(false), 16, tris);
			for (const auto& tri : tris) {
				if (TriHas(tri, g_files.detect)) {
					return true;
				}
			}
			for (const auto& tri : g_files.triFiles) {
				if (TriHas(tri, g_files.detect)) {
					return true;
				}
			}
			return false;
		}

		bool IsSuit(std::uint32_t a_form)
		{
			return a_form && std::ranges::any_of(g_files.bodysuits, [&](const Bodysuit& s) { return s.form == a_form; });
		}

		// Nothing on in the body slot (33) but a bodysuit: their own body shows.
		bool Naked(RE::Actor* a_actor)
		{
			const auto worn = Undress::WornItems(a_actor, 1u << (33 - 30));
			return std::ranges::all_of(worn, [](std::uint32_t a_item) { return IsSuit(a_item); });
		}

		bool Wears(RE::Actor* a_actor, std::uint32_t a_item)
		{
			const auto worn = Undress::WornItems(a_actor);
			return std::ranges::find(worn, a_item) != worn.end();
		}

		std::uint32_t CountOf(RE::Actor* a_actor, std::uint32_t a_item)
		{
			std::uint32_t count = 0;
			if (const auto list = a_actor->inventoryList) {
				for (auto& item : list->data) {
					if (item.object && item.object->GetFormID() == a_item) {
						for (auto stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
							count += stack->GetCount();
						}
					}
				}
			}
			return count;
		}

		void Dispatch(std::string_view a_script, std::string_view a_function, std::uint32_t a_id, std::uint32_t a_item)
		{
			if (const auto vm = FourStim::GetVM()) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				std::vector<std::int32_t> items{ static_cast<std::int32_t>(a_item) };
				Compat::DispatchStaticCall(vm, a_script, a_function, callback, static_cast<std::int32_t>(a_id), items);
			}
		}

		void SetMorphs(std::uint32_t a_id, float a_erection, float a_up, float a_down)
		{
			if (const auto vm = FourStim::GetVM()) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(vm, "FourStimErection"sv, "SetMorphs"sv, callback, static_cast<std::int32_t>(a_id), static_cast<std::int32_t>(g_keyword),
					g_files.erection, a_erection, g_files.up, a_up, g_files.down, a_down);
			}
		}

		void ClearMorphs(std::uint32_t a_id)
		{
			if (const auto vm = FourStim::GetVM(); vm && g_keyword) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				Compat::DispatchStaticCall(vm, "FourStimErection"sv, "ClearMorphs"sv, callback, static_cast<std::int32_t>(a_id), static_cast<std::int32_t>(g_keyword));
			}
		}

		// The bodysuit for a_actor, if one's plugin is loaded. g_lock held.
		std::uint32_t SuitFor(RE::Actor* a_actor)
		{
			const auto sex = SceneRegistry::SexOf(a_actor);
			for (const auto& suit : g_files.bodysuits) {
				if (suit.form && (suit.sex == SceneRegistry::Sex::kAny || suit.sex == sex)) {
					return suit.form;
				}
			}
			return 0;
		}

		// Works out a_actor's body. Physics first (an SMP body may name a
		// .tri too), then morphs (the meshes or the files' .tri), then a
		// bodysuit once they're naked (their own body showing, so a .tri
		// install would have been seen by then). g_lock held.
		Method Detect(std::uint32_t a_id, RE::Actor* a_actor, State& a_state)
		{
			if (Physics::IsSwapped(a_id)) {
				return Method::kPhysics;
			}
			if (g_keyword && HasLooksMenu() && MorphBody(a_actor)) {
				return Method::kMorph;
			}
			if (!Naked(a_actor)) {
				return Method::kUnknown;  // their body isn't showing yet
			}
			if (const auto suit = SuitFor(a_actor)) {
				a_state.suit = suit;
				a_state.suitBefore = Wears(a_actor, suit);
				a_state.suitTheirs = CountOf(a_actor, suit) > 0;
				return Method::kBodysuit;
			}
			return Method::kNone;
		}

		// Puts a_state into effect on a_actor. g_lock held.
		void Apply(std::uint32_t a_id, RE::Actor* a_actor, State& a_state)
		{
			switch (a_state.method) {
			case Method::kMorph:
				{
					// The bend turns the skeleton's node; without one (no ZeX),
					// the body's own Erection Up / Down morphs bend it.
					const float erection = a_state.flaccid ? 0.0F : g_files.value;
					const float up = a_state.flaccid || a_state.hasNode ? 0.0F : std::clamp(a_state.steps / 9.0F, 0.0F, 1.0F);
					const float down = a_state.flaccid || a_state.hasNode ? 0.0F : std::clamp(-a_state.steps / 9.0F, 0.0F, 1.0F);
					if (erection != a_state.erection || up != a_state.up || down != a_state.down) {
						a_state.erection = erection;
						a_state.up = up;
						a_state.down = down;
						SetMorphs(a_id, erection, up, down);
					}
				}
				break;
			case Method::kBodysuit:
				{
					if (a_state.suitBefore) {
						break;  // theirs, worn before the scene: left as it was
					}
					const bool want = !a_state.flaccid && Naked(a_actor);
					const bool on = Wears(a_actor, a_state.suit);
					if (want && !on) {
						if (!a_state.suitOn) {
							REX::INFO("Erection: {:08X} puts the erect bodysuit {:08X} on", a_id, a_state.suit);
						}
						a_state.suitOn = true;
						Dispatch("FourStimUndress"sv, "WearObject"sv, a_id, a_state.suit);
					} else if (!want && on && a_state.suitOn) {
						REX::INFO("Erection: {:08X} takes the erect bodysuit off ({})", a_id, a_state.flaccid ? "flaccid" : "dressed");
						Dispatch("FourStimUndress"sv, "TakeOffObjects"sv, a_id, a_state.suit);
					}
				}
				break;
			default:
				break;
			}
		}

		// Undoes a_state on a_id (the scene is over). g_lock held.
		void Undo(std::uint32_t a_id, const State& a_state)
		{
			switch (a_state.method) {
			case Method::kMorph:
				if (a_state.erection >= 0.0F) {
					ClearMorphs(a_id);
					REX::INFO("Erection: {:08X} flaccid again (morphs removed)", a_id);
				}
				break;
			case Method::kBodysuit:
				if (a_state.suitOn && !a_state.suitBefore) {
					// Off, and away if 4Stim gave it (one they had stays theirs).
					Dispatch("FourStimUndress"sv, a_state.suitTheirs ? "TakeOffObjects"sv : "RemoveObjects"sv, a_id, a_state.suit);
					REX::INFO("Erection: {:08X} flaccid again (bodysuit off)", a_id);
				}
				break;
			default:
				break;  // a physics swap goes back with the scene (Physics.h)
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
		g_triCache.clear();
		LoadLocked();
	}

	void SceneStarted(const std::vector<std::uint32_t>& a_ids)
	{
		if (!g_config.enabled) {
			return;
		}
		std::scoped_lock lock(g_lock);
		EnsureLocked();
		for (const auto id : a_ids) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (const auto it = g_states.find(id); it != g_states.end()) {
				// Still erect from a scene just before (a new scene took them
				// over).
				it->second.flaccid = false;
				continue;
			}
			if (!actor || !ActorProperties::HasPenis(actor)) {
				continue;
			}
			g_states.emplace(id, State{});
		}
	}

	void SetBend(std::uint32_t a_id, int a_steps)
	{
		std::scoped_lock lock(g_lock);
		if (const auto it = g_states.find(a_id); it != g_states.end()) {
			auto& state = it->second;
			const bool flaccid = a_steps <= -10;
			const int  steps = flaccid ? 0 : a_steps;
			if (flaccid == state.flaccid && steps == state.steps) {
				return;
			}
			if (flaccid != state.flaccid) {
				REX::INFO("Erection: {:08X} {} (bend {})", a_id, flaccid ? "flaccid" : "erect", a_steps);
			}
			state.flaccid = flaccid;
			state.steps = steps;
			state.next = {};  // applied on the next tick
		}
	}

	void Tick()
	{
		std::scoped_lock lock(g_lock);
		if (g_states.empty()) {
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		for (auto& [id, state] : g_states) {
			if (now < state.next) {
				continue;
			}
			state.next = now + std::chrono::milliseconds(500);
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			const auto root = actor ? actor->Get3D(false) : nullptr;
			if (!root) {
				continue;
			}
			if (state.method == Method::kUnknown) {
				state.method = Detect(id, actor, state);
				if (state.method != Method::kUnknown) {
					REX::INFO("Erection: {:08X}'s body: {}", id, MethodName(state.method));
				}
			}
			state.hasNode = root->GetObjectByName(RE::BSFixedString(PenisBend::Settings().node)) != nullptr;
			Apply(id, actor, state);
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : a_ids) {
			if (const auto it = g_states.find(id); it != g_states.end()) {
				Undo(id, it->second);
				g_states.erase(it);
			}
		}
	}

	bool CanGoFlaccid(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto       it = g_states.find(a_id);
		return it != g_states.end() && (it->second.method == Method::kMorph || it->second.method == Method::kBodysuit);
	}

	bool IsErectionItem(std::uint32_t a_formID)
	{
		std::scoped_lock lock(g_lock);
		return IsSuit(a_formID);
	}

	std::uint32_t Given(std::uint32_t a_id)
	{
		std::scoped_lock lock(g_lock);
		const auto it = g_states.find(a_id);
		return it != g_states.end() && it->second.suitOn && !it->second.suitBefore && !it->second.suitTheirs ? it->second.suit : 0;
	}

	void AfterLoad(std::uint32_t a_id, std::uint32_t a_given)
	{
		std::scoped_lock lock(g_lock);
		EnsureLocked();
		if (g_keyword && HasLooksMenu()) {
			ClearMorphs(a_id);  // nothing to remove is fine
		}
		if (a_given && IsSuit(a_given)) {
			Dispatch("FourStimUndress"sv, "RemoveObjects"sv, a_id, a_given);
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		g_states.clear();
	}
}
