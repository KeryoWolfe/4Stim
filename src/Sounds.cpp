#include "Sounds.h"
#include "Translations.h"
#include "compat/ScrapFunction.h"
#include "compat/Fallbacks.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
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
			// OStim's "condition": a perk whose conditions must hold, with the
			// actor as subject and their primary partner as target. 0 = none.
			std::uint32_t            condition = 0;
			// 4Stim's addition: only while the actor takes part in one of
			// these actions (ids). Empty = always.
			std::vector<std::string> actions;
			// OStim's dialogue entries: a dialogue topic (DIAL) said instead of
			// a sound, and dialogueThreshold (said once the countdown is at or
			// below it).
			std::uint32_t            topic = 0;
			int                      threshold = 0;
		};

		struct ReactionSet
		{
			std::vector<SoundEntry> sound;
			std::vector<SoundEntry> soundMuffled;
			std::vector<SoundEntry> dialogue;  // OStim's

			[[nodiscard]] bool Empty() const { return sound.empty() && soundMuffled.empty() && dialogue.empty(); }
		};

		struct VoiceSet
		{
			// What a choice saves (OStim saves the target's form ID): the
			// target's form ID as a signed number, as OStim's
			// GetVoiceSetPairs; for a set without a target (4Stim's mute),
			// its file name. "" for the default sets, which aren't listed.
			std::string key;
			std::string file;          // its file name, lowercase, without .json (sPlayerVoice may name it)
			std::uint32_t target = 0;  // its target's form ID (0: none, or a default set)
			std::string name;
			ReactionSet moan;
			ReactionSet climax;
			ReactionSet climaxCommentSelf;   // OStim's: after their own climax (NPCs)
			ReactionSet climaxCommentOther;  // OStim's: after a partner's climax
			// OStim's event reactions, by event id.
			std::unordered_map<std::string, ReactionSet> eventActorReactions;
			std::unordered_map<std::string, ReactionSet> eventTargetReactions;
			std::unordered_map<std::string, ReactionSet> eventPerformerReactions;
			std::uint32_t                                postSceneDialogue = 0;  // OStim's: a topic said as the scene ends
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
			float                                 reactionIn = 0.0F;   // ...this long, counted once the sound playing has ended (OStim's eventTimer)
			bool                                  heldForClimax = false;  // OStim's awaitingClimaxInner: nothing new starts
			std::vector<std::string>              actions;  // action type ids the actor takes part in
			std::uint32_t                         partner = 0;  // OStim's primary partner (conditions' target); 0 = themselves
			bool                                  talk = false;     // the scene's actions let them talk (OStim's graphActor->talk)
			bool                                  talking = false;  // saying a line now (OStim's isTalking)
			float                                 grace = 0.0F;     // OStim's soundGracePeriod: nothing new for this long
			int                                   dialogueCountdown = 0;
			bool                                  female = false;
			bool                                  player = false;
		};

		Config g_config;

		// The voice sets: read on the main thread and from Papyrus (the
		// voice set list), so behind a lock. Scene state (g_states) is the
		// main thread's only.
		std::recursive_mutex                                          g_voiceLock;
		bool                                                          g_loaded = false;
		std::unordered_map<std::uint32_t, std::shared_ptr<const VoiceSet>> g_byTarget;  // NPC, voice type or race form ID
		std::shared_ptr<const VoiceSet>                               g_defaultMale, g_defaultFemale;
		std::vector<std::shared_ptr<const VoiceSet>>                  g_listed;  // the ones a player can choose (OStim: every set but the defaults)

		// The player's choices (OStim's Serialization::getVoiceSet): actor
		// base form ID -> voice set key. Saved with the game.
		std::mutex                                                    g_choiceLock;
		std::unordered_map<std::uint32_t, std::string>                g_choices;
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

		// OStim's loadSoundSets: a list of entries, or (as OStim) one sound
		// on its own, a sound descriptor ({"mod", "formid"}) or 4Stim's
		// {"file"}, which plays with a_expression.
		std::vector<SoundEntry> ReadEntries(const nlohmann::json& a_set, const char* a_key, const std::string& a_expression = {})
		{
			std::vector<SoundEntry> out;
			const auto list = a_set.find(a_key);
			if (list == a_set.end()) {
				return out;
			}
			if (list->is_object()) {
				SoundEntry entry;
				if (list->contains("file")) {
					entry.sound = ReadSound(*list);
				} else {
					entry.sound.descriptor = ReadForm(*list);
				}
				if (entry.sound.Empty()) {
					REX::WARN("Sound: \"{}\" ({}) isn't a loaded sound", a_key, list->dump());
				} else {
					entry.expression = Lower(a_expression);
					out.push_back(std::move(entry));
				}
				return out;
			}
			if (!list->is_array()) {
				REX::WARN("Sound: \"{}\" is neither a list nor a sound", a_key);
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
				entry.expression = Lower(e.value("expression", a_expression));
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
				if (const auto c = e.find("condition"); c != e.end()) {
					entry.condition = ReadFormRaw(*c, false).value_or(0);
					if (!entry.condition || !RE::TESForm::GetFormByID<RE::BGSPerk>(entry.condition)) {
						// As OStim: a condition that can't be found doesn't count
						// (the entry always fits); 4Stim says so in the log.
						REX::WARN("Sound: a \"{}\" entry's condition perk ({}) isn't loaded: the entry plays without it", a_key, c->dump());
						entry.condition = 0;
					}
				}
				out.push_back(std::move(entry));
			}
			return out;
		}

		// OStim's loadDialogueSets: a list of {"dialogue": topic, "condition",
		// "expression", "dialogueThreshold", "moanIntervalOverride"}, or one
		// topic on its own.
		std::vector<SoundEntry> ReadDialogue(const nlohmann::json& a_set)
		{
			std::vector<SoundEntry> out;
			const auto list = a_set.find("dialogue");
			if (list == a_set.end()) {
				return out;
			}
			auto topicOf = [](const nlohmann::json& a_form) -> std::uint32_t {
				const auto id = ReadForm(a_form);
				return id && RE::TESForm::GetFormByID<RE::TESTopic>(id) ? id : 0;
			};
			if (list->is_object()) {
				SoundEntry entry;
				entry.topic = topicOf(*list);
				if (entry.topic) {
					out.push_back(std::move(entry));
				} else {
					REX::WARN("Sound: \"dialogue\" ({}) isn't a loaded dialogue topic", list->dump());
				}
				return out;
			}
			if (!list->is_array()) {
				return out;
			}
			for (const auto& e : *list) {
				if (!e.is_object() || !e.contains("dialogue")) {
					continue;
				}
				SoundEntry entry;
				entry.topic = topicOf(e["dialogue"]);
				if (!entry.topic) {
					REX::WARN("Sound: a dialogue entry's topic ({}) isn't loaded", e["dialogue"].dump());
					continue;
				}
				entry.expression = Lower(e.value("expression", std::string{}));
				entry.threshold = e.value("dialogueThreshold", 0);
				if (const auto o = e.find("moanIntervalOverride"); o != e.end() && o->is_number()) {
					entry.moanIntervalOverride = o->get<float>();
				}
				if (const auto c = e.find("condition"); c != e.end()) {
					entry.condition = ReadFormRaw(*c, false).value_or(0);
					if (!entry.condition || !RE::TESForm::GetFormByID<RE::BGSPerk>(entry.condition)) {
						entry.condition = 0;
					}
				}
				out.push_back(std::move(entry));
			}
			return out;
		}

		ReactionSet ReadReactionSet(const nlohmann::json& a_set)
		{
			ReactionSet set;
			if (a_set.is_object()) {
				// OStim's soundExpression: the entries' expression when they name none.
				const auto expression = a_set.value("soundExpression", std::string{});
				set.sound = ReadEntries(a_set, "sound", expression);
				set.soundMuffled = ReadEntries(a_set, "soundMuffled", expression);
				set.dialogue = ReadDialogue(a_set);
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

		void LoadLocked()
		{
			g_loaded = true;
			g_byTarget.clear();
			g_defaultMale.reset();
			g_defaultFemale.reset();
			g_listed.clear();
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
					voice->name = Translations::Localize(root.value("name", e.path().stem().string()));  // translated, as OStim
					voice->file = Lower(e.path().stem().string());
					const auto file = e.path().filename().string();
					if (const auto moan = root.find("moan"); moan != root.end() && moan->is_object() && moan->contains("mod")) {
						// OStim's legacy syntax (its own defaultmale.json): one sound
						// each for moan, moanMuffled, climax and climaxMuffled, with
						// moanExpression / climaxExpression (default "moan" /
						// "climax"). As OStim, nothing else is read then.
						const auto moanExpression = root.value("moanExpression", std::string("moan"));
						const auto climaxExpression = root.value("climaxExpression", std::string("climax"));
						voice->moan.sound = ReadEntries(root, "moan", moanExpression);
						voice->moan.soundMuffled = ReadEntries(root, "moanMuffled", moanExpression);
						voice->climax.sound = ReadEntries(root, "climax", climaxExpression);
						voice->climax.soundMuffled = ReadEntries(root, "climaxMuffled", climaxExpression);
						REX::INFO("Sound: {}: OStim's legacy voice set syntax", file);
					} else {
						voice->moan = ReadReaction(root, "moan");
						voice->climax = ReadReaction(root, "climax");
						voice->climaxCommentSelf = ReadReaction(root, "climaxCommentSelf");
						voice->climaxCommentOther = ReadReaction(root, "climaxCommentOther");
						voice->eventActorReactions = ReadReactionMap(root, "eventActorReactions");
						voice->eventTargetReactions = ReadReactionMap(root, "eventTargetReactions");
						voice->eventPerformerReactions = ReadReactionMap(root, "eventPerformerReactions");
					}
					if (const auto post = root.find("postSceneDialogue"); post != root.end()) {
						const auto id = ReadForm(*post);
						voice->postSceneDialogue = id && RE::TESForm::GetFormByID<RE::TESTopic>(id) ? id : 0;
						if (!voice->postSceneDialogue) {
							REX::WARN("Sound: {}: postSceneDialogue isn't a loaded dialogue topic", file);
						}
					}
					// OStim's "voice" (lines said in another voice type): Fallout 4's
					// Say can only borrow another actor's voice, so not read.
					for (const auto field : { "voice" }) {
						if (root.contains(field)) {
							REX::INFO("Sound: {}: \"{}\" isn't supported yet: left out", file, field);
						}
					}
					if (voice->moan.sound.empty() && voice->moan.soundMuffled.empty() && voice->climax.sound.empty() && voice->climax.soundMuffled.empty()) {
						REX::WARN("Sound: {}: no moan or climax sound read from it", file);
					}
					const auto targetJSON = root.value("target", nlohmann::json::object());
					const auto targetID = ReadFormRaw(targetJSON, true);
					if (targetID && *targetID > 1) {
						voice->target = *targetID;
						voice->key = std::to_string(static_cast<std::int32_t>(*targetID));
					} else if (!targetID && !targetJSON.contains("formid")) {
						// 4Stim's: no target, chosen by name only (OStim skips such a file).
						voice->key = Lower(e.path().stem().string());
						REX::INFO("Sound: {}: no \"target\": only used when chosen", file);
					} else if (!targetID) {
						REX::WARN("Sound: {}: its target isn't loaded, so it's not used", e.path().filename().string());
						continue;
					}
					std::shared_ptr<const VoiceSet> shared = voice;
					if (!voice->key.empty()) {
						g_listed.push_back(shared);
					}
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
					target(targetJSON);
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
			std::ranges::sort(g_listed, [](const auto& a, const auto& b) { return Lower(a->name) < Lower(b->name); });
			REX::INFO("Sound: {} voice set(s), {} to choose from{}{}", count, g_listed.size(), g_defaultMale ? "" : ", no default male one",
				g_defaultFemale ? "" : ", no default female one");
		}

		void EnsureLoadedLocked()
		{
			if (!g_loaded) {
				LoadLocked();
			}
		}

		std::shared_ptr<const VoiceSet> ByKeyLocked(const std::string& a_key)
		{
			const auto it = std::ranges::find_if(g_listed, [&](const auto& v) { return v->key == a_key; });
			return it != g_listed.end() ? *it : nullptr;
		}

		std::uint32_t BaseOf(RE::Actor* a_actor)
		{
			const auto npc = a_actor ? a_actor->GetNPC() : nullptr;
			return npc ? npc->GetFormID() : 0;
		}

		// OStim's GameActor::isHuman: not a child, and an NPC (the
		// ActorTypeNPC keyword, on the race or the NPC). OStim also leaves
		// out Skyrim's mannequins, which Fallout 4 doesn't have.
		bool IsHuman(RE::Actor* a_actor)
		{
			if (a_actor->IsChild()) {
				return false;
			}
			const auto npc = a_actor->GetNPC();
			return (a_actor->race && a_actor->race->HasKeywordString("ActorTypeNPC")) || (npc && npc->HasKeywordString("ActorTypeNPC"));
		}

		// As OStim's SoundTable::getVoiceSet: the player's choice for this
		// actor, the actor's own NPC, their voice type (else their race's
		// default voice for their sex, as OStim's GameActor::getVoice), their
		// race, and last the default for their sex, for humans only (a
		// creature with no set of its own makes no sound).
		std::shared_ptr<const VoiceSet> VoiceFor(RE::Actor* a_actor)
		{
			std::scoped_lock lock(g_voiceLock);
			EnsureLoadedLocked();
			if (!a_actor) {
				return nullptr;
			}
			const char* name = a_actor->GetDisplayFullName();
			const auto  found = [&](const std::shared_ptr<const VoiceSet>& a_set, const char* a_how) {
				REX::INFO("Sound: voice set \"{}\" for {:08X} ({}) by {}", a_set->name, a_actor->GetFormID(), name ? name : "", a_how);
				return a_set;
			};
			const auto base = BaseOf(a_actor);
			std::string chosen;
			{
				std::scoped_lock choiceLock(g_choiceLock);
				if (const auto it = g_choices.find(base); it != g_choices.end()) {
					chosen = it->second;
				}
			}
			if (chosen.empty() && a_actor == RE::PlayerCharacter::GetSingleton() && !g_config.playerVoice.empty() && g_config.playerVoice != "0") {
				// 4Stim.ini's sPlayerVoice: a voice set's key, file name or name.
				const auto want = Lower(g_config.playerVoice);
				const auto it = std::ranges::find_if(g_listed, [&](const auto& v) { return v->key == want || v->file == want || Lower(v->name) == want; });
				if (it != g_listed.end()) {
					return found(*it, "sPlayerVoice");
				}
				REX::WARN("Sound: sPlayerVoice \"{}\" isn't a voice set", g_config.playerVoice);
			}
			if (!chosen.empty()) {
				if (const auto set = ByKeyLocked(chosen)) {
					return found(set, "your choice");
				}
			}
			const auto byTarget = [&](const RE::TESForm* a_form) -> std::shared_ptr<const VoiceSet> {
				if (!a_form) {
					return nullptr;
				}
				const auto it = g_byTarget.find(a_form->GetFormID());
				return it != g_byTarget.end() ? it->second : nullptr;
			};
			const auto npc = a_actor->GetNPC();
			if (const auto set = byTarget(npc)) {
				return found(set, "actor base");
			}
			const bool  female = Compat::GetSex(a_actor) == RE::SEX::kFemale;
			const RE::TESForm* voice = npc ? npc->voiceType : nullptr;
			if (!voice && a_actor->race) {
				voice = a_actor->race->defaultVoiceType[female ? 1 : 0];
			}
			if (const auto set = byTarget(voice)) {
				return found(set, "voice type");
			}
			if (const auto set = byTarget(a_actor->race)) {
				return found(set, "race");
			}
			if (!IsHuman(a_actor)) {
				REX::INFO("Sound: no voice set for {:08X} ({}): not human, so no default one", a_actor->GetFormID(), name ? name : "");
				return nullptr;
			}
			return female ? g_defaultFemale : g_defaultMale;
		}

		float RandomInterval()
		{
			const float lo = std::max(g_config.moanIntervalMin, 0.2F);
			const float hi = std::max(g_config.moanIntervalMax, lo);
			return std::uniform_real_distribution<float>(lo, hi)(g_random);
		}

		// Whether an entry fits now: its condition perk (OStim's) and its
		// actions (4Stim's).
		bool Fits(const SoundEntry& a_entry, RE::Actor* a_actor, const State& a_state)
		{
			if (!a_entry.actions.empty() &&
				std::ranges::none_of(a_entry.actions, [&](const std::string& a) { return std::ranges::find(a_state.actions, a) != a_state.actions.end(); })) {
				return false;
			}
			if (a_entry.condition) {
				const auto perk = RE::TESForm::GetFormByID<RE::BGSPerk>(a_entry.condition);
				const auto partner = a_state.partner ? RE::TESForm::GetFormByID<RE::Actor>(a_state.partner) : nullptr;
				if (perk && !perk->perkConditions.IsTrue(a_actor, partner ? partner : a_actor)) {
					return false;
				}
			}
			return true;
		}

		// Plays the first entry of a_set that fits (muffled ones when
		// muffled), as OStim's getSoundSet: packs list specific entries
		// first and a catch-all last. Randomness comes from inside an entry
		// (a folder of files, or the sound descriptor's own). Returns the
		// entry played, or null.
		const SoundEntry* PlayReaction(RE::Actor* a_actor, State& a_state, const ReactionSet& a_set, const SoundEntry** a_fits = nullptr)
		{
			const SoundEntry* first = nullptr;
			for (const auto& e : a_state.muffled ? a_set.soundMuffled : a_set.sound) {
				if (Fits(e, a_actor, a_state)) {
					first = &e;
					break;
				}
			}
			if (a_fits) {
				*a_fits = first;  // the entry that fits, played or not
			}
			if (!first || g_config.disabled || g_config.moanVolume <= 0.0F) {
				return nullptr;
			}
			const auto& entry = *first;
			a_state.sound = Play(entry.sound, a_actor, g_config.moanVolume);
			if (!a_state.sound.started) {
				return nullptr;
			}
			a_state.playing = true;
			return &entry;
		}


		// Stops what a_id is saying, and the face that goes with it (a moan's
		// expression lasts until its sound ends, so stopping the sound
		// without it held the face for the rest of the scene).
		void StopSound(std::uint32_t a_id, State& a_state)
		{
			if (a_state.playing) {
				a_state.sound.Stop();
				a_state.playing = false;
			}
			if (a_state.eventExpression) {
				a_state.eventExpression = false;
				Expressions::ClearEvent(a_id);
			}
		}

		// OStim's setDialogueCountdown: moans until the next line, by sex.
		void SetDialogueCountdown(State& a_state)
		{
			const int lo = a_state.female ? g_config.femaleDialogueCountdownMin : g_config.maleDialogueCountdownMin;
			const int hi = std::max(lo, a_state.female ? g_config.femaleDialogueCountdownMax : g_config.maleDialogueCountdownMax);
			a_state.dialogueCountdown = std::uniform_int_distribution<int>(lo, hi)(g_random) + 1;
		}

		RE::BGSVoiceType* VoiceTypeOf(RE::Actor* a_actor)
		{
			const auto npc = a_actor ? a_actor->GetNPC() : nullptr;
			return npc ? npc->voiceType : nullptr;
		}

		// OStim's canTalk.
		bool CanTalk(std::uint32_t a_id, const State& a_state, RE::Actor* a_actor)
		{
			if (a_state.mutedByHand || a_state.mutedByProperty || a_state.muffled || a_state.playing) {
				return false;
			}
			if (a_state.player && !g_config.playerDialogue) {
				return false;
			}
			if (a_actor->IsTalking()) {
				return false;
			}
			if (g_config.preventSameVoiceCrossTalk) {
				const auto voice = VoiceTypeOf(a_actor);
				for (const auto& [other, state] : g_states) {
					const auto actor = other != a_id && voice ? RE::TESForm::GetFormByID<RE::Actor>(other) : nullptr;
					if (actor && VoiceTypeOf(actor) == voice && actor->IsTalking()) {
						return false;
					}
				}
			}
			return true;
		}

		// OStim's playSound, the dialogue part: a line from a_set if the
		// countdown has come down to its threshold. True if one was said.
		bool SayLine(std::uint32_t a_id, State& a_state, RE::Actor* a_actor, const ReactionSet& a_set)
		{
			--a_state.dialogueCountdown;
			const SoundEntry* entry = nullptr;
			for (const auto& e : a_set.dialogue) {
				if (Fits(e, a_actor, a_state)) {
					entry = &e;
					break;
				}
			}
			if (!entry || a_state.dialogueCountdown > entry->threshold) {
				return false;
			}
			const auto vm = FourStim::GetVM();
			if (!vm) {
				return false;
			}
			if (!entry->expression.empty()) {
				a_state.eventExpression = Expressions::SetEvent(a_id, entry->expression, false);
			}
			// Fallout 4's ObjectReference.Say, to their primary partner (OStim's
			// SayTo console command).
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			Compat::DispatchStaticCall(vm, "FourStimScene"sv, "SayTopic"sv, callback, static_cast<std::int32_t>(a_id), static_cast<std::int32_t>(entry->topic),
				static_cast<std::int32_t>(a_state.partner ? a_state.partner : a_id), 0.0F);
			a_state.talking = true;
			// As OStim's: IsTalking takes a moment to come on (longer for the player).
			a_state.grace = a_state.player ? 1.5F : 0.25F;
			a_state.cooldown = entry->moanIntervalOverride > 0.0F ? entry->moanIntervalOverride : RandomInterval();
			SetDialogueCountdown(a_state);
			REX::INFO("Sound: {:08X} says topic {:08X}", a_id, entry->topic);
			return true;
		}

		// OStim's playSound: a line (where the scene lets them talk, or always
		// for a_ignoreChecks: climaxes and reactions), else a sound.
		void StartReaction(std::uint32_t a_id, State& a_state, const ReactionSet& a_set, bool a_ignoreChecks = true)
		{
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
			if (!actor) {
				return;
			}
			if ((a_ignoreChecks || a_state.talk) && !a_set.dialogue.empty() && CanTalk(a_id, a_state, actor) && SayLine(a_id, a_state, actor, a_set)) {
				return;
			}
			if (!a_ignoreChecks && !a_state.moan) {
				a_state.cooldown = RandomInterval();
				return;
			}
			const SoundEntry* fits = nullptr;
			const auto        entry = PlayReaction(actor, a_state, a_set, &fits);
			// As OStim's playSound: the face comes only from the sound set
			// that plays (its "expression"; legacy sets default to "climax" /
			// "moan"), so no set, no face. With 4Stim's bSounds=0 (OStim has
			// no such switch) the set that would play still gives its face,
			// for the expression's own duration.
			if (entry && !entry->expression.empty()) {
				a_state.eventExpression = Expressions::SetEvent(a_id, entry->expression, false);
			} else if (!entry && fits && g_config.disabled && !fits->expression.empty()) {
				Expressions::SetEvent(a_id, fits->expression, true);
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
		std::scoped_lock lock(g_voiceLock);
		LoadLocked();
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
		// A descriptor through Papyrus (no handle) counts as playing until
		// its estimate.
		return handle.soundID == static_cast<std::uint32_t>(-1) || handle.IsPlaying();
	}

	void Playing::Stop()
	{
		if (started && handle.soundID != static_cast<std::uint32_t>(-1)) {
			handle.FadeOutAndRelease(200);
		}
		started = false;
	}

	namespace
	{
		// A sound output model (SOPM) for the sound files, which have none of
		// their own: without one they played as 2D sounds, heard as loud
		// wherever the player was (an NPC scene across town followed you
		// around). The game's own models, picked once: one that fades with
		// distance, audible out to about 2000 units (closest to it), as
		// Fallout 4's voices are.
		const RE::BSISoundOutputModel* OutputModel()
		{
			static std::once_flag                 once;
			static const RE::BSISoundOutputModel* model = nullptr;
			std::call_once(once, []() {
				const auto handler = RE::TESDataHandler::GetSingleton();
				if (!handler) {
					return;
				}
				float          best = 0.0F;
				std::uint32_t  bestID = 0;
				for (const auto form : handler->GetFormArray<RE::BGSSoundOutput>()) {
					if (!form) {
						continue;
					}
					const RE::BSISoundOutputModel* candidate = form;
					if (!candidate->DoGetAttenuatesWithDistance()) {
						continue;
					}
					// How far it's still heard (in 100-unit steps, up to 10000).
					float reach = 0.0F;
					for (float d = 100.0F; d <= 10000.0F && candidate->DoGetAudibility(d); d += 100.0F) {
						reach = d;
					}
					if (reach < 800.0F) {
						continue;
					}
					if (!model || std::fabs(reach - 2000.0F) < std::fabs(best - 2000.0F)) {
						model = candidate;
						best = reach;
						bestID = form->GetFormID();
					}
				}
				if (model) {
					REX::INFO("Sound: files play through sound output model {:08X} (heard out to about {} units)", bestID, best);
				} else {
					REX::WARN("Sound: no sound output model that fades with distance; sound files play as 2D sounds");
				}
			});
			return model;
		}
	}

	namespace
	{
		// Volume, position and the actor to follow, then play: the same for
		// files and descriptors (OStim's GameSound::play: SetObjectToFollow,
		// SetVolume, Play; Fallout 4's handle has no SetVolume, so the volume
		// is an attenuation, as for files).
		void PlayHandle(Playing& a_playing, RE::NiAVObject* a_root, float a_volume, const RE::BSISoundOutputModel* a_model)
		{
			if (a_volume < 1.0F) {
				// Attenuation in hundredths of a decibel.
				const float db = -20.0F * std::log10(a_volume);
				a_playing.handle.SetStaticAttenuation(static_cast<std::uint16_t>(std::clamp(db * 100.0F, 0.0F, 10000.0F)));
			}
			if (a_model) {
				a_playing.handle.SetOutputModel(a_model);
			}
			if (Compat::CanSetSoundPosition()) {
				a_playing.handle.SetPosition(a_root->world.translate);
			}
			a_playing.handle.SetObjectToFollow(a_root);
			a_playing.handle.Play();
			a_playing.until = std::chrono::steady_clock::now() + std::chrono::seconds(10);  // safety net
			a_playing.started = true;
		}
	}

	Playing Play(const Sound& a_sound, RE::Actor* a_actor, float a_volume)
	{
		Playing playing;
		if (a_sound.Empty() || !a_actor || g_config.disabled || a_volume <= 0.0F) {
			return playing;
		}
		const auto audio = RE::BSAudioManager::GetSingleton();
		const auto root = a_actor->Get3D();
		if (!audio || !root) {
			return playing;
		}
		if (!a_sound.files.empty()) {
			const auto& file = a_sound.files[std::uniform_int_distribution<std::size_t>(0, a_sound.files.size() - 1)(g_random)];
			audio->GetSoundHandleByFile(playing.handle, Compat::ResourceID(file.c_str()), g_config.flags, g_config.priority);
			if (playing.handle.soundID == static_cast<std::uint32_t>(-1)) {
				REX::WARN("Sound: couldn't load {}", file);
				return playing;
			}
			// Positional, on the actor (OutputModel: else it's 2D).
			PlayHandle(playing, root, a_volume, OutputModel());
			return playing;
		}
		// A sound descriptor, played natively as OStim's GameSound::play
		// (BSAudioManager::GetSoundHandle): its volume, a real handle that
		// mute, climax and the scene's end stop, and its real length. The
		// descriptor's own output model, so none is set. OStim passes
		// Skyrim's usage flags 0x10; here the files' iSoundFlags.
		const auto form = RE::TESForm::GetFormByID<RE::BGSSoundDescriptorForm>(a_sound.descriptor);
		if (!form) {
			REX::WARN("Sound: {:08X} isn't a sound descriptor", a_sound.descriptor);
			return playing;
		}
		if (Compat::CanPlaySoundDescriptors()) {
			const RE::BSISoundDescriptor* descriptor = form;
			if (audio->GetSoundHandle(playing.handle, descriptor, 0.0F, g_config.flags) &&
				playing.handle.soundID != static_cast<std::uint32_t>(-1)) {
				PlayHandle(playing, root, a_volume, nullptr);
				return playing;
			}
			REX::WARN("Sound: couldn't play sound descriptor {:08X}", a_sound.descriptor);
			return playing;
		}
		// 4Stim: where GetSoundHandle isn't found (docs/OLDGEN.md), through
		// Papyrus (Sound.Play), which has no volume and can't be stopped; its
		// length isn't known here, so it counts as playing for 2 s.
		const auto vm = FourStim::GetVM();
		if (!vm) {
			return playing;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		Compat::DispatchStaticCall(vm, "FourStimScene"sv, "PlaySoundForm"sv, callback, static_cast<std::int32_t>(a_sound.descriptor),
			static_cast<std::int32_t>(a_actor->GetFormID()));
		playing.until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
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

	void Enter(std::uint32_t a_id, const SceneRegistry::Scene& a_scene, std::size_t a_role, const std::vector<std::uint32_t>& a_actors)
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
		state.talk = a_scene.actors[a_role].talk;
		if (added) {
			state.female = actor && Compat::GetSex(actor) == RE::SEX::kFemale;
			state.player = actor && actor == RE::PlayerCharacter::GetSingleton();
			SetDialogueCountdown(state);
		}
		state.muffled = a_scene.actors[a_role].muffled || state.muffledAlways;
		state.actions.clear();
		for (const auto& action : a_scene.actions) {
			if (action.actor == a_role || action.target == a_role || action.performer == a_role) {
				state.actions.push_back(action.type->id);
			}
			// "<id>:actor" / ":target" / ":performer": only on that side (as
			// OStim's default set's sucking sound, for the mouth's side of a
			// blowjob only: its actor faction).
			if (action.actor == a_role) {
				state.actions.push_back(action.type->id + ":actor");
			}
			if (action.target == a_role) {
				state.actions.push_back(action.type->id + ":target");
			}
			if (action.performer == a_role) {
				state.actions.push_back(action.type->id + ":performer");
			}
		}
		// OStim's Node::getPrimaryPartner: the other side of the first
		// action this role is on (themselves if none), the target of their
		// voice set's conditions.
		state.partner = 0;
		for (const auto& action : a_scene.actions) {
			std::size_t other = SceneRegistry::SceneAction::kNoRole;
			if (action.actor == a_role && action.target != a_role) {
				other = action.target;
			} else if (action.target == a_role && action.actor != a_role) {
				other = action.actor;
			} else {
				continue;
			}
			if (other < a_actors.size()) {
				state.partner = a_actors[other];
			}
			break;
		}
	}

	void Climax(std::uint32_t a_id, const std::vector<std::uint32_t>& a_actors, bool a_isPlayer)
	{
		// As OStim's reactToClimax: the others comment on it 0.75 s on (once
		// what they're saying has ended).
		for (const auto other : a_actors) {
			if (other == a_id) {
				continue;
			}
			const auto o = g_states.find(other);
			if (o != g_states.end() && o->second.voice && !o->second.voice->climaxCommentOther.Empty()) {
				if (!o->second.voice->climaxCommentOther.dialogue.empty()) {
					o->second.dialogueCountdown = 0;  // as OStim's: the comment's line is said
				}
				o->second.reaction = &o->second.voice->climaxCommentOther;
				o->second.reactionIn = 0.75F;
			}
		}
		const auto it = g_states.find(a_id);
		if (it == g_states.end()) {
			return;  // as OStim's: no voice set, no climax sound and no climax face
		}
		auto& state = it->second;
		state.heldForClimax = false;
		StopSound(a_id, state);  // nothing's playing by now (the climax waited); this clears the face
		state.reaction = nullptr;
		state.dialogueCountdown = 0;  // as OStim's playClimaxSound
		if (state.voice && !state.mutedByHand) {
			StartReaction(a_id, state, state.voice->climax);
		}  // as OStim's: muted, or no voice set, no climax face either
		// Their own comment, 1 s after the climax sound (OStim: not the
		// player's, whose comment would cut the climax off).
		if (!a_isPlayer && state.voice && !state.voice->climaxCommentSelf.Empty()) {
			if (!state.voice->climaxCommentSelf.dialogue.empty()) {
				state.dialogueCountdown = 0;
			}
			state.reaction = &state.voice->climaxCommentSelf;
			state.reactionIn = 1.0F;
		}
	}

	bool IsMakingSound(std::uint32_t a_id)
	{
		const auto it = g_states.find(a_id);
		return it != g_states.end() && it->second.playing && it->second.sound.IsPlaying();
	}

	void HoldForClimax(std::uint32_t a_id, bool a_hold)
	{
		if (const auto it = g_states.find(a_id); it != g_states.end()) {
			it->second.heldForClimax = a_hold;
		}
	}

	void Tick(float a_seconds)
	{
		for (auto& [id, state] : g_states) {
			// As OStim's loopSound: a grace period after something started,
			// a line being said, then what's playing, plays to its end first.
			if (state.grace > 0.0F) {
				state.grace -= a_seconds;
				continue;
			}
			if (state.talking) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				if (actor && actor->IsTalking()) {
					continue;
				}
				state.talking = false;
				if (state.eventExpression) {
					state.eventExpression = false;
					Expressions::ClearEvent(id);
				}
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
			if (state.heldForClimax) {
				continue;  // their climax goes next
			}
			// An event reaction (OStim's eventTimer): counted down once the
			// sound before it has ended, then it plays; no moan meanwhile.
			if (state.reaction) {
				if ((state.reactionIn -= a_seconds) <= 0.0F) {
					const auto reaction = state.reaction;
					state.reaction = nullptr;
					if (!state.mutedByHand) {
						StopSound(id, state);  // the moan's face, in case the reaction plays nothing
						StartReaction(id, state, *reaction);
					}
				}
				continue;
			}
			if ((!state.moan && !state.talk) || !state.voice || state.mutedByHand) {
				continue;
			}
			if ((state.cooldown -= a_seconds) <= 0.0F) {
				StartReaction(id, state, state.voice->moan, false);
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
		if (a_muted) {
			StopSound(a_id, it->second);
		}
		REX::INFO("Sound: {:08X} {}", a_id, a_muted ? "muted" : "unmuted");
	}

	bool IsMuted(std::uint32_t a_id)
	{
		const auto it = g_states.find(a_id);
		return it != g_states.end() && (it->second.mutedByHand || it->second.mutedByProperty);
	}

	std::vector<std::string> VoiceSetPairs()
	{
		std::scoped_lock lock(g_voiceLock);
		EnsureLoadedLocked();
		std::vector<std::string> pairs{ "0", Translations::Get("$ostim_generic_default", "Default") };
		for (const auto& set : g_listed) {
			pairs.push_back(set->key);
			pairs.push_back(set->name);
		}
		return pairs;
	}

	std::string ChosenVoiceSet(std::uint32_t a_base)
	{
		std::scoped_lock lock(g_choiceLock);
		const auto it = g_choices.find(a_base);
		return it != g_choices.end() ? it->second : std::string{};
	}

	std::string VoiceSetName(std::uint32_t a_base)
	{
		const auto chosen = ChosenVoiceSet(a_base);
		std::scoped_lock lock(g_voiceLock);
		EnsureLoadedLocked();
		const auto set = chosen.empty() ? nullptr : ByKeyLocked(chosen);
		return set ? set->name : Translations::Get("$ostim_generic_default", "Default");
	}

	void SetVoiceSet(std::uint32_t a_base, const std::string& a_key)
	{
		if (!a_base) {
			return;
		}
		{
			std::scoped_lock lock(g_choiceLock);
			if (a_key.empty() || a_key == "0") {
				g_choices.erase(a_base);
			} else {
				g_choices[a_base] = a_key;
			}
		}
		REX::INFO("Sound: voice set for actor base {:08X}: {}", a_base, a_key.empty() || a_key == "0" ? std::string("default") : a_key);
	}

	void ApplyVoiceSet(std::uint32_t a_base)
	{
		for (auto& [id, state] : g_states) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor || state.mutedByProperty || BaseOf(actor) != a_base) {
				continue;
			}
			StopSound(id, state);
			state.reaction = nullptr;
			state.voice = VoiceFor(actor);
		}
	}

	std::vector<std::pair<std::uint32_t, std::string>> AllVoiceChoices()
	{
		std::scoped_lock lock(g_choiceLock);
		return { g_choices.begin(), g_choices.end() };
	}

	void RestoreVoiceChoices(std::vector<std::pair<std::uint32_t, std::string>> a_choices)
	{
		std::scoped_lock lock(g_choiceLock);
		g_choices.clear();
		for (auto& [base, key] : a_choices) {
			g_choices[base] = std::move(key);
		}
	}

	void ClearVoiceChoices()
	{
		std::scoped_lock lock(g_choiceLock);
		g_choices.clear();
	}

	void Leave(const std::vector<std::uint32_t>& a_ids, bool a_postDialogue)
	{
		// OStim's post-scene dialogue: each NPC with a postSceneDialogue says
		// it to the player (in a scene with them) or their partner, 1-2 s
		// on; only the first of several with the same voice type; not when
		// muted, nor in a scene of one.
		if (a_postDialogue && a_ids.size() > 1) {
			const auto player = RE::PlayerCharacter::GetSingleton();
			const bool withPlayer = player && std::ranges::find(a_ids, player->GetFormID()) != a_ids.end();
			std::vector<RE::BGSVoiceType*> spoken;
			for (const auto id : a_ids) {
				const auto it = g_states.find(id);
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
				if (it == g_states.end() || !actor || actor == player || !it->second.voice || !it->second.voice->postSceneDialogue ||
					it->second.mutedByHand || it->second.mutedByProperty) {
					continue;
				}
				const auto voice = VoiceTypeOf(actor);
				if (std::ranges::find(spoken, voice) != spoken.end()) {
					continue;
				}
				spoken.push_back(voice);
				std::uint32_t target = withPlayer ? player->GetFormID() : it->second.partner;
				if (target == id || !target) {
					target = a_ids.front() != id ? a_ids.front() : a_ids.back();
				}
				if (const auto vm = FourStim::GetVM()) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
					Compat::DispatchStaticCall(vm, "FourStimScene"sv, "SayTopic"sv, callback, static_cast<std::int32_t>(id),
						static_cast<std::int32_t>(it->second.voice->postSceneDialogue), static_cast<std::int32_t>(target),
						std::uniform_real_distribution<float>(1.0F, 2.0F)(g_random));
				}
			}
		}
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
