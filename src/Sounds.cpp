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

#include "Actions.h"
#include "Bridge.h"
#include "ActorProperties.h"
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
			Sound                    sound;
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
			// OStim's event reactions, by event id.
			std::unordered_map<std::string, ReactionSet> eventActorReactions;
			std::unordered_map<std::string, ReactionSet> eventTargetReactions;
			std::unordered_map<std::string, ReactionSet> eventPerformerReactions;
		};

		struct State
		{
			std::shared_ptr<const VoiceSet>       voice;
			bool                                  moan = false;
			bool                                  muffled = false;
			bool                                  muffledAlways = false;  // by an actor property, the whole scene
			bool                                  mutedByHand = false;    // OStim's OActor.Mute (Papyrus, the HUD's options)
			bool                                  mutedByProperty = false;  // an actor property's "mute" (a gag)
			float                                 cooldown = 0.0F;
			bool                                  playing = false;
			Playing                               sound;
			bool                                  eventExpression = false;
			const ReactionSet*                    reaction = nullptr;  // an event reaction waiting...
			float                                 reactionIn = 0.0F;   // ...this long
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
		std::optional<std::uint32_t> ReadFormRaw(const nlohmann::json& a_value, bool a_raw)
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
				entry.sound = ReadSound(e);
				if (entry.sound.Empty()) {
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

		ReactionSet ReadReactionSet(const nlohmann::json& a_set)
		{
			ReactionSet set;
			if (a_set.is_object()) {
				set.sound = ReadEntries(a_set, "sound");
				set.soundMuffled = ReadEntries(a_set, "soundMuffled");
			}
			return set;
		}

		ReactionSet ReadReaction(const nlohmann::json& a_root, const char* a_key)
		{
			const auto it = a_root.find(a_key);
			return it != a_root.end() ? ReadReactionSet(*it) : ReactionSet{};
		}

		// OStim's eventActorReactions...: {"<event id>": {"sound": [...], ...}}.
		std::unordered_map<std::string, ReactionSet> ReadReactionMap(const nlohmann::json& a_root, const char* a_key)
		{
			std::unordered_map<std::string, ReactionSet> out;
			if (const auto it = a_root.find(a_key); it != a_root.end() && it->is_object()) {
				for (const auto& [event, set] : it->items()) {
					out[Lower(event)] = ReadReactionSet(set);
				}
			}
			return out;
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
					voice->eventActorReactions = ReadReactionMap(root, "eventActorReactions");
					voice->eventTargetReactions = ReadReactionMap(root, "eventTargetReactions");
					voice->eventPerformerReactions = ReadReactionMap(root, "eventPerformerReactions");
					std::shared_ptr<const VoiceSet> shared = voice;
					auto target = [&](const nlohmann::json& a_form) {
						const auto id = ReadFormRaw(a_form, true);
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
		const SoundEntry* PlayReaction(RE::Actor* a_actor, State& a_state, const ReactionSet& a_set)
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
			a_state.sound = Play(entry.sound, a_actor, g_config.moanVolume);
			if (!a_state.sound.started) {
				return nullptr;
			}
			a_state.playing = true;
			return &entry;
		}


		void StartReaction(std::uint32_t a_id, State& a_state, const ReactionSet& a_set, const char* a_fallbackEvent)
		{
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
			if (!actor) {
				return;
			}
			const auto entry = PlayReaction(actor, a_state, a_set);
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

	std::uint32_t ReadForm(const nlohmann::json& a_value)
	{
		return ReadFormRaw(a_value, false).value_or(0);
	}

	Sound ReadSound(const nlohmann::json& a_entry)
	{
		Sound sound;
		if (!a_entry.is_object()) {
			return sound;
		}
		if (const auto file = a_entry.find("file"); file != a_entry.end() && file->is_string()) {
			sound.files = ReadFiles(file->get<std::string>());
		} else if (const auto form = a_entry.find("sound"); form != a_entry.end()) {
			sound.descriptor = ReadForm(*form);
		}
		return sound;
	}

	bool Playing::IsPlaying() const
	{
		if (!started || std::chrono::steady_clock::now() >= until) {
			return false;
		}
		// A descriptor (no handle) counts as playing until its estimate.
		return handle.soundID == static_cast<std::uint32_t>(-1) || handle.IsPlaying();
	}

	void Playing::Stop()
	{
		if (started && handle.soundID != static_cast<std::uint32_t>(-1)) {
			handle.FadeOutAndRelease(200);
		}
		started = false;
	}

	Playing Play(const Sound& a_sound, RE::Actor* a_actor, float a_volume)
	{
		Playing playing;
		if (a_sound.Empty() || !a_actor || g_config.disabled || a_volume <= 0.0F) {
			return playing;
		}
		if (!a_sound.files.empty()) {
			const auto audio = RE::BSAudioManager::GetSingleton();
			const auto root = a_actor->Get3D();
			if (!audio || !root) {
				return playing;
			}
			const auto& file = a_sound.files[std::uniform_int_distribution<std::size_t>(0, a_sound.files.size() - 1)(g_random)];
			audio->GetSoundHandleByFile(playing.handle, RE::BSResource::ID(file.c_str()), g_config.flags, g_config.priority);
			if (playing.handle.soundID == static_cast<std::uint32_t>(-1)) {
				REX::WARN("Sound: couldn't load {}", file);
				return playing;
			}
			if (a_volume < 1.0F) {
				// Attenuation in hundredths of a decibel.
				const float db = -20.0F * std::log10(a_volume);
				playing.handle.SetStaticAttenuation(static_cast<std::uint16_t>(std::clamp(db * 100.0F, 0.0F, 10000.0F)));
			}
			playing.handle.SetObjectToFollow(root);
			playing.handle.Play();
			playing.until = std::chrono::steady_clock::now() + std::chrono::seconds(10);  // safety net
		} else {
			// A sound descriptor: through Papyrus (Sound.Play), which has no
			// volume; its length isn't known here, so it counts as playing
			// for 2 s.
			const auto vm = FourStim::GetVM();
			if (!vm) {
				return playing;
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("FourStimScene"sv, "PlaySoundForm"sv, callback, static_cast<std::int32_t>(a_sound.descriptor),
				static_cast<std::int32_t>(a_actor->GetFormID()));
			playing.until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		}
		playing.started = true;
		return playing;
	}

	void ReactToEvent(std::uint32_t a_id, const std::vector<std::string>& a_events, std::string_view a_which, float a_delay)
	{
		const auto it = g_states.find(a_id);
		if (it == g_states.end() || !it->second.voice) {
			return;
		}
		auto&       state = it->second;
		const auto& voice = *state.voice;
		const auto& map = a_which == "target"    ? voice.eventTargetReactions :
		                  a_which == "performer" ? voice.eventPerformerReactions :
		                                           voice.eventActorReactions;
		// As OStim's reactToEvent: the event's own reaction, else its
		// supertype's.
		for (const auto& event : a_events) {
			if (const auto found = map.find(event); found != map.end()) {
				state.reaction = &found->second;
				state.reactionIn = std::max(a_delay, 0.0F);
				return;
			}
		}
	}

	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role)
	{
		if (a_role >= a_scene.actors.size()) {
			return;
		}
		auto [it, added] = g_states.try_emplace(a_id);
		auto& state = it->second;
		const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
		if (added) {
			// Muted by an actor property (OStim's "mute", a gag...): no voice.
			state.mutedByProperty = ActorProperties::Muted(actor);
			state.voice = state.mutedByProperty ? nullptr : VoiceFor(actor);
			state.cooldown = RandomInterval();
			state.muffledAlways = ActorProperties::Muffled(actor);  // OStim's actor property "muffle"
		}
		state.moan = a_scene.actors[a_role].moan;
		state.muffled = a_scene.actors[a_role].muffled || state.muffledAlways;
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
		if (state.playing) {
			state.sound.Stop();
		}
		state.playing = false;
		state.reaction = nullptr;
		if (state.voice && !state.mutedByHand) {
			StartReaction(a_id, state, state.voice->climax, "climax");
		} else {
			Expressions::SetEvent(a_id, "climax", true);
		}
	}

	void Tick(float a_seconds)
	{
		for (auto& [id, state] : g_states) {
			// An event reaction (OStim's eventTimer): when it's due, it cuts
			// in over a moan.
			if (state.reaction && (state.reactionIn -= a_seconds) <= 0.0F) {
				const auto reaction = state.reaction;
				state.reaction = nullptr;
				if (state.mutedByHand) {
					continue;
				}
				if (state.playing) {
					state.sound.Stop();
					state.playing = false;
				}
				StartReaction(id, state, *reaction, nullptr);
				continue;
			}
			if (state.playing) {
				if (state.sound.IsPlaying()) {
					continue;
				}
				state.playing = false;
				if (state.eventExpression) {
					state.eventExpression = false;
					Expressions::ClearEvent(id);
				}
				continue;
			}
			if (!state.moan || !state.voice || state.mutedByHand) {
				continue;
			}
			if ((state.cooldown -= a_seconds) <= 0.0F) {
				StartReaction(id, state, state.voice->moan, nullptr);
			}
		}
	}

	void SetMuted(std::uint32_t a_id, bool a_muted)
	{
		const auto it = g_states.find(a_id);
		if (it == g_states.end()) {
			return;
		}
		it->second.mutedByHand = a_muted;
		if (a_muted && it->second.playing) {
			it->second.sound.Stop();
			it->second.playing = false;
		}
		REX::INFO("Sound: {:08X} {}", a_id, a_muted ? "muted" : "unmuted");
	}

	bool IsMuted(std::uint32_t a_id)
	{
		const auto it = g_states.find(a_id);
		return it != g_states.end() && (it->second.mutedByHand || it->second.mutedByProperty);
	}

	void Leave(const std::vector<std::uint32_t>& a_ids)
	{
		for (const auto id : a_ids) {
			const auto it = g_states.find(id);
			if (it == g_states.end()) {
				continue;
			}
			if (it->second.playing) {
				it->second.sound.Stop();
			}
			g_states.erase(it);
		}
	}

	void Clear()
	{
		g_states.clear();
	}
}
