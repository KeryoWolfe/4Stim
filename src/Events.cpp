#include "Events.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Excitement.h"
#include "Sounds.h"

namespace Events
{
	namespace
	{
		constexpr auto EVENT_FOLDER = "Data/F4SE/Plugins/4Stim/Events";  // OStim: SKSE/Plugins/OStim/events

		// One of an event's actor, target and performer (OStim's EventActor).
		// Unset values come from the supertype, as OStim's getters do.
		struct EventRole
		{
			std::optional<float> stimulation;
			std::optional<float> maxStimulation;
			std::optional<float> reactionDelay;  // seconds (OStim reads seconds, keeps ms)
		};

		struct Event
		{
			std::string          id;
			std::string          supertype;  // lowercase id, or empty
			EventRole            roles[3];   // actor, target, performer
			Sounds::Sound        sound;
			float                shakeStrength = 0.0F;
			float                shakeDuration = 0.0F;
			float                rumbleStrength = 0.0F;
			float                rumbleDuration = 0.0F;
			std::vector<std::string> tags;
		};

		constexpr const char* ROLE_KEYS[3] = { "actor", "target", "performer" };

		std::mutex                             g_lock;
		bool                                   g_loaded = false;
		std::unordered_map<std::string, Event> g_events;

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		std::optional<float> Number(const nlohmann::json& a_object, const char* a_key)
		{
			const auto it = a_object.find(a_key);
			return it != a_object.end() && it->is_number() ? std::optional<float>(it->get<float>()) : std::nullopt;
		}

		void LoadLocked()
		{
			g_loaded = true;
			g_events.clear();
			std::error_code ec;
			for (const auto& e : std::filesystem::recursive_directory_iterator(EVENT_FOLDER, ec)) {
				if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".json") {
					continue;
				}
				try {
					std::ifstream in(e.path());
					const auto    root = nlohmann::json::parse(in, nullptr, true, true);
					if (!root.is_object()) {
						continue;
					}
					Event event;
					event.id = Lower(e.path().stem().string());
					event.supertype = Lower(root.value("supertype", std::string{}));
					for (int r = 0; r < 3; ++r) {
						if (const auto it = root.find(ROLE_KEYS[r]); it != root.end() && it->is_object()) {
							event.roles[r].stimulation = Number(*it, "stimulation");
							event.roles[r].maxStimulation = Number(*it, "maxStimulation");
							event.roles[r].reactionDelay = Number(*it, "reactionDelay");
						}
					}
					// OStim's "sound" is a descriptor form; 4Stim also takes
					// "file" (a sound file or folder under Data\Sound\).
					event.sound = Sounds::ReadSound(root);
					event.shakeStrength = std::clamp(Number(root, "cameraShakeStrength").value_or(0.0F), 0.0F, 1.0F);
					event.shakeDuration = std::max(Number(root, "cameraShakeDuration").value_or(0.0F), 0.0F);
					event.rumbleStrength = std::clamp(Number(root, "controllerRumbleStrength").value_or(0.0F), 0.0F, 1.0F);
					event.rumbleDuration = std::max(Number(root, "controllerRumbleDuration").value_or(0.0F), 0.0F);
					if (const auto tags = root.find("tags"); tags != root.end() && tags->is_array()) {
						for (const auto& t : *tags) {
							if (t.is_string()) {
								event.tags.push_back(Lower(t.get<std::string>()));
							}
						}
					}
					const auto id = event.id;
					g_events[id] = std::move(event);
				} catch (const std::exception& ex) {
					REX::WARN("Events: {}: couldn't be read ({})", e.path().filename().string(), ex.what());
				}
			}
			for (const auto& [id, event] : g_events) {
				if (!event.supertype.empty() && !g_events.contains(event.supertype)) {
					REX::WARN("Events: supertype \"{}\" of event \"{}\" doesn't exist", event.supertype, id);
				}
			}
			REX::INFO("Events: {} event(s) from {}", g_events.size(), EVENT_FOLDER);
		}

		// a_event and its supertypes, nearest first (a loop stops it).
		std::vector<const Event*> Chain(const Event* a_event)
		{
			std::vector<const Event*> chain;
			while (a_event && std::ranges::find(chain, a_event) == chain.end()) {
				chain.push_back(a_event);
				if (a_event->supertype.empty()) {
					break;
				}
				const auto it = g_events.find(a_event->supertype);
				a_event = it != g_events.end() ? &it->second : nullptr;
			}
			return chain;
		}

		// The first value in the chain that's set (OStim: not 0).
		float First(const std::vector<const Event*>& a_chain, float Event::*a_field)
		{
			for (const auto e : a_chain) {
				if (e->*a_field != 0.0F) {
					return e->*a_field;
				}
			}
			return 0.0F;
		}

		std::optional<float> RoleValue(const std::vector<const Event*>& a_chain, int a_role, std::optional<float> EventRole::*a_field)
		{
			for (const auto e : a_chain) {
				if (const auto& v = e->roles[a_role].*a_field) {
					return v;
				}
			}
			return std::nullopt;
		}
	}

	void Reload()
	{
		std::scoped_lock lock(g_lock);
		LoadLocked();
	}

	std::vector<std::string> List()
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		std::vector<std::string> ids;
		for (const auto& [id, event] : g_events) {
			ids.push_back(id);
		}
		std::ranges::sort(ids);
		return ids;
	}

	Effects Fire(const std::string& a_event, const std::vector<std::uint32_t>& a_actors, int a_actor, int a_target, int a_performer)
	{
		std::unique_lock lock(g_lock);
		if (!g_loaded) {
			LoadLocked();
		}
		Effects out;
		const auto it = g_events.find(Lower(a_event));
		if (it == g_events.end()) {
			REX::WARN("Events: no event \"{}\" (not in {})", a_event, EVENT_FOLDER);
			return out;
		}
		const auto chain = Chain(&it->second);
		out.found = true;
		out.shakeStrength = First(chain, &Event::shakeStrength);
		out.shakeDuration = First(chain, &Event::shakeDuration);
		out.rumbleStrength = First(chain, &Event::rumbleStrength);
		out.rumbleDuration = First(chain, &Event::rumbleDuration);
		Sounds::Sound sound;
		for (const auto e : chain) {
			if (!e->sound.Empty()) {
				sound = e->sound;
				break;
			}
		}
		const std::string        id = it->first;
		std::vector<std::string> ids;
		for (const auto e : chain) {
			ids.push_back(e->id);
		}
		struct Who
		{
			std::uint32_t id = 0;
			float         stimulation = 0.0F;
			float         maxStimulation = 100.0F;
			float         reactionDelay = 0.0F;
		};
		Who roles[3];
		const int indices[3] = { a_actor, a_target, a_performer };
		for (int r = 0; r < 3; ++r) {
			if (indices[r] >= 0 && static_cast<std::size_t>(indices[r]) < a_actors.size()) {
				roles[r].id = a_actors[static_cast<std::size_t>(indices[r])];
			}
			roles[r].stimulation = RoleValue(chain, r, &EventRole::stimulation).value_or(0.0F);
			roles[r].maxStimulation = RoleValue(chain, r, &EventRole::maxStimulation).value_or(100.0F);
			roles[r].reactionDelay = RoleValue(chain, r, &EventRole::reactionDelay).value_or(0.0F);
		}
		lock.unlock();

		REX::INFO("Events: \"{}\" (actor {:08X}, target {:08X}, performer {:08X})", id, roles[0].id, roles[1].id, roles[2].id);

		// As OStim's Thread::callEvent.
		if (!sound.Empty() && roles[0].id) {
			Sounds::Play(sound, RE::TESForm::GetFormByID<RE::Actor>(roles[0].id), Sounds::Settings().soundVolume);
		}
		for (const auto& who : roles) {
			if (who.id && who.stimulation > 0.0F) {
				const float now = Excitement::Get(who.id);
				if (now >= 0.0F && now < who.maxStimulation) {
					Excitement::Add(who.id, who.stimulation, true);
				}
			}
		}
		if (roles[0].id && roles[1].id) {
			Sounds::ReactToEvent(roles[0].id, ids, "actor", roles[0].reactionDelay);
			Sounds::ReactToEvent(roles[1].id, ids, "target", roles[1].reactionDelay);
		}
		if (roles[2].id && (roles[2].id == roles[0].id ? roles[1].id : roles[0].id)) {
			Sounds::ReactToEvent(roles[2].id, ids, "performer", roles[2].reactionDelay);
		}
		return out;
	}
}
