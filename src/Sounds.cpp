#include "Sounds.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Bridge.h"
#include "Expressions.h"

namespace Sounds
{
	namespace
	{
		constexpr auto VOICE_SET_FOLDER = "Data/F4SE/Plugins/4Stim/Voice Sets";  // OStim: SKSE/Plugins/OStim/voice sets

		// One sound a reaction can play: OStim's sound descriptor, or 4Stim's
		// sound files (one picked at random).
		struct SoundEntry
		{
			std::uint32_t            descriptor = 0;  // a sound descriptor (SNDR) form
			std::vector<std::string> files;           // paths under Data\ ("Sound\...")
			std::string              expression;      // event expression while it plays
			float                    moanIntervalOverride = -1.0F;  // seconds; < 0 = none
			// 4Stim, in place of OStim's perk "condition": only while the
			// actor takes part in one of these actions (ids). Empty = always.
			std::vector<std::string> actions;
		};

		struct ReactionSet
		{
			std::vector<SoundEntry> sound;
			std::vector<SoundEntry> soundMuffled;
		};

		struct VoiceSet
		{
			std::string name;
			ReactionSet moan;
			ReactionSet climax;
		};

		struct State
		{
			std::shared_ptr<const VoiceSet>       voice;
			bool                                  moan = false;
			bool                                  muffled = false;
			float                                 cooldown = 0.0F;
			bool                                  playing = false;
			RE::BSSoundHandle                     handle;
			std::chrono::steady_clock::time_point playingUntil{};  // descriptor sounds: an estimate
			bool                                  eventExpression = false;
			std::vector<std::string>              actions;  // action type ids the actor takes part in
		};

		Config g_config;

		bool                                                          g_loaded = false;
		std::unordered_map<std::uint32_t, std::shared_ptr<const VoiceSet>> g_byTarget;  // NPC or voice type form ID
		std::shared_ptr<const VoiceSet>                               g_defaultMale, g_defaultFemale;
		std::unordered_map<std::uint32_t, State>                      g_states;
		std::mt19937                                                  g_random{ std::random_device{}() };

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		// {"mod": "Fallout4.esm", "formid": "0x..."} -> its full form ID, or
		// the raw ID for OStim's default targets (0 male, 1 female).
		std::optional<std::uint32_t> ReadForm(const nlohmann::json& a_value, bool a_raw = false)
		{
			if (!a_value.is_object()) {
				return std::nullopt;
			}
			const auto formID = a_value.value("formid", std::string{});
			const auto mod = a_value.value("mod", std::string{});
			if (formID.empty()) {
				return std::nullopt;
			}
			std::uint32_t local = 0;
			try {
				local = static_cast<std::uint32_t>(std::stoul(formID, nullptr, 16));
			} catch (...) {
				return std::nullopt;
			}
			if (a_raw && local <= 1) {
				return local;
			}
			const auto handler = RE::TESDataHandler::GetSingleton();
			if (!handler || mod.empty()) {
				return std::nullopt;
			}
			auto form = handler->LookupForm(local & 0x00FFFFFF, mod);
			if (!form) {
				form = handler->LookupForm(local & 0x00000FFF, mod);
			}
			return form ? std::optional<std::uint32_t>(form->GetFormID()) : std::nullopt;
		}

		// "file": a sound file or a folder of them, under Data\Sound\.
		std::vector<std::string> ReadFiles(std::string a_path)
		{
			std::ranges::replace(a_path, '/', '\\');
			if (Lower(a_path).starts_with("sound\\")) {
				a_path = a_path.substr(6);
			}
			std::vector<std::string> files;
			const auto      full = std::filesystem::path("Data\\Sound") / a_path;
			std::error_code ec;
			if (std::filesystem::is_directory(full, ec)) {
				for (const auto& e : std::filesystem::directory_iterator(full, ec)) {
					const auto ext = Lower(e.path().extension().string());
					if (e.is_regular_file() && (ext == ".wav" || ext == ".xwm")) {
						files.push_back("Sound\\" + a_path + "\\" + e.path().filename().string());
					}
				}
				std::ranges::sort(files);
			} else {
				files.push_back("Sound\\" + a_path);
			}
			return files;
		}

		std::vector<SoundEntry> ReadEntries(const nlohmann::json& a_set, const char* a_key)
		{
			std::vector<SoundEntry> out;
			const auto list = a_set.find(a_key);
			if (list == a_set.end() || !list->is_array()) {
				return out;
			}
			for (const auto& e : *list) {
				if (!e.is_object()) {
					continue;
				}
				SoundEntry entry;
				if (const auto file = e.find("file"); file != e.end() && file->is_string()) {
					entry.files = ReadFiles(file->get<std::string>());
				} else if (const auto sound = e.find("sound"); sound != e.end()) {
					entry.descriptor = ReadForm(*sound).value_or(0);
				}
				if (entry.files.empty() && entry.descriptor == 0) {
					continue;
				}
				entry.expression = Lower(e.value("expression", std::string{}));
				if (const auto acts = e.find("actions"); acts != e.end() && acts->is_array()) {
					for (const auto& a : *acts) {
						if (a.is_string()) {
							entry.actions.push_back(Lower(a.get<std::string>()));
						}
					}
				}
				if (const auto o = e.find("moanIntervalOverride"); o != e.end() && o->is_number()) {
					entry.moanIntervalOverride = o->get<float>();
				}
				out.push_back(std::move(entry));
			}
			return out;
		}

		ReactionSet ReadReaction(const nlohmann::json& a_root, const char* a_key)
		{
			ReactionSet set;
			if (const auto it = a_root.find(a_key); it != a_root.end() && it->is_object()) {
				set.sound = ReadEntries(*it, "sound");
				set.soundMuffled = ReadEntries(*it, "soundMuffled");
			}
			return set;
		}

		void Load()
		{
			g_loaded = true;
			g_byTarget.clear();
			g_defaultMale.reset();
			g_defaultFemale.reset();
			std::error_code ec;
			int             count = 0;
			for (const auto& e : std::filesystem::directory_iterator(VOICE_SET_FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					auto          voice = std::make_shared<VoiceSet>();
					voice->name = root.value("name", e.path().stem().string());
					voice->moan = ReadReaction(root, "moan");
					voice->climax = ReadReaction(root, "climax");
					std::shared_ptr<const VoiceSet> shared = voice;
					auto target = [&](const nlohmann::json& a_form) {
						const auto id = ReadForm(a_form, true);
						if (!id) {
							return;
						}
						if (*id == 0) {
							g_defaultMale = shared;
						} else if (*id == 1) {
							g_defaultFemale = shared;
						} else {
							g_byTarget[*id] = shared;
						}
					};
					target(root.value("target", nlohmann::json::object()));
					if (const auto aliases = root.find("aliases"); aliases != root.end() && aliases->is_array()) {
						for (const auto& a : *aliases) {
							target(a);
						}
					}
					++count;
				} catch (const std::exception& ex) {
					REX::WARN("Sound: {}: couldn't be read ({})", e.path().filename().string(), ex.what());
				}
			}
			REX::INFO("Sound: {} voice set(s){}{}", count, g_defaultMale ? "" : ", no default male one", g_defaultFemale ? "" : ", no default female one");
		}

		// As OStim: the actor's own NPC, then their voice type, then the
		// default for their sex.
		std::shared_ptr<const VoiceSet> VoiceFor(RE::Actor* a_actor)
		{
			if (!g_loaded) {
				Load();
			}
			if (!a_actor) {
				return nullptr;
			}
			if (const auto npc = a_actor->GetNPC()) {
				if (const auto it = g_byTarget.find(npc->GetFormID()); it != g_byTarget.end()) {
					return it->second;
				}
				if (npc->voiceType) {
					if (const auto it = g_byTarget.find(npc->voiceType->GetFormID()); it != g_byTarget.end()) {
						return it->second;
					}
				}
			}
			return a_actor->GetSex() == RE::SEX::kFemale ? g_defaultFemale : g_defaultMale;
		}

		float RandomInterval()
		{
			const float lo = std::max(g_config.moanIntervalMin, 0.2F);
			const float hi = std::max(g_config.moanIntervalMax, lo);
			return std::uniform_real_distribution<float>(lo, hi)(g_random);
		}

		// Plays a random entry of a_set (muffled ones when muffled) on
		// a_actor. Returns the entry played, or null.
		const SoundEntry* Play(RE::Actor* a_actor, State& a_state, const ReactionSet& a_set)
		{
			std::vector<const SoundEntry*> list;
			for (const auto& e : a_state.muffled ? a_set.soundMuffled : a_set.sound) {
				if (e.actions.empty() || std::ranges::any_of(e.actions, [&](const std::string& a) { return std::ranges::find(a_state.actions, a) != a_state.actions.end(); })) {
					list.push_back(&e);
				}
			}
			if (list.empty() || g_config.disabled || g_config.moanVolume <= 0.0F) {
				return nullptr;
			}
			const auto& entry = *list[std::uniform_int_distribution<std::size_t>(0, list.size() - 1)(g_random)];
			if (!entry.files.empty()) {
				const auto audio = RE::BSAudioManager::GetSingleton();
				const auto root = a_actor->Get3D();
				if (!audio || !root) {
					return nullptr;
				}
				const auto&       file = entry.files[std::uniform_int_distribution<std::size_t>(0, entry.files.size() - 1)(g_random)];
				RE::BSSoundHandle handle;
				audio->GetSoundHandleByFile(handle, RE::BSResource::ID(file.c_str()), g_config.flags, g_config.priority);
				if (handle.soundID == static_cast<std::uint32_t>(-1)) {
					REX::WARN("Sound: couldn't load {}", file);
					return nullptr;
				}
				if (g_config.moanVolume < 1.0F) {
					// Attenuation in hundredths of a decibel.
					const float db = -20.0F * std::log10(g_config.moanVolume);
					handle.SetStaticAttenuation(static_cast<std::uint16_t>(std::clamp(db * 100.0F, 0.0F, 10000.0F)));
				}
				handle.SetObjectToFollow(root);
				handle.Play();
				a_state.handle = handle;
				a_state.playingUntil = std::chrono::steady_clock::now() + std::chrono::seconds(10);  // safety net
			} else {
				// A sound descriptor: through Papyrus (Sound.Play); its length
				// isn't known here, so it counts as playing for 2 s.
				const auto vm = FourStim::GetVM();
				if (!vm) {
					return nullptr;
				}
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				vm->DispatchStaticCall("FourStimScene"sv, "PlaySoundForm"sv, callback, static_cast<std::int32_t>(entry.descriptor),
					static_cast<std::int32_t>(a_actor->GetFormID()));
				a_state.handle = RE::BSSoundHandle{};
				a_state.playingUntil = std::chrono::steady_clock::now() + std::chrono::seconds(2);
			}
			a_state.playing = true;
			return &entry;
		}

		bool StillPlaying(State& a_state)
		{
			if (std::chrono::steady_clock::now() >= a_state.playingUntil) {
				return false;
			}
			return a_state.handle.soundID == static_cast<std::uint32_t>(-1) || a_state.handle.IsPlaying();
		}

		void StartReaction(std::uint32_t a_id, State& a_state, const ReactionSet& a_set, const char* a_fallbackEvent)
		{
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
			if (!actor) {
				return;
			}
			const auto entry = Play(actor, a_state, a_set);
			const auto event = entry && !entry->expression.empty() ? entry->expression : std::string(a_fallbackEvent ? a_fallbackEvent : "");
			if (!event.empty()) {
				// With a sound: until it ends; without, for the expression's own duration.
				a_state.eventExpression = Expressions::SetEvent(a_id, event, entry == nullptr) && entry != nullptr;
			}
			a_state.cooldown = entry && entry->moanIntervalOverride > 0.0F ? entry->moanIntervalOverride : RandomInterval();
		}
	}

	Config& Settings()
	{
		return g_config;
	}

	void Reload()
	{
		Load();
	}

	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role)
	{
		if (a_role >= a_scene.actors.size()) {
			return;
		}
		auto [it, added] = g_states.try_emplace(a_id);
		auto& state = it->second;
		if (added) {
			state.voice = VoiceFor(RE::TESForm::GetFormByID<RE::Actor>(a_id));
			state.cooldown = RandomInterval();
		}
		state.moan = a_scene.actors[a_role].moan;
		state.muffled = a_scene.actors[a_role].muffled;
		state.actions.clear();
		for (const auto& action : a_scene.actions) {
			if (action.actor == a_role || action.target == a_role || action.performer == a_role) {
				state.actions.push_back(action.type->id);
			}
		}
	}

	void Climax(std::uint32_t a_id)
	{
		const auto it = g_states.find(a_id);
		if (it == g_states.end()) {
			Expressions::SetEvent(a_id, "climax", true);
			return;
		}
		auto& state = it->second;
		if (state.playing && state.handle.soundID != static_cast<std::uint32_t>(-1)) {
			state.handle.Stop();
		}
		state.playing = false;
		if (state.voice) {
			StartReaction(a_id, state, state.voice->climax, "climax");
		} else {
			Expressions::SetEvent(a_id, "climax", true);
		}
	}

	void Tick(float a_seconds)
	{
		for (auto& [id, state] : g_states) {
			if (state.playing) {
				if (StillPlaying(state)) {
					continue;
				}
				state.playing = false;
				if (state.eventExpression) {
					state.eventExpression = false;
					Expressions::ClearEvent(id);
				}
				continue;
			}
			if (!state.moan || !state.voice) {
				continue;
			}
			if ((state.cooldown -= a_seconds) <= 0.0F) {
				StartReaction(id, state, state.voice->moan, nullptr);
			}
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		for (const auto id : a_ids) {
			const auto it = g_states.find(id);
			if (it == g_states.end()) {
				continue;
			}
			if (it->second.playing && it->second.handle.soundID != static_cast<std::uint32_t>(-1)) {
				it->second.handle.FadeOutAndRelease(200);
			}
			g_states.erase(it);
		}
	}

	void Clear()
	{
		g_states.clear();
	}
}
