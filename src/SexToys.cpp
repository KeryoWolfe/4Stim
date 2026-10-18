#include "SexToys.h"

#include <array>
#include <mutex>
#include <thread>

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

		// One toy in one slot (OStim's ToyHandler).
		struct Handler
		{
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
			return actor && actor->GetSex() == RE::SEX::kMale;
		}

		// OStim's SynchronizationType: does the actor in this slot get a toy?
		bool Synced(std::uint32_t a_actor)
		{
			switch (g_config.synchronization) {
			case 0:
				{
					const auto player = RE::PlayerCharacter::GetSingleton();
					return player && player->GetFormID() == a_actor;
				}
			case 1:
			case 2:
				{
					const bool male = g_config.synchronization == 1;
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
			const auto& c = g_config;
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
			g_toys[a_handler.toy].Update(baseline, peak, c.peaks ? static_cast<std::int16_t>(std::clamp(a_peakInterval, -1, 32767)) : std::int16_t{ -1 });
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
			std::vector<bool> used(g_toys.size(), false);
			std::size_t       inUse = 0;
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
							if (used[t] || !Synced(actor)) {
								continue;
							}
							used[t] = true;
							++inUse;
							Handler handler;
							handler.actor = actor;
							handler.slot = slot;
							handler.toy = t;
							handler.updateInterval = g_toys[t].toy->getUpdateInterval();
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
			c.synchronization = Pick(a_value, { "player", "male", "female", "scene" }, 3);
		} else if (a_key == "stoyscaling") {
			c.scaling = Pick(a_value, { "none", "rate", "value", "speed" }, 0);
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
		if (!g_config.climax) {
			return;
		}
		for (auto& group : g_groups) {
			for (auto& handler : group.toys) {
				if (handler.actor == a_actor && handler.toy < g_toys.size()) {
					handler.cooldown = 1000;
					g_toys[handler.toy].Peak(g_config.climaxMagnitude, 1.0F);
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
