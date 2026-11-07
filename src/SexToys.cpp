#include "SexToys.h"
#include "compat/Fallbacks.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "4Stim/PluginInterface.h"
#include "Actions.h"
#include "Excitement.h"

namespace SexToys
{
	namespace
	{
		// A registered toy (OStim's ToyWrapper): its calls run on detached
		// threads, so a slow device never holds up the game.
		struct Toy
		{
			FourStimAPI::SexToy* toy = nullptr;
			std::string          id;
			std::string          name;
			bool                 on = false;  // turned on for the scene running

			void TurnOn()
			{
				if (!on) {
					on = true;
					std::thread([t = toy]() { t->turnOn(); }).detach();
				}
			}
			void TurnOff()
			{
				if (on) {
					on = false;
					std::thread([t = toy]() { t->turnOff(); }).detach();
				}
			}
			void Update(float a_baseline, float a_peak, std::int16_t a_interval) const
			{
				std::thread([t = toy, a_baseline, a_peak, a_interval]() { t->update(a_baseline, a_peak, a_interval); }).detach();
			}
			void Peak(float a_magnitude, float a_duration) const
			{
				std::thread([t = toy, a_magnitude, a_duration]() { t->peak(a_magnitude, a_duration); }).detach();
			}
			void Stop() const
			{
				std::thread([t = toy]() { t->stop(); }).detach();
			}
		};

		// OStim's SlotSettings: what a toy does in a slot. Its
		// synchronization and scaling types have a fifth value, Individual
		// (4): the next level down decides.
		struct SlotSettings
		{
			bool  enabled = true;
			int   synchronization = 4;
			int   scaling = 4;
			float minScale = 0.0F;
			float maxScale = 1.0F;
			float minMagnitude = 0.1F;
			float maxMagnitude = 0.5F;
			bool  peaks = true;
			float minPeak = 0.6F;
			float maxPeak = 1.0F;
			bool  climax = true;
			float climaxMagnitude = 1.0F;
		};
		constexpr int  INDIVIDUAL = 4;
		constexpr auto ALL = "all";  // OStim's GLOBAL_KEY, for toys and slots

		// One toy in one slot (OStim's ToyHandler).
		struct Handler
		{
			SlotSettings  settings;  // OStim's getSlotSettings: the level whose scaling isn't Individual
			std::uint32_t actor = 0;
			std::string   slot;
			std::size_t   toy = 0;           // index into g_toys
			int           updateInterval = 0;  // ms
			int           cooldown = 0;        // ms
		};

		// The toys of one action (OStim's ToyGroup).
		struct Group
		{
			std::size_t          action = 0;
			bool                 hasPeaks = false;
			int                  sinceLastPeak = 0;  // ms
			std::size_t          nextPeak = 5;       // 5 = waiting for a first peak to time from
			std::array<int, 5>   intervals{};
			std::vector<Handler> toys;
		};

		Config                                     g_config;
		// OStim's per-toy and per-slot settings (toy id -> slot -> settings;
		// "all" for every toy or slot), from Toy Settings.json. "all" / "all"
		// is 4Stim.ini's [SexToys].
		constexpr auto TOY_SETTINGS_FILE = "Data/F4SE/Plugins/4Stim/Toy Settings.json";
		std::unordered_map<std::string, std::unordered_map<std::string, SlotSettings>> g_table;
		bool                                       g_tableLoaded = false;
		std::mutex                                 g_lock;
		std::vector<Toy>                           g_toys;
		std::vector<FourStimAPI::SexToyReloadListener*> g_listeners;
		bool                                       g_reloading = false;

		// The player's scene.
		std::vector<std::uint32_t>                  g_actors;
		std::shared_ptr<const SceneRegistry::Scene> g_scene;
		int                                         g_speed = 0;
		std::vector<Group>                          g_groups;

		bool IsMale(std::uint32_t a_id)
		{
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_id);
			return actor && Compat::GetSex(actor) == RE::SEX::kMale;
		}

		// 4Stim.ini's [SexToys]: OStim's "all" toy's "all" slot.
		SlotSettings GlobalSettings()
		{
			const auto& c = g_config;
			return { c.enabled, c.synchronization, c.scaling, c.minScale, c.maxScale, c.minMagnitude, c.maxMagnitude, c.peaks, c.minPeak, c.maxPeak, c.climax, c.climaxMagnitude };
		}

		// OStim's Settings::getToySettings(toy)->getSlotSettings(slot): its
		// defaults where nothing is set.
		SlotSettings At(const std::string& a_toy, const std::string& a_slot)
		{
			if (a_toy == ALL && a_slot == ALL) {
				return GlobalSettings();
			}
			if (const auto toy = g_table.find(a_toy); toy != g_table.end()) {
				if (const auto slot = toy->second.find(a_slot); slot != toy->second.end()) {
					return slot->second;
				}
			}
			return {};
		}

		// OStim's ToyThread::getSynchronizationType: the first of every toy
		// and slot, every toy in this slot, this toy in every slot, this toy
		// in this slot that isn't Individual.
		int SyncFor(const std::string& a_toy, const std::string& a_slot)
		{
			for (const auto& [toy, slot] : { std::pair{ std::string(ALL), std::string(ALL) }, std::pair{ std::string(ALL), a_slot }, std::pair{ a_toy, std::string(ALL) } }) {
				if (const int sync = At(toy, slot).synchronization; sync != INDIVIDUAL) {
					return sync;
				}
			}
			return At(a_toy, a_slot).synchronization;
		}

		// OStim's ToyHandler getSlotSettings: the same order, by scaling type.
		SlotSettings SettingsFor(const std::string& a_toy, const std::string& a_slot)
		{
			for (const auto& [toy, slot] : { std::pair{ std::string(ALL), std::string(ALL) }, std::pair{ std::string(ALL), a_slot }, std::pair{ a_toy, std::string(ALL) } }) {
				if (auto settings = At(toy, slot); settings.scaling != INDIVIDUAL) {
					return settings;
				}
			}
			return At(a_toy, a_slot);
		}

		// Toy Settings.json, in OStim's export layout: {"toySettings": {"<toy
		// id>" or "all": {"slotSettings": {"<slot>" or "all": {"enabled",
		// "synchronizationType", "scalingType", "minScale", "maxScale",
		// "minMagnitude", "maxMagnitude", "doPeaks", "minPeak", "maxPeak",
		// "doClimax", "climaxMagnitude"}}}}}, or that inside "toys".
		void LoadTable()
		{
			g_tableLoaded = true;
			g_table.clear();
			std::error_code ec;
			if (!std::filesystem::exists(TOY_SETTINGS_FILE, ec)) {
				return;
			}
			try {
				std::ifstream in(TOY_SETTINGS_FILE);
				auto          root = nlohmann::json::parse(in, nullptr, true, true);
				if (root.contains("toys")) {
					root = root["toys"];
				}
				const auto toys = root.find("toySettings");
				if (toys == root.end() || !toys->is_object()) {
					REX::WARN("Sex toys: {} has no \"toySettings\"", TOY_SETTINGS_FILE);
					return;
				}
				std::size_t count = 0;
				for (const auto& [toy, value] : toys->items()) {
					const auto slots = value.find("slotSettings");
					if (slots == value.end() || !slots->is_object()) {
						continue;
					}
					for (const auto& [slot, j] : slots->items()) {
						if (toy == ALL && slot == ALL) {
							REX::INFO("Sex toys: {}: \"all\" / \"all\" is 4Stim.ini's [SexToys], left out", TOY_SETTINGS_FILE);
							continue;
						}
						SlotSettings set;
						set.enabled = j.value("enabled", set.enabled);
						set.synchronization = std::clamp(j.value("synchronizationType", set.synchronization), 0, 4);
						set.scaling = std::clamp(j.value("scalingType", set.scaling), 0, 4);
						set.minScale = j.value("minScale", set.minScale);
						set.maxScale = std::max(j.value("maxScale", set.maxScale), 0.001F);
						set.minMagnitude = j.value("minMagnitude", set.minMagnitude);
						set.maxMagnitude = j.value("maxMagnitude", set.maxMagnitude);
						set.peaks = j.value("doPeaks", set.peaks);
						set.minPeak = j.value("minPeak", set.minPeak);
						set.maxPeak = j.value("maxPeak", set.maxPeak);
						set.climax = j.value("doClimax", set.climax);
						set.climaxMagnitude = j.value("climaxMagnitude", set.climaxMagnitude);
						g_table[toy][slot] = set;
						++count;
					}
				}
				REX::INFO("Sex toys: {} toy / slot setting(s) from {}", count, TOY_SETTINGS_FILE);
			} catch (const std::exception& ex) {
				REX::WARN("Sex toys: {} couldn't be read ({})", TOY_SETTINGS_FILE, ex.what());
			}
		}

		// OStim's SynchronizationType: does the actor in this slot get a toy?
		bool Synced(std::uint32_t a_actor, int a_sync)
		{
			switch (a_sync) {
			case 0:
				{
					const auto player = RE::PlayerCharacter::GetSingleton();
					return player && player->GetFormID() == a_actor;
				}
			case 1:
			case 2:
				{
					const bool male = a_sync == 1;
					for (const auto id : g_actors) {
						if (IsMale(id) == male) {
							return id == a_actor;  // the first of that sex only
						}
					}
					return false;
				}
			default:
				return true;
			}
		}

		// OStim's ToyHandler::update. Locked.
		void Update(Handler& a_handler, int a_peakInterval)
		{
			if (a_handler.cooldown > 0 || a_handler.toy >= g_toys.size()) {
				return;
			}
			a_handler.cooldown = a_handler.updateInterval;
			const auto& c = a_handler.settings;
			float       value = c.maxScale;
			switch (c.scaling) {
			case 1:
				value = Excitement::Rate(a_handler.actor);
				break;
			case 2:
				value = Excitement::Get(a_handler.actor) / 100.0F;  // OStim's excitement is 0-100 too; 4Stim scales it to the 0-1 defaults
				break;
			case 3:
				{
					const int count = g_scene ? static_cast<int>(g_scene->speeds.size()) : 1;
					value = count > 1 ? static_cast<float>(g_speed) / static_cast<float>(count - 1) : 0.0F;
				}
				break;
			default:
				break;
			}
			float baseline = 0.0F, peak = 0.0F;
			if (value <= c.minScale) {
				baseline = c.minMagnitude;
				peak = c.minPeak;
			} else if (value >= c.maxScale) {
				baseline = c.maxMagnitude;
				peak = c.maxPeak;
			} else {
				const float scale = (value - c.minScale) / (c.maxScale - c.minScale);
				baseline = c.minMagnitude + (c.maxMagnitude - c.minMagnitude) * scale;
				peak = c.minPeak + (c.maxPeak - c.minPeak) * scale;
			}
			// As OStim's: without peaks, no peak strength or interval.
			g_toys[a_handler.toy].Update(baseline, c.peaks ? peak : -1.0F, c.peaks ? static_cast<std::int16_t>(std::clamp(a_peakInterval, -1, 32767)) : std::int16_t{ -1 });
		}

		void StopGroups()
		{
			for (const auto& group : g_groups) {
				for (const auto& handler : group.toys) {
					if (handler.toy < g_toys.size()) {
						g_toys[handler.toy].Stop();
					}
				}
			}
			g_groups.clear();
		}

		// OStim's ToyThread::nodeChanged: the toys given to the scene's slots,
		// one slot per toy, in the order of the scene's actions. Locked.
		void Build()
		{
			StopGroups();
			if (!g_config.enabled || !g_scene || g_reloading || g_toys.empty()) {
				return;
			}
			if (!g_tableLoaded) {
				LoadTable();
			}
			std::vector<bool> used(g_toys.size(), false);
			std::size_t       inUse = 0;
			// As OStim's: a toy turned off in every slot isn't used at all.
			for (std::size_t t = 0; t < g_toys.size(); ++t) {
				if (!At(g_toys[t].id, ALL).enabled) {
					used[t] = true;
					++inUse;
				}
			}
			bool              anySlot = false;
			for (std::size_t a = 0; a < g_scene->actions.size() && inUse < g_toys.size(); ++a) {
				const auto& action = g_scene->actions[a];
				Group       group;
				group.action = a;
				group.hasPeaks = action.doPeaks && (action.peaksAnnotated || action.type->peak.kind != Actions::Peak::Kind::kNone);
				const std::array<std::pair<const Actions::Side*, std::size_t>, 3> sides{ { { &action.type->actor, action.actor },
					{ &action.type->target, action.target }, { &action.type->performer, action.performer } } };
				for (const auto& [side, role] : sides) {
					if (role >= g_actors.size()) {
						continue;
					}
					const auto actor = g_actors[role];
					anySlot = anySlot || !side->toySlots.empty();
					for (const auto& slot : side->toySlots) {
						for (std::size_t t = 0; t < g_toys.size(); ++t) {
							if (used[t]) {
								continue;
							}
							const auto& id = g_toys[t].id;
							if (!At(id, slot).enabled || !At(ALL, slot).enabled || !Synced(actor, SyncFor(id, slot))) {
								continue;
							}
							used[t] = true;
							++inUse;
							Handler handler;
							handler.actor = actor;
							handler.slot = slot;
							handler.toy = t;
							handler.updateInterval = g_toys[t].toy->getUpdateInterval();
							handler.settings = SettingsFor(id, slot);
							group.toys.push_back(std::move(handler));
							g_toys[t].TurnOn();
						}
					}
				}
				if (!group.toys.empty()) {
					for (auto& handler : group.toys) {
						Update(handler, -1);
					}
					REX::INFO("Sex toys: \"{}\" action {} ({}): {} toy(s)", g_scene->id, a, action.type->id, group.toys.size());
					g_groups.push_back(std::move(group));
				}
			}
			if (g_groups.empty()) {
				REX::INFO("Sex toys: \"{}\": {} for the {} toy(s)", g_scene->id,
					anySlot ? "no slot whose actor sToySynchronization allows" : "its actions have no toy slots (\"toySlot\")", g_toys.size());
			}
		}

		void TurnAllOff()
		{
			for (auto& toy : g_toys) {
				toy.TurnOff();
			}
		}

		class InterfaceImpl : public FourStimAPI::SexToyInterface
		{
		public:
			std::uint32_t getVersion() override { return 1; }

			void registerReloadListener(FourStimAPI::SexToyReloadListener* a_listener) override
			{
				if (a_listener) {
					std::scoped_lock lock(g_lock);
					g_listeners.push_back(a_listener);
				}
			}

			void addToy(FourStimAPI::SexToy* a_toy) override
			{
				if (!a_toy) {
					return;
				}
				std::scoped_lock lock(g_lock);
				Toy toy;
				toy.toy = a_toy;
				toy.id = a_toy->getID() ? a_toy->getID() : "";
				toy.name = a_toy->getName() ? a_toy->getName() : toy.id;
				REX::INFO("Sex toys: registered \"{}\" ({})", toy.name, toy.id);
				g_toys.push_back(std::move(toy));
			}
		};
	}

	Config& Settings()
	{
		return g_config;
	}

	namespace
	{
		// a_value's index in a_names (case-insensitive), else a_default.
		int Pick(const std::string& a_value, std::initializer_list<const char*> a_names, int a_default)
		{
			int i = 0;
			for (const auto name : a_names) {
				if (_stricmp(a_value.c_str(), name) == 0) {
					return i;
				}
				++i;
			}
			return a_default;
		}
	}

	bool ReadSetting(const std::string& a_key, const std::string& a_value)
	{
		auto& c = g_config;
		auto  unit = [&]() { return std::clamp(std::stof(a_value), 0.0F, 1.0F); };
		if (a_key == "btoysenabled") {
			c.enabled = std::stoi(a_value) != 0;
		} else if (a_key == "stoysynchronization") {
			c.synchronization = Pick(a_value, { "player", "male", "female", "scene", "individual" }, 3);
		} else if (a_key == "stoyscaling") {
			c.scaling = Pick(a_value, { "none", "rate", "value", "speed", "individual" }, 0);
		} else if (a_key == "ftoyminscale") {
			c.minScale = std::max(std::stof(a_value), 0.0F);
		} else if (a_key == "ftoymaxscale") {
			c.maxScale = std::max(std::stof(a_value), 0.001F);
		} else if (a_key == "ftoyminmagnitude") {
			c.minMagnitude = unit();
		} else if (a_key == "ftoymaxmagnitude") {
			c.maxMagnitude = unit();
		} else if (a_key == "btoypeaks") {
			c.peaks = std::stoi(a_value) != 0;
		} else if (a_key == "ftoyminpeak") {
			c.minPeak = unit();
		} else if (a_key == "ftoymaxpeak") {
			c.maxPeak = unit();
		} else if (a_key == "btoyclimax") {
			c.climax = std::stoi(a_value) != 0;
		} else if (a_key == "ftoyclimaxmagnitude") {
			c.climaxMagnitude = unit();
		} else {
			return false;
		}
		return true;
	}

	FourStimAPI::SexToyInterface* Interface()
	{
		static InterfaceImpl impl;
		return &impl;
	}

	void Reload()
	{
		std::vector<FourStimAPI::SexToyReloadListener*> listeners;
		{
			std::scoped_lock lock(g_lock);
			if (g_reloading || g_listeners.empty()) {
				return;
			}
			g_tableLoaded = false;  // Toy Settings.json read again
			StopGroups();
			TurnAllOff();
			g_reloading = true;
			g_toys.clear();  // as OStim: the plugins own their toys
			listeners = g_listeners;
		}
		std::thread([listeners]() {
			for (const auto listener : listeners) {
				listener->reloadToys();
			}
			{
				std::scoped_lock lock(g_lock);
				g_reloading = false;
			}
			// The player's scene (if one runs) gets the new toys.
			F4SE::GetTaskInterface()->AddTask([]() {
				std::scoped_lock lock(g_lock);
				if (g_scene) {
					Build();
				}
			});
		}).detach();
	}

	void Enter(const std::vector<std::uint32_t>& a_actors, const std::shared_ptr<const SceneRegistry::Scene>& a_scene, int a_speed)
	{
		bool starting = false;
		{
			std::scoped_lock lock(g_lock);
			starting = g_actors.empty();
			const bool sameScene = !starting && g_scene && a_scene && g_scene->id == a_scene->id && g_actors == a_actors;
			g_actors = a_actors;
			g_speed = a_speed;
			if (sameScene) {
				// OStim's ToyGroup::speedChanged: peak timing starts over.
				for (auto& group : g_groups) {
					group.nextPeak = group.intervals.size();
					group.intervals.fill(0);
				}
				return;
			}
			g_scene = a_scene;
			if (!starting) {
				Build();
			}
		}
		if (starting) {
			Reload();  // OStim: the toys are reloaded each time the player's thread starts (Build follows)
			std::scoped_lock lock(g_lock);
			if (!g_reloading) {
				Build();  // no device plugin listening: the toys there are (none, usually)
			}
		}
	}

	void Peak(const std::vector<std::uint32_t>& a_actors, std::size_t a_action)
	{
		std::scoped_lock lock(g_lock);
		if (g_groups.empty() || a_actors != g_actors) {
			return;
		}
		for (auto& group : g_groups) {
			if (group.action != a_action) {
				continue;
			}
			// OStim's ToyGroup::peak: the average of the last five intervals.
			if (group.nextPeak >= group.intervals.size()) {
				group.nextPeak = 0;
				group.sinceLastPeak = 0;
				continue;
			}
			group.intervals[group.nextPeak] = group.sinceLastPeak;
			group.nextPeak = (group.nextPeak + 1) % group.intervals.size();
			group.sinceLastPeak = 0;
			int sum = 0, count = 0;
			for (const int interval : group.intervals) {
				if (interval == 0) {
					break;
				}
				sum += interval;
				++count;
			}
			const int interval = count > 0 ? sum / count : -1;
			for (auto& handler : group.toys) {
				Update(handler, interval);
			}
		}
	}

	void Climax(std::uint32_t a_actor)
	{
		std::scoped_lock lock(g_lock);
		for (auto& group : g_groups) {
			for (auto& handler : group.toys) {
				if (handler.actor == a_actor && handler.toy < g_toys.size() && handler.settings.climax) {
					handler.cooldown = 1000;
					g_toys[handler.toy].Peak(handler.settings.climaxMagnitude, 1.0F);
				}
			}
		}
	}

	void Tick(float a_seconds)
	{
		std::scoped_lock lock(g_lock);
		if (g_groups.empty()) {
			return;
		}
		const int ms = static_cast<int>(a_seconds * 1000.0F);
		for (auto& group : g_groups) {
			group.sinceLastPeak = std::min(group.sinceLastPeak + ms, 60000);
			for (auto& handler : group.toys) {
				handler.cooldown -= ms;
			}
			// OStim's ToyGroup::loop: without peaks, an update every second.
			if (!group.hasPeaks && group.sinceLastPeak > 1000) {
				for (auto& handler : group.toys) {
					Update(handler, -1);
				}
				group.sinceLastPeak = 0;
			}
		}
	}

	void Leave(const std::vector<std::uint32_t>& a_actors)
	{
		std::scoped_lock lock(g_lock);
		if (g_actors.empty() || std::ranges::none_of(a_actors, [](std::uint32_t id) { return std::ranges::find(g_actors, id) != g_actors.end(); })) {
			return;
		}
		StopGroups();
		TurnAllOff();
		g_actors.clear();
		g_scene.reset();
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		StopGroups();
		TurnAllOff();
		g_actors.clear();
		g_scene.reset();
	}
}
