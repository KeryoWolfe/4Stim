#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <fstream>
#include <unordered_map>

#include "Bridge.h"
#include "HUD.h"
#include "SceneEvents.h"
#include "Furniture.h"
#include "SceneRegistry.h"

namespace
{
	using namespace std::literals;

	// Papyrus-facing script name. Must be "FourStim", not "4Stim" -- Papyrus
	// identifiers can't start with a digit. The plugin/project itself is
	// still called 4Stim everywhere else (xmake project name, log text).
	// Must match the .psc script name exactly.
	constexpr auto SCRIPT_NAME = "FourStim"sv;

	constexpr float PI_F = 3.14159265358979323846f;

	// Stashed at Papyrus registration time so we can dispatch calls INTO
	// Papyrus-native engine functions (e.g. Actor.SetRestrained) later,
	// not just receive calls Papyrus makes to us.
	RE::BSScript::IVirtualMachine* g_vm = nullptr;

	// ---- Settings (Data\F4SE\Plugins\4Stim.ini) ----

	struct Settings
	{
		std::uint32_t hotkey = 0x4E;           // Windows virtual-key code; 0x4E = N
		std::int32_t  targetMode = 0;          // 0 = Crosshair, 1 = Proximity
		float         maxDistance = 300.0f;    // crosshair: how far to look
		float         crosshairCone = 10.0f;   // crosshair: degrees either side of your facing
		float         proximityRadius = 200.0f;
		float         freeCameraSpeed = 0.5f;  // multiplier on the game's free camera speed during scenes
		std::uint32_t speedUpKey = 0xBB;       // virtual-key code; 0xBB = the =/+ key
		std::uint32_t speedDownKey = 0xBD;     // 0xBD = the -/_ key
		std::string   hudTheme = "Color";     // file name in Data\Interface\4Stim\Themes\, without .json
		bool          hudEnabled = true;
		bool          logAnimEvents = false;  // log scene actors' animation events (for authors)
		float         transitionLead = 0.0F;   // seconds before a transition's length to move on (see 4Stim.ini)
		bool          matchSex = true;         // only offer scenes whose roles' sexes the actors fit
		float         furnitureRadius = 500.0F;  // how far from the player to look for furniture
		float         furnitureHeight = 100.0F;  // and how far up or down
		bool          logFurniture = false;      // log every candidate object when looking
	};
	Settings g_settings;

	void LoadSettings()
	{
		std::ifstream in("Data/F4SE/Plugins/4Stim.ini");
		if (!in) {
			REX::INFO("Settings: no 4Stim.ini, using defaults");
			return;
		}
		auto trim = [](std::string s) {
			const auto b = s.find_first_not_of(" \t\r");
			const auto e = s.find_last_not_of(" \t\r");
			return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
		};
		std::string line;
		while (std::getline(in, line)) {
			line = trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto key = trim(line.substr(0, eq));
			auto value = trim(line.substr(eq + 1));
			if (const auto c = value.find(';'); c != std::string::npos) {
				value = trim(value.substr(0, c));
			}
			std::ranges::transform(key, key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
			try {
				if (key == "ihotkey") {
					g_settings.hotkey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "stargetmode") {
					auto v = value;
					std::ranges::transform(v, v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
					g_settings.targetMode = (v == "proximity") ? 1 : 0;
				} else if (key == "fmaxdistance") {
					g_settings.maxDistance = std::stof(value);
				} else if (key == "fcrosshaircone") {
					g_settings.crosshairCone = std::stof(value);
				} else if (key == "fproximityradius") {
					g_settings.proximityRadius = std::stof(value);
				} else if (key == "ffreecameraspeed") {
					g_settings.freeCameraSpeed = std::stof(value);
				} else if (key == "ispeedupkey") {
					g_settings.speedUpKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "ispeeddownkey") {
					g_settings.speedDownKey = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
				} else if (key == "stheme") {
					g_settings.hudTheme = value;
				} else if (key == "benablehud") {
					g_settings.hudEnabled = std::stoi(value) != 0;
				} else if (key == "ffurnitureradius") {
					g_settings.furnitureRadius = std::stof(value);
				} else if (key == "ffurnitureheight") {
					g_settings.furnitureHeight = std::stof(value);
				} else if (key == "blogfurniture") {
					g_settings.logFurniture = std::stoi(value) != 0;
				} else if (key == "bmatchsex") {
					g_settings.matchSex = std::stoi(value) != 0;
				} else if (key == "bloganimevents") {
					g_settings.logAnimEvents = std::stoi(value) != 0;
				} else if (key == "ftransitionlead") {
					g_settings.transitionLead = std::clamp(std::stof(value), -2.0F, 5.0F);
				}
			} catch (...) {
				REX::WARN("Settings: couldn't read \"{}\" for {}", value, key);
			}
		}
		REX::INFO("Settings: hotkey 0x{:X}, target mode {}, max distance {}, cone {}, radius {}, free camera speed x{}, speed keys 0x{:X}/0x{:X}",
			g_settings.hotkey, g_settings.targetMode == 1 ? "Proximity" : "Crosshair",
			g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius, g_settings.freeCameraSpeed,
			g_settings.speedUpKey, g_settings.speedDownKey);
		REX::INFO("Settings: HUD {}, theme \"{}\", transition lead {}s, match sex {}", g_settings.hudEnabled ? "on" : "off", g_settings.hudTheme, g_settings.transitionLead, g_settings.matchSex ? "on" : "off");
		REX::INFO("Settings: furniture within {} (height {}), log {}", g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.logFurniture ? "on" : "off");
	}


	// ---- The focused scene ----
	// The scene the player is in, or an NPC scene the player started to watch
	// with the free camera (see FourStim::FocusedScene). Lets the hotkey open
	// the HUD instead of starting a new scene, lets speed/navigation replay the
	// right idles, and is what the HUD shows. Set when the scene's animation
	// starts, cleared when it stops. Not save-persisted yet (that's the claim
	// registry's job). Still called "player scene" in names Papyrus sees.

	using PlayerScene = FourStim::FocusedScene;

	std::mutex  g_playerSceneLock;
	PlayerScene g_playerScene;

	// Set by SaveStartView (the first step of a scene the player watches with
	// the free camera): the next scene to start becomes the focused scene even
	// without the player in it.
	std::atomic<bool> g_focusNextScene = false;

	PlayerScene GetPlayerScene()
	{
		std::scoped_lock lock(g_playerSceneLock);
		return g_playerScene;
	}

	void SetPlayerScene(PlayerScene a_scene)
	{
		{
			std::scoped_lock lock(g_playerSceneLock);
			g_playerScene = std::move(a_scene);
		}
		HUD::OnFocusedSceneChanged();
	}

	// Clears the focused scene if a_actorID is in it.
	void ClearFocusIfIn(std::uint32_t a_actorID)
	{
		const auto focused = GetPlayerScene();
		if (focused.Active() && (focused.role0 == a_actorID || focused.role1 == a_actorID)) {
			SetPlayerScene({});
		}
	}

	// ---- Sexes ----
	// Scenes say who may take each role ("sex" on each actor, SCENES.md);
	// with bMatchSex on, only scenes the actors fit are offered or played.

	std::vector<SceneRegistry::Sex> SexesOf(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<SceneRegistry::Sex> sexes;
		for (const auto id : a_ids) {
			sexes.push_back(SceneRegistry::SexOf(RE::TESForm::GetFormByID<RE::Actor>(id)));
		}
		return sexes;
	}

	// The filter for the picker's lists: these actors, roles open (a new
	// scene) or already given (a running one).
	SceneRegistry::ListFilter SexFilter(const std::vector<std::uint32_t>& a_ids, bool a_fixedOrder)
	{
		SceneRegistry::ListFilter filter;
		filter.fixedOrder = a_fixedOrder;
		if (g_settings.matchSex) {
			filter.sexes = SexesOf(a_ids);
		}
		return filter;
	}

	// Whether the actors of a running scene (role order) may play a_scene.
	bool SexesFit(const SceneRegistry::Scene& a_scene, const std::vector<std::uint32_t>& a_ids)
	{
		return !g_settings.matchSex || SceneRegistry::Fits(a_scene.actors, SexesOf(a_ids));
	}

	// ---- Furniture ----
	// A scene with "furniture" plays on a piece of furniture of that type (or
	// a subtype); one without plays anywhere but on furniture.

	bool FurnitureFits(const std::string& a_sceneFurniture, const std::string& a_here)
	{
		if (a_sceneFurniture.empty()) {
			return a_here.empty();
		}
		return !a_here.empty() && Furniture::IsA(a_here, a_sceneFurniture);
	}

	// Furniture near the player when the picker opened (new scenes only).
	std::mutex                      g_pickerFurnitureLock;
	std::vector<Furniture::Found>   g_pickerFurniture;

	std::vector<Furniture::Found> PickerFurniture()
	{
		std::scoped_lock lock(g_pickerFurnitureLock);
		return g_pickerFurniture;
	}

	// The nearest found piece a scene for a_type can be played on.
	const Furniture::Found* FurnitureFor(const std::vector<Furniture::Found>& a_found, const std::string& a_type)
	{
		for (const auto& f : a_found) {  // nearest first
			if (Furniture::IsA(f.type, a_type)) {
				return &f;
			}
		}
		return nullptr;
	}

	// The furniture the next scene started for these actors goes on (set
	// when the picker starts a furniture scene).
	struct PendingFurniture
	{
		std::uint32_t              ref = 0;
		std::string                type;
		std::vector<std::uint32_t> actors;  // sorted
		std::array<float, 4>       offset{};  // the scene's own offset from the spot
	};
	std::mutex                      g_pendingFurnitureLock;
	std::optional<PendingFurniture> g_pendingFurniture;

	std::optional<PendingFurniture> PendingFurnitureFor(std::vector<std::uint32_t> a_actors, bool a_take)
	{
		std::ranges::sort(a_actors);
		std::scoped_lock lock(g_pendingFurnitureLock);
		if (!g_pendingFurniture || g_pendingFurniture->actors != a_actors) {
			return std::nullopt;
		}
		auto out = g_pendingFurniture;
		if (a_take) {
			g_pendingFurniture.reset();
		}
		return out;
	}

	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot);

	// Moves a_actors onto the pending furniture's spot, if the next scene for
	// them has one. False if it doesn't.
	bool PlaceOnPendingFurniture(const std::vector<RE::Actor*>& a_actors)
	{
		std::vector<std::uint32_t> ids;
		for (const auto actor : a_actors) {
			ids.push_back(actor->GetFormID());
		}
		const auto pending = PendingFurnitureFor(ids, false);
		if (!pending) {
			return false;
		}
		const auto ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(pending->ref);
		if (!ref) {
			REX::WARN("Furniture: {:08X} is gone, the scene is played where the actors are", pending->ref);
			return false;
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		auto       spot = Furniture::SpotFor(ref, pending->type, player ? player->GetPosition() : ref->GetPosition());
		if (const auto& o = pending->offset; o[0] != 0.0F || o[1] != 0.0F || o[2] != 0.0F || o[3] != 0.0F) {
			// In the spot's frame: x to its right, y forward.
			const float c = std::cos(spot.heading), s = std::sin(spot.heading);
			spot.position.x += o[0] * c + o[1] * s;
			spot.position.y += -o[0] * s + o[1] * c;
			spot.position.z += o[2];
			spot.heading += o[3] * PI_F / 180.0F;
			REX::INFO("Furniture: scene offset ({}, {}, {}, {} deg)", o[0], o[1], o[2], o[3]);
		}
		for (const auto actor : a_actors) {
			PlaceActorAt(actor, spot);
		}
		return true;
	}

	// ---- Running scenes ----
	// Every scene started through 4Stim, player or not: other mods hear
	// about starts and ends (SceneEvents.h), and transitions and sequences
	// move scenes on by themselves (below). Main thread only.

	struct ActiveScene
	{
		std::vector<std::uint32_t> actors;  // role order
		std::string                sceneID;
		int                        speed = 0;

		// Autoplay: a sequence being played (step = its current entry), and
		// the game time left before the scene moves on (< 0 = it doesn't).
		std::shared_ptr<const SceneRegistry::Sequence> sequence;
		std::size_t                                    step = 0;
		float                                          remaining = -1.0F;

		// Speed from before a transition, for its destination (the
		// transition itself has only one speed).
		int carrySpeed = 0;

		// A scene the player picked during a transition: played when the
		// transition ends, instead of its destination.
		std::string queuedScene;

		// The furniture it's played on (0 / "" = none).
		std::uint32_t furnitureRef = 0;
		std::string   furnitureType;
	};

	// Remembers the outgoing speed when a scene enters a transition.
	void NoteCarrySpeed(ActiveScene& a_active, const std::string& a_newSceneID)
	{
		const auto from = SceneRegistry::Find(a_active.sceneID);
		const auto to = SceneRegistry::Find(a_newSceneID);
		if (to && to->IsTransition() && !(from && from->IsTransition())) {
			a_active.carrySpeed = a_active.speed;
		}
	}
	std::vector<ActiveScene> g_activeScenes;

	// Picker IDs of sequences (vs scenes) start with this.
	constexpr std::string_view SEQUENCE_PREFIX = "seq:"sv;

	// A sequence asked for (StartSequence / the picker) that attaches to the
	// next scene started with these actors, if that scene is its first.
	struct PendingSequence
	{
		std::shared_ptr<const SceneRegistry::Sequence> sequence;
		std::vector<std::uint32_t>                     actors;  // any order
	};
	std::mutex                     g_pendingSequenceLock;
	std::optional<PendingSequence> g_pendingSequence;

	void ArmAutoplay(ActiveScene& a_scene);
	void StartAutoplayTicks();
	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID);
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed);

	ActiveScene* FindActiveScene(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		return it != g_activeScenes.end() ? &*it : nullptr;
	}

	void TrackSceneStart(std::vector<std::uint32_t> a_actors, const std::string& a_sceneID)
	{
		// An actor can only be in one scene: a new scene replaces an old one.
		std::erase_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::any_of(a_scene.actors, [&](std::uint32_t id) { return std::ranges::find(a_actors, id) != a_actors.end(); });
		});
		ActiveScene scene{ a_actors, a_sceneID };
		if (const auto furniture = PendingFurnitureFor(a_actors, true)) {
			scene.furnitureRef = furniture->ref;
			scene.furnitureType = furniture->type;
			REX::INFO("Scene \"{}\": on furniture {:08X} ({})", a_sceneID, furniture->ref, furniture->type);
		}
		{
			std::scoped_lock lock(g_pendingSequenceLock);
			if (g_pendingSequence) {
				auto sorted = a_actors;
				auto wanted = g_pendingSequence->actors;
				std::ranges::sort(sorted);
				std::ranges::sort(wanted);
				const auto& first = g_pendingSequence->sequence->entries.front();
				if (sorted == wanted) {
					if (_stricmp(first.scene.c_str(), a_sceneID.c_str()) == 0) {
						scene.sequence = g_pendingSequence->sequence;
						REX::INFO("Sequence \"{}\": started ({} scene(s))", scene.sequence->id, scene.sequence->entries.size());
					}
					g_pendingSequence.reset();
				}
			}
		}
		g_activeScenes.push_back(std::move(scene));
		SceneEvents::SceneStarted(a_actors, a_sceneID);
		WatchAnimEvents(a_actors, a_sceneID);
		auto& added = g_activeScenes.back();
		if (added.sequence && added.sequence->entries.front().speed != 0) {
			PlayOnActiveScene(added, added.sequence->entries.front().scene, added.sequence->entries.front().speed);
		}
		ArmAutoplay(added);
	}

	void TrackSceneStop(std::uint32_t a_actorID)
	{
		const auto it = std::ranges::find_if(g_activeScenes, [&](const ActiveScene& a_scene) {
			return std::ranges::find(a_scene.actors, a_actorID) != a_scene.actors.end();
		});
		if (it == g_activeScenes.end()) {
			return;
		}
		const auto ended = std::move(*it);
		g_activeScenes.erase(it);
		SceneEvents::SceneEnded(ended.actors, ended.sceneID);
	}

	// A scene moved on by someone else (navigation, a speed key). Leaving
	// the scene a sequence is on ends the sequence: the player took over.
	void TrackSceneChange(std::uint32_t a_actorID, const std::string& a_sceneID, int a_speed)
	{
		const auto scene = FindActiveScene(a_actorID);
		if (!scene) {
			return;
		}
		const bool moved = _stricmp(scene->sceneID.c_str(), a_sceneID.c_str()) != 0;
		if (moved) {
			NoteCarrySpeed(*scene, a_sceneID);
			scene->queuedScene.clear();
			WatchAnimEvents(scene->actors, a_sceneID);
		}
		scene->sceneID = a_sceneID;
		scene->speed = a_speed;
		if (moved) {
			if (scene->sequence) {
				REX::INFO("Sequence \"{}\": stopped, the scene was moved to \"{}\"", scene->sequence->id, a_sceneID);
				scene->sequence.reset();
			}
			ArmAutoplay(*scene);
		}
	}

	// ---- Autoplay: transitions and sequences ----
	// A transition scene (one with a "destination") plays for its "length"
	// and then moves on to its destination; a sequence plays its scenes for
	// their durations, one after another. Time is counted in frames the game
	// isn't paused, so a pause menu doesn't skip ahead.
	//
	// The clock: a background thread queues one AutoplayTick task every
	// ~10 ms while anything is counting down. A task can't simply re-queue
	// itself: the game runs tasks added during its task pass in that same
	// pass, so that loops without the frame ever advancing (the game froze
	// for the whole transition, and the destination idle was lost).

	std::atomic<bool>                     g_autoplayTicking = false;   // the ticker thread is wanted
	std::atomic<bool>                     g_autoplayTickQueued = false;  // a tick task is waiting to run
	std::chrono::steady_clock::time_point g_autoplayLast;

	// Sets how long a_scene stays where it is before it moves on.
	void ArmAutoplay(ActiveScene& a_scene)
	{
		a_scene.remaining = -1.0F;
		if (a_scene.sequence) {
			const auto& entry = a_scene.sequence->entries[a_scene.step];
			a_scene.remaining = entry.duration;
		} else if (const auto scene = SceneRegistry::Find(a_scene.sceneID); scene && scene->IsTransition()) {
			// At the motion's end by default: the clip holds its last pose
			// past that, and the game starts blending a one-shot clip back to
			// the base pose ~0.35 s before the clip ends ("IdleStop", see
			// AnimEventHook, which moves on then if the timer hasn't).
			a_scene.remaining = g_settings.transitionLead >= 0.0F ?
			                        std::max(scene->length - g_settings.transitionLead, scene->length * 0.5F) :
			                        scene->length - g_settings.transitionLead;
		}
		if (a_scene.remaining >= 0.0F) {
			StartAutoplayTicks();
		}
	}

	// Plays a_sceneID at a_speed on a running scene's actors (all roles on
	// this frame), and records it: the running-scene list, the focused scene
	// if it's this one, and a SceneChanged event.
	bool PlayOnActiveScene(ActiveScene& a_active, const std::string& a_sceneID, int a_speed)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() != a_active.actors.size()) {
			REX::WARN("Autoplay: can't play \"{}\" on this scene's {} actor(s)", a_sceneID, a_active.actors.size());
			return false;
		}
		const int  speed = std::clamp(a_speed, 0, static_cast<int>(scene->speeds.size()) - 1);
		const auto& idles = scene->speeds[speed];
		std::string results;
		for (std::size_t role = 0; role < a_active.actors.size(); ++role) {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(a_active.actors[role]);
			const auto process = actor ? actor->currentProcess : nullptr;
			const bool ok = process && process->PlayIdle(*actor, idles[role], nullptr);
			results += std::format("{}{:08X} {}", role ? ", " : "", a_active.actors[role], ok ? "ok" : "refused");
		}
		REX::INFO("Autoplay: \"{}\" speed {}: PlayIdle {}", scene->id, speed + 1, results);
		WatchAnimEvents(a_active.actors, scene->id);
		const auto previous = a_active.sceneID;
		NoteCarrySpeed(a_active, scene->id);
		a_active.sceneID = scene->id;
		a_active.speed = speed;

		auto focused = GetPlayerScene();
		if (focused.Active() && focused.role0 == a_active.actors[0]) {
			focused.sceneID = scene->id;
			focused.speed = speed;
			SetPlayerScene(focused);
		}
		if (_stricmp(previous.c_str(), scene->id.c_str()) != 0) {
			SceneEvents::SceneChanged(a_active.actors, previous, scene->id);
		}
		return true;
	}

	// a_active's time is up: on to the next scene.
	void AdvanceAutoplay(ActiveScene& a_active)
	{
		if (!a_active.queuedScene.empty()) {
			const auto queued = std::exchange(a_active.queuedScene, std::string{});
			if (a_active.sequence) {
				REX::INFO("Sequence \"{}\": stopped, the player picked \"{}\"", a_active.sequence->id, queued);
				a_active.sequence.reset();
			}
			REX::INFO("Transition \"{}\" -> \"{}\" (picked during the transition)", a_active.sceneID, queued);
			if (PlayOnActiveScene(a_active, queued, a_active.carrySpeed)) {
				ArmAutoplay(a_active);
			} else {
				a_active.remaining = -1.0F;
			}
			return;
		}
		if (a_active.sequence) {
			const auto sequence = a_active.sequence;
			if (a_active.step + 1 < sequence->entries.size()) {
				++a_active.step;
				const auto& entry = sequence->entries[a_active.step];
				REX::INFO("Sequence \"{}\": step {}/{}: \"{}\"", sequence->id, a_active.step + 1, sequence->entries.size(), entry.scene);
				if (!PlayOnActiveScene(a_active, entry.scene, entry.speed)) {
					a_active.sequence.reset();
				}
				ArmAutoplay(a_active);
				return;
			}
			REX::INFO("Sequence \"{}\": finished on \"{}\"", sequence->id, a_active.sceneID);
			a_active.sequence.reset();
			// Ended on a transition: let it finish moving on.
			const auto scene = SceneRegistry::Find(a_active.sceneID);
			if (!scene || !scene->IsTransition()) {
				a_active.remaining = -1.0F;
				return;
			}
		}
		const auto scene = SceneRegistry::Find(a_active.sceneID);
		if (!scene || !scene->IsTransition()) {
			a_active.remaining = -1.0F;
			return;
		}
		REX::INFO("Transition \"{}\" -> \"{}\"", scene->id, scene->destination);
		if (PlayOnActiveScene(a_active, scene->destination, a_active.carrySpeed)) {
			ArmAutoplay(a_active);
		} else {
			a_active.remaining = -1.0F;
		}
	}

	void AutoplayTick()
	{
		g_autoplayTickQueued = false;
		const auto now = std::chrono::steady_clock::now();
		float      elapsed = std::chrono::duration<float>(now - g_autoplayLast).count();
		g_autoplayLast = now;
		elapsed = std::min(elapsed, 0.25F);  // a hitch or a load screen doesn't skip a whole step

		const auto ui = RE::UI::GetSingleton();
		const bool paused = ui && ui->menuMode > 0;
		if (!paused) {
			// AdvanceAutoplay can't add or remove running scenes, so the
			// list is safe to walk while it runs.
			for (auto& active : g_activeScenes) {
				if (active.remaining < 0.0F) {
					continue;
				}
				active.remaining -= elapsed;
				if (active.remaining <= 0.0F) {
					AdvanceAutoplay(active);
				}
			}
		}
		const bool waiting = std::ranges::any_of(g_activeScenes, [](const ActiveScene& a_scene) { return a_scene.remaining >= 0.0F; });
		if (!waiting) {
			g_autoplayTicking = false;
		}
	}

	// Main thread. Starts the ticker thread if it isn't running.
	void StartAutoplayTicks()
	{
		if (g_autoplayTicking.exchange(true)) {
			return;
		}
		g_autoplayLast = std::chrono::steady_clock::now();
		std::thread([]() {
			while (g_autoplayTicking) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				if (g_autoplayTicking && !g_autoplayTickQueued.exchange(true)) {
					F4SE::GetTaskInterface()->AddTask(AutoplayTick);
				}
			}
		}).detach();
	}

	// Plays a_sequence from its first entry on the running scene a_actorID
	// is in. Main thread.
	bool StartSequenceOnScene(std::uint32_t a_actorID, const std::shared_ptr<const SceneRegistry::Sequence>& a_sequence)
	{
		const auto active = FindActiveScene(a_actorID);
		if (!active) {
			REX::WARN("Sequence \"{}\": {:08X} isn't in a scene", a_sequence->id, a_actorID);
			return false;
		}
		if (active->actors.size() != a_sequence->actorCount) {
			REX::WARN("Sequence \"{}\" is for {} actor(s), the scene has {}", a_sequence->id, a_sequence->actorCount, active->actors.size());
			return false;
		}
		const auto& first = a_sequence->entries.front();
		if (!PlayOnActiveScene(*active, first.scene, first.speed)) {
			return false;
		}
		active->sequence = a_sequence;
		active->step = 0;
		REX::INFO("Sequence \"{}\": started on the running scene ({} scene(s))", a_sequence->id, a_sequence->entries.size());
		ArmAutoplay(*active);
		return true;
	}

	// Queues a_sequence for the next scene these actors start (its first
	// scene), from any thread.
	void QueueSequence(std::shared_ptr<const SceneRegistry::Sequence> a_sequence, std::vector<std::uint32_t> a_actors)
	{
		std::scoped_lock lock(g_pendingSequenceLock);
		g_pendingSequence = PendingSequence{ std::move(a_sequence), std::move(a_actors) };
	}

	bool IsPlayer(const RE::Actor* a_actor)
	{
		return a_actor && a_actor == RE::PlayerCharacter::GetSingleton();
	}


	// Calls a native Papyrus-exposed member function on an Actor from
	// native code, via the VM dispatch path confirmed in
	// RE/B/BSScriptUtil.h. Fire-and-forget: we don't wait for or read a
	// return value -- the bool returned here only means "successfully
	// queued", not "the function actually ran and did something".
	//
	// a_scriptName must be the script that actually DECLARES the native
	// function (e.g. "Actor" for SetRestrained/EnableAI, but
	// "ObjectReference" for MoveTo/SetAngle, which Actor only inherits).
	// Dispatching with the wrong declaring script silently "succeeds"
	// (dispatch=true) while doing nothing -- that's what happened with
	// MoveTo/SetAngle before this was split out.
	template <class... Args>
	bool CallActorMethod(RE::Actor* a_actor, std::string_view a_scriptName, std::string_view a_funcName, Args... a_args)
	{
		if (!g_vm || !a_actor) {
			return false;
		}

		auto&      handles = g_vm->GetObjectHandlePolicy();
		const auto handle = handles.GetHandleForObject(
			RE::BSScript::GetVMTypeID<RE::Actor>(),
			static_cast<const void*>(a_actor));
		if (handle == handles.EmptyHandle()) {
			REX::WARN("CallActorMethod: could not get handle for actor {:08X}", a_actor->GetFormID());
			return false;
		}

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;  // null: fire-and-forget
		return g_vm->DispatchMethodCall(
			static_cast<std::uint64_t>(handle),
			a_scriptName,
			a_funcName,
			callback,
			a_args...);
	}

	// Teleports a_actor to a_spot (on the main thread). Natively, not through
	// Papyrus SetPosition: that left the player where they stood.
	bool PlaceActorAt(RE::Actor* a_actor, const Furniture::Spot& a_spot)
	{
		if (!a_actor) {
			return false;
		}
		const auto id = a_actor->GetFormID();
		F4SE::GetTaskInterface()->AddTask([id, a_spot]() {
			const auto actor = RE::TESForm::GetFormByID<RE::Actor>(id);
			if (!actor) {
				return;
			}
			actor->SetPosition(a_spot.position, true);
			actor->SetHeading(a_spot.heading);
			const auto& now = actor->data.location;
			REX::INFO("Furniture: {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg; now at ({:.1f}, {:.1f}, {:.1f})",
				id, a_spot.position.x, a_spot.position.y, a_spot.position.z, a_spot.heading * 180.0F / PI_F, now.x, now.y, now.z);
		});
		return true;
	}

	// ---- Spike 3: play / stop a pose (unchanged) ----

	// Single-actor scene: plays role 0 of a registered scene on a_actor.
	void StartScene(std::monostate, RE::Actor* a_actor, RE::Actor*, std::string a_sceneID)
	{
		if (!a_actor) {
			REX::WARN("StartScene called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene) {
			REX::WARN("StartScene: no scene \"{}\" (check the Scenes folder and this log's load summary)", a_sceneID);
			g_focusNextScene = false;
			return;
		}
		const auto idle = scene->speeds[0][0];
		const auto sceneID = scene->id;

		REX::INFO("StartScene: {:08X} <- scene \"{}\" (idle {:08X})", a_actor->GetFormID(), scene->id, idle->GetFormID());
		PlaceOnPendingFurniture({ a_actor });

		// Papyrus calls arrive on a script thread; run engine animation code on the main thread.
		F4SE::GetTaskInterface()->AddTask([a_actor, idle, sceneID]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StartScene: actor has no AI process");
				return;
			}
			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StartScene: PlayIdle returned {}", ok);
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor);
			if (ok) {
				TrackSceneStart({ a_actor->GetFormID() }, sceneID);
				if (focus) {
					PlayerScene focused{ a_actor->GetFormID(), 0, sceneID, 0 };
					if (const auto active = FindActiveScene(a_actor->GetFormID())) {
						focused.furniture = active->furnitureType;
					}
					SetPlayerScene(std::move(focused));
				}
			}
		});
	}

	void StopScene(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("StopScene called with a null actor");
			return;
		}

		REX::INFO("StopScene: {:08X}", a_actor->GetFormID());

		// FO4.esm's IdleStop (000E9855), played the same way as a pose.
		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopScene: could not find IdleStop in Fallout4.esm");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor, idle]() {
			const auto process = a_actor->currentProcess;
			if (!process) {
				REX::WARN("StopScene: actor has no AI process");
				return;
			}

			const auto ok = process->PlayIdle(*a_actor, idle, nullptr);
			REX::INFO("StopScene: PlayIdle(IdleStop) returned {}", ok);
			ClearFocusIfIn(a_actor->GetFormID());
			TrackSceneStop(a_actor->GetFormID());
		});
	}

	// ---- Spike 4: lock / unlock an actor in place ----
	// Uses CallActorMethod to invoke the engine's own Papyrus-native
	// Actor.SetRestrained / Actor.EnableAI, confirmed from Actor.psc:
	//   bool Function SetRestrained(bool abRestrained = true) native
	//   Function EnableAI(bool abEnable = true, bool abPauseVoice = false) native

	// Blocks the Activate/Talk prompt and stops dialogue (including an
	// automatic proximity greet, which needs no button press and can fire
	// the instant an actor is moved near the player -- confirmed: a full
	// dialogue menu opened on its own within ~1s of MoveActorTo, well
	// before LockActor used to apply this same block ~3s later). Call
	// this BEFORE MoveActorTo brings the actor near the
	// player, not as part of locking them in place afterward.
	void SuppressInteraction(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("SuppressInteraction called with a null actor");
			return;
		}

		const auto blockOk = CallActorMethod(a_actor, "ObjectReference"sv, "BlockActivation"sv, true, true);
		const auto dialogueOk = CallActorMethod(a_actor, "Actor"sv, "AllowPCDialogue"sv, false);
		REX::INFO("SuppressInteraction: {:08X} BlockActivation dispatch={}, AllowPCDialogue dispatch={}",
			a_actor->GetFormID(), blockOk, dialogueOk);
	}

	void RestoreInteraction(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("RestoreInteraction called with a null actor");
			return;
		}

		const auto blockOk = CallActorMethod(a_actor, "ObjectReference"sv, "BlockActivation"sv, false, false);
		const auto dialogueOk = CallActorMethod(a_actor, "Actor"sv, "AllowPCDialogue"sv, true);
		REX::INFO("RestoreInteraction: {:08X} BlockActivation dispatch={}, AllowPCDialogue dispatch={}",
			a_actor->GetFormID(), blockOk, dialogueOk);
	}

	void LockActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("LockActor called with a null actor");
			return;
		}

		REX::INFO("LockActor: {:08X}", a_actor->GetFormID());

		const auto restrainedOk = CallActorMethod(a_actor, "Actor"sv, "SetRestrained"sv, true);
		const auto aiOk = CallActorMethod(a_actor, "Actor"sv, "EnableAI"sv, false, false);
		REX::INFO("LockActor: SetRestrained dispatch={}, EnableAI dispatch={}", restrainedOk, aiOk);
	}

	void UnlockActor(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("UnlockActor called with a null actor");
			return;
		}

		REX::INFO("UnlockActor: {:08X}", a_actor->GetFormID());

		const auto restrainedOk = CallActorMethod(a_actor, "Actor"sv, "SetRestrained"sv, false);
		const auto aiOk = CallActorMethod(a_actor, "Actor"sv, "EnableAI"sv, true, false);
		REX::INFO("UnlockActor: SetRestrained dispatch={}, EnableAI dispatch={}", restrainedOk, aiOk);
	}

	// ---- Spike 4 continued: position one actor relative to another ----
	// Uses `data.angle.z` (radians) for the anchor's current heading -- the
	// one field confirmed directly from Actor.h's own inline GetHeading():
	// `return data.angle.z;`. Position itself is no longer read in native
	// code at all; MoveTo computes it from the anchor's own location.

	// Places a_target at a_distance units from a_anchor, offset by
	// a_angleOffsetDeg degrees from the anchor's own facing, then turns
	// a_target to face back toward the anchor.
	//
	// Goes through ObjectReference's own native MoveTo/SetAngle (dispatched
	// via CallActorMethod, same as LockActor/UnlockActor) rather than the
	// raw C++ SetPosition/SetHeading virtuals. Those only update partial
	// actor state and caused real desync bugs (mesh stretching, then a
	// facegen glitch after a QueueUpdate workaround); MoveTo/SetAngle are
	// the same native functions the engine itself uses for every teleport,
	// so they should keep position, 3D, and physics in sync properly.
	//
	// MoveTo's offsets are relative to the ANCHOR's own local facing
	// (X = right, Y = forward), so we don't need the anchor's world
	// heading to compute the offset -- only to compute the final facing
	// angle afterward.
	void MoveActorTo(std::monostate, RE::Actor* a_target, RE::Actor* a_anchor, float a_distance, float a_angleOffsetDeg)
	{
		if (!a_target || !a_anchor) {
			REX::WARN("MoveActorTo called with a null actor");
			return;
		}

		// NOTE: switched from MoveTo(ObjectReference, ...) to SetPosition
		// (plain floats) -- MoveTo's object argument silently no-op'd even
		// with timing ruled out as the cause, while SetAngle (plain floats,
		// no object arg) worked correctly. Computing world-space XYZ
		// ourselves sidesteps whatever that argument-packing issue was.
		// data.location is UNCONFIRMED (inferred alongside the confirmed
		// data.angle.z); if this fails to compile, that's the first thing
		// to check -- search TESObjectREFR.h for GetPositionX/Y/Z or the
		// real member name.
		const auto anchorPos = a_anchor->data.location;
		const float anchorHeadingRad = a_anchor->data.angle.z;
		const float anchorHeadingDeg = anchorHeadingRad * 180.0f / PI_F;

		const float angleRad = anchorHeadingRad + a_angleOffsetDeg * PI_F / 180.0f;
		const float targetX = anchorPos.x + a_distance * std::sin(angleRad);
		const float targetY = anchorPos.y + a_distance * std::cos(angleRad);
		const float targetZ = anchorPos.z;
		const float targetHeadingDeg = anchorHeadingDeg + a_angleOffsetDeg + 180.0f;  // face back toward anchor

		REX::INFO("MoveActorTo: {:08X} relative to {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
			a_target->GetFormID(), a_anchor->GetFormID(), targetX, targetY, targetZ, targetHeadingDeg);

		const auto moveOk = CallActorMethod(a_target, "ObjectReference"sv, "SetPosition"sv, targetX, targetY, targetZ);
		const auto angleOk = CallActorMethod(a_target, "ObjectReference"sv, "SetAngle"sv, 0.0f, 0.0f, targetHeadingDeg);
		REX::INFO("MoveActorTo: SetPosition dispatch={}, SetAngle dispatch={}", moveOk, angleOk);
	}

	// ---- Scene collision ----
	// Paired animations put both actors' references on one spot, so their
	// collision capsules overlap for the whole scene and shove each other
	// apart (confirmed: equal-and-opposite 40-70 unit displacement before
	// the idles even started). Bodies sharing a nonzero Havok system group
	// skip collision with EACH OTHER but still collide with the world, so
	// they keep standing on the ground. (The kNoCollision flag would also
	// drop ground collision, so it's deliberately not used.)

	// Original filter per actor, restored at scene end. Not save-persisted:
	// character controllers are rebuilt on load anyway.
	std::unordered_map<std::uint32_t, std::uint32_t> g_savedFilters;

	RE::bhkCharacterController* GetCharController(RE::Actor* a_actor)
	{
		const auto process = a_actor ? a_actor->currentProcess : nullptr;
		const auto middleHigh = process ? process->middleHigh : nullptr;
		return middleHigh ? middleHigh->charController.get() : nullptr;
	}

	bool ApplyFilter(RE::Actor* a_actor, std::uint32_t a_filter)
	{
		const auto controller = GetCharController(a_actor);
		if (!controller) {
			return false;
		}
		RE::CFilter filter{};
		filter.filter = a_filter;
		return controller->SetCollisionFilterInfo(filter);
	}

	void IgnorePairCollision(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("IgnorePairCollision called with a null actor");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor0, a_actor1]() {
			const auto c0 = GetCharController(a_actor0);
			const auto c1 = GetCharController(a_actor1);
			const auto b0 = c0 ? c0->GetBodyImpl() : nullptr;
			const auto b1 = c1 ? c1->GetBodyImpl() : nullptr;
			if (!b0 || !b1) {
				REX::WARN("IgnorePairCollision: missing character controller or body");
				return;
			}

			const auto f0 = b0->m_collisionFilterInfo;
			const auto f1 = b1->m_collisionFilterInfo;
			g_savedFilters.try_emplace(a_actor0->GetFormID(), f0);
			g_savedFilters.try_emplace(a_actor1->GetFormID(), f1);

			RE::CFilter cf0{}, cf1{};
			cf0.filter = f0;
			cf1.filter = f1;
			// Share actor 0's group if it has one; otherwise a fixed group
			// unlikely to clash with anything the game assigns.
			std::uint32_t group = cf0.GetSystemGroup();
			if (group == 0) {
				group = 0xF5A1;
			}
			cf0.SetSystemGroup(group);
			cf1.SetSystemGroup(group);

			const auto ok0 = c0->SetCollisionFilterInfo(cf0);
			const auto ok1 = c1->SetCollisionFilterInfo(cf1);
			REX::INFO("IgnorePairCollision: {:08X} filter {:08X} -> {:08X} ({}), {:08X} filter {:08X} -> {:08X} ({})",
				a_actor0->GetFormID(), f0, cf0.filter, ok0, a_actor1->GetFormID(), f1, cf1.filter, ok1);
		});
	}

	void RestoreCollision(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			REX::WARN("RestoreCollision called with a null actor");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor]() {
			const auto it = g_savedFilters.find(a_actor->GetFormID());
			if (it == g_savedFilters.end()) {
				REX::INFO("RestoreCollision: {:08X} has no saved filter", a_actor->GetFormID());
				return;
			}
			const auto ok = ApplyFilter(a_actor, it->second);
			REX::INFO("RestoreCollision: {:08X} filter -> {:08X} ({})", a_actor->GetFormID(), it->second, ok);
			g_savedFilters.erase(it);
		});
	}

	// ---- Two-actor scenes ----

	// Puts both actors on the same spot, a_distance units in front of
	// a_anchor, with the same heading: a_anchor's heading plus
	// a_headingOffsetDeg (180 = facing the anchor, 0 = facing where the
	// anchor faces; use distance 0 / offset 0 to build the scene on the
	// anchor's own spot, e.g. when the anchor is a participant). Paired animations
	// are authored around one shared origin, with the offset between the
	// actors baked into the animation itself, so both must start from the
	// same point and facing.
	void PlacePair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, RE::Actor* a_anchor, float a_distance, float a_headingOffsetDeg)
	{
		if (!a_actor0 || !a_actor1 || !a_anchor) {
			REX::WARN("PlacePair called with a null actor");
			return;
		}
		if (PlaceOnPendingFurniture({ a_actor0, a_actor1 })) {
			return;
		}

		const auto  anchorPos = a_anchor->data.location;
		const float anchorHeadingRad = a_anchor->data.angle.z;
		const float x = anchorPos.x + a_distance * std::sin(anchorHeadingRad);
		const float y = anchorPos.y + a_distance * std::cos(anchorHeadingRad);
		const float z = anchorPos.z;
		float headingDeg = std::fmod(anchorHeadingRad * 180.0f / PI_F + a_headingOffsetDeg, 360.0f);
		if (headingDeg < 0.0f) {
			headingDeg += 360.0f;
		}

		REX::INFO("PlacePair: {:08X} + {:08X} -> ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
			a_actor0->GetFormID(), a_actor1->GetFormID(), x, y, z, headingDeg);

		for (auto* actor : { a_actor0, a_actor1 }) {
			const auto moveOk = CallActorMethod(actor, "ObjectReference"sv, "SetPosition"sv, x, y, z);
			const auto angleOk = CallActorMethod(actor, "ObjectReference"sv, "SetAngle"sv, 0.0f, 0.0f, headingDeg);
			REX::INFO("PlacePair: {:08X} SetPosition dispatch={}, SetAngle dispatch={}", actor->GetFormID(), moveOk, angleOk);
		}
	}

	// Starts roles 0 and 1 of a registered two-actor scene in the same task,
	// i.e. on the same game frame, so the two actors begin in step.
	void PlaySceneIdles(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1, std::string a_sceneID)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("PlaySceneIdles called with a null actor");
			return;
		}

		const auto scene = SceneRegistry::Find(a_sceneID);
		if (!scene || scene->actors.size() < 2) {
			REX::WARN("PlaySceneIdles: no two-actor scene \"{}\"", a_sceneID);
			g_focusNextScene = false;
			return;
		}
		const auto idle0 = scene->speeds[0][0];
		const auto idle1 = scene->speeds[0][1];
		const auto sceneID = scene->id;

		REX::INFO("PlaySceneIdles: scene \"{}\": {:08X} <- {:08X}, {:08X} <- {:08X}",
			scene->id, a_actor0->GetFormID(), idle0->GetFormID(), a_actor1->GetFormID(), idle1->GetFormID());

		F4SE::GetTaskInterface()->AddTask([a_actor0, idle0, a_actor1, idle1, sceneID]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			if (!p0 || !p1) {
				REX::WARN("PlaySceneIdles: an actor has no AI process");
				return;
			}
			for (auto* actor : { a_actor0, a_actor1 }) {
				const auto& pos = actor->data.location;
				REX::INFO("PlaySceneIdles: {:08X} actually at ({:.1f}, {:.1f}, {:.1f}), heading {:.1f} deg",
					actor->GetFormID(), pos.x, pos.y, pos.z, actor->data.angle.z * 180.0f / PI_F);
			}
			const auto ok0 = p0->PlayIdle(*a_actor0, idle0, nullptr);
			const auto ok1 = p1->PlayIdle(*a_actor1, idle1, nullptr);
			REX::INFO("PlaySceneIdles: PlayIdle returned {} / {}", ok0, ok1);
			const bool focus = g_focusNextScene.exchange(false) || IsPlayer(a_actor0) || IsPlayer(a_actor1);
			if (!ok0 && !ok1) {
				return;
			}
			TrackSceneStart({ a_actor0->GetFormID(), a_actor1->GetFormID() }, sceneID);
			if (focus) {
				PlayerScene focused{ a_actor0->GetFormID(), a_actor1->GetFormID(), sceneID, 0 };
				if (const auto active = FindActiveScene(a_actor0->GetFormID())) {
					focused.furniture = active->furnitureType;
				}
				SetPlayerScene(std::move(focused));
			}
		});
	}

	// Queues sequence a_sequenceID for the next scene started with these
	// actors (akActor1 None for a solo sequence) and returns the scene to
	// start, its first one; "" if there's no such sequence or the actor
	// count doesn't match. FourStimScene.BeginPairSequence wraps this.
	std::string QueueSequenceNative(std::monostate, std::string a_sequenceID, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequenceID);
		if (!sequence || !a_actor0) {
			REX::WARN("QueueSequence: no sequence \"{}\" (or no actor)", a_sequenceID);
			return {};
		}
		std::vector<std::uint32_t> actors{ a_actor0->GetFormID() };
		if (a_actor1) {
			actors.push_back(a_actor1->GetFormID());
		}
		if (actors.size() != sequence->actorCount) {
			REX::WARN("QueueSequence: \"{}\" is for {} actor(s), {} given", sequence->id, sequence->actorCount, actors.size());
			return {};
		}
		QueueSequence(sequence, std::move(actors));
		return sequence->entries.front().scene;
	}

	// Plays sequence a_sequenceID on the scene a_actor is already in.
	// Returns false if there's no such sequence; the rest is checked on the
	// next frame (see the log).
	bool StartSequenceOnSceneNative(std::monostate, RE::Actor* a_actor, std::string a_sequenceID)
	{
		const auto sequence = SceneRegistry::FindSequence(a_sequenceID);
		if (!sequence || !a_actor) {
			REX::WARN("StartSequenceOnScene: no sequence \"{}\" (or no actor)", a_sequenceID);
			return false;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID(), sequence]() { StartSequenceOnScene(id, sequence); });
		return true;
	}

	// Stops the sequence the scene a_actor is in is playing (the scene stays
	// where it is).
	void StopSequenceNative(std::monostate, RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		F4SE::GetTaskInterface()->AddTask([id = a_actor->GetFormID()]() {
			const auto active = FindActiveScene(id);
			if (active && active->sequence) {
				REX::INFO("Sequence \"{}\": stopped", active->sequence->id);
				active->sequence.reset();
				ArmAutoplay(*active);  // a transition still finishes
			}
		});
	}

	// Number of actors (roles) in a registered scene, or 0 if it doesn't exist.
	std::int32_t GetSceneActorCount(std::monostate, std::string a_sceneID)
	{
		const auto scene = SceneRegistry::Find(a_sceneID);
		return scene ? static_cast<std::int32_t>(scene->actors.size()) : 0;
	}

	// Re-reads all scene files (handy while authoring). Returns the count loaded.
	std::int32_t ReloadScenes(std::monostate)
	{
		const auto count = SceneRegistry::Reload();
		HUD::LoadConfig();  // theme and Utility entries too, for HUD authors
		return count;
	}

	bool RegisterForSceneEvents(std::monostate, RE::TESForm* a_receiver)
	{
		return SceneEvents::Register(a_receiver);
	}

	void UnregisterForSceneEvents(std::monostate, RE::TESForm* a_receiver)
	{
		SceneEvents::Unregister(a_receiver);
	}

	// Clears both actors' poses in the same task (same frame), mirroring
	// PlaySceneIdles, so the pair releases together.
	void StopPair(std::monostate, RE::Actor* a_actor0, RE::Actor* a_actor1)
	{
		if (!a_actor0 || !a_actor1) {
			REX::WARN("StopPair called with a null actor");
			return;
		}

		const auto handler = RE::TESDataHandler::GetSingleton();
		const auto idle = handler ? handler->LookupForm<RE::TESIdleForm>(0x00E9855, "Fallout4.esm"sv) : nullptr;
		if (!idle) {
			REX::WARN("StopPair: could not find IdleStop in Fallout4.esm");
			return;
		}

		F4SE::GetTaskInterface()->AddTask([a_actor0, a_actor1, idle]() {
			const auto p0 = a_actor0->currentProcess;
			const auto p1 = a_actor1->currentProcess;
			const auto ok0 = p0 ? p0->PlayIdle(*a_actor0, idle, nullptr) : false;
			const auto ok1 = p1 ? p1->PlayIdle(*a_actor1, idle, nullptr) : false;
			REX::INFO("StopPair: {:08X} / {:08X} IdleStop returned {} / {}", a_actor0->GetFormID(), a_actor1->GetFormID(), ok0, ok1);
			ClearFocusIfIn(a_actor0->GetFormID());
			ClearFocusIfIn(a_actor1->GetFormID());
			TrackSceneStop(a_actor0->GetFormID());
			TrackSceneStop(a_actor1->GetFormID());
		});
	}

	// ---- Player controls during scenes ----
	// A dedicated input-enable layer, held for the whole scene, blocks
	// everything a scene shouldn't allow (menus, favorites, combat, activate,
	// jumping, VATS...) while leaving looking enabled. Movement is toggled
	// by the scene view: on in the free camera (which flies with the
	// movement keys), off in first person. Replaces SetPlayerAIDriven, which
	// also disabled the look/move input the free camera needs.

	RE::BSTSmartPointer<RE::BSInputEnableLayer> g_sceneLayer;

	void SetLayerUserEvents(std::int32_t a_flags, bool a_enable)
	{
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (manager && g_sceneLayer) {
			manager->EnableUserEvent(g_sceneLayer->layerID, static_cast<RE::UserEvents::USER_EVENT_FLAG>(a_flags), a_enable, RE::UserEvents::SENDER_ID::kScript);
		}
	}

	void SetLayerOtherEvents(std::int32_t a_flags, bool a_enable)
	{
		const auto manager = RE::BSInputEnableManager::GetSingleton();
		if (manager && g_sceneLayer) {
			manager->EnableOtherEvent(g_sceneLayer->layerID, static_cast<RE::OtherInputEvents::OTHER_EVENT_FLAG>(a_flags), a_enable, RE::UserEvents::SENDER_ID::kScript);
		}
	}

	constexpr std::int32_t flag(RE::UserEvents::USER_EVENT_FLAG a_flag) { return static_cast<std::int32_t>(a_flag); }
	constexpr std::int32_t flag(RE::OtherInputEvents::OTHER_EVENT_FLAG a_flag) { return static_cast<std::int32_t>(a_flag); }

	void LockPlayerControls(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto manager = RE::BSInputEnableManager::GetSingleton();
			if (!manager || g_sceneLayer) {
				return;
			}
			if (!manager->AllocateNewLayer(g_sceneLayer, "4Stim") || !g_sceneLayer) {
				REX::WARN("LockPlayerControls: couldn't allocate an input layer");
				return;
			}
			using U = RE::UserEvents::USER_EVENT_FLAG;
			using O = RE::OtherInputEvents::OTHER_EVENT_FLAG;
			SetLayerUserEvents(flag(U::kActivate) | flag(U::kMenu) | flag(U::kPOVSwitch) | flag(U::kFighting) | flag(U::kSneaking) |
								   flag(U::kMainFour) | flag(U::kJumping) | flag(U::kVATS),
				false);
			SetLayerOtherEvents(flag(O::kJournalTabs) | flag(O::kActivation) | flag(O::kFastTravel) | flag(O::kPOVChange) | flag(O::kVATS) |
									flag(O::kFavorites) | flag(O::kPipboyLight) | flag(O::kZKey) | flag(O::kRunning) | flag(O::kSprinting),
				false);
			REX::INFO("LockPlayerControls: layer {} active", g_sceneLayer->layerID);
		});
	}

	void ReleaseSceneLayer()  // main thread only
	{
		if (g_sceneLayer) {
			SetLayerUserEvents(flag(RE::UserEvents::USER_EVENT_FLAG::kAll), true);
			SetLayerOtherEvents(flag(RE::OtherInputEvents::OTHER_EVENT_FLAG::kAll), true);
			g_sceneLayer.reset();
			REX::INFO("Player controls restored");
		}
	}

	void UnlockPlayerControls(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() { ReleaseSceneLayer(); });
	}

	// ---- Scene camera and HUD ----
	// While a scene with the player (or one the player chose to watch) runs,
	// the vanilla HUD is hidden and the camera is the third-person free
	// camera, slowed by fFreeCameraSpeed from 4Stim.ini. At the end the HUD
	// comes back, the speed is restored, and the camera returns to the view
	// the player started in. The camera's real state is queried each time
	// (QCameraEquals) so it can't get out of step with the game.
	// (A first-person option is shelved: the game's first-person mode hides
	// the player's body, so it needs a head-pinned camera instead.)

	std::atomic<bool> g_sceneCameraActive = false;
	bool              g_startedFirstPerson = false;
	bool              g_startViewSaved = false;
	float             g_savedFreeCamSpeed = -1.0f;  // < 0: nothing saved

	constexpr auto FREE_CAM_SPEED_SETTING = "fFreeCameraTranslationSpeed:Camera"sv;

	void SetHUDVisible(bool a_visible)
	{
		if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage("HUDMenu", a_visible ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
		}
	}

	void ScaleFreeCameraSpeed()
	{
		const auto setting = RE::GetINISetting(FREE_CAM_SPEED_SETTING);
		if (!setting) {
			REX::WARN("Scene camera: game setting {} not found, free camera speed unchanged", FREE_CAM_SPEED_SETTING);
			return;
		}
		if (g_savedFreeCamSpeed < 0.0f) {
			g_savedFreeCamSpeed = setting->GetFloat();
		}
		setting->SetFloat(g_savedFreeCamSpeed * g_settings.freeCameraSpeed);
		REX::INFO("Scene camera: free camera speed {} -> {}", g_savedFreeCamSpeed, setting->GetFloat());
	}

	void RestoreFreeCameraSpeed()
	{
		if (g_savedFreeCamSpeed < 0.0f) {
			return;
		}
		if (const auto setting = RE::GetINISetting(FREE_CAM_SPEED_SETTING)) {
			setting->SetFloat(g_savedFreeCamSpeed);
		}
		g_savedFreeCamSpeed = -1.0f;
	}

	// Enters the free camera from third person. First person hides the
	// player's body (only the first-person arms are drawn), and entering the
	// free camera straight from first person keeps it hidden, so switch to
	// third person first and wait -- the game blends between views over a
	// few frames -- until it has actually taken effect. Main thread only.
	void EnterFreeCamera(int a_attempt)
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !g_sceneCameraActive || camera->QCameraEquals(RE::CameraState::kFree)) {
			return;
		}
		if (!camera->QCameraEquals(RE::CameraState::k3rdPerson)) {
			if (a_attempt == 0) {
				camera->SetState(camera->cameraStates[RE::CameraState::k3rdPerson].get());
			}
			if (a_attempt < 60) {  // about a second at 60 fps
				F4SE::GetTaskInterface()->AddTask([a_attempt]() { EnterFreeCamera(a_attempt + 1); });
				return;
			}
			REX::WARN("Scene camera: third person didn't take effect, entering free camera anyway");
		}
		camera->ToggleFreeCameraMode(false);  // false = don't freeze time
		REX::INFO("Scene camera: free camera");
	}

	// Records whether the player is in first person, so the scene can force
	// third person (Game.ForceThirdPerson, the game's full view change that
	// also swaps the first-person arms for the body) and still return the
	// player to first person afterwards. Call before forcing third person.
	void SaveStartView(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (camera && !g_sceneCameraActive) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
				g_startViewSaved = true;
			}
			g_focusNextScene = true;  // the scene being prepared is one the player watches
		});
	}

	void BeginSceneCamera(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			const auto camera = RE::PlayerCamera::GetSingleton();
			if (!camera || g_sceneCameraActive) {
				return;
			}
			if (!g_startViewSaved) {
				g_startedFirstPerson = camera->QCameraEquals(RE::CameraState::kFirstPerson);
			}
			g_sceneCameraActive = true;
			SetHUDVisible(false);
			ScaleFreeCameraSpeed();
			EnterFreeCamera(0);
		});
	}

	void EndSceneCamera(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			if (!g_sceneCameraActive) {
				return;
			}
			g_sceneCameraActive = false;
			g_startViewSaved = false;
			if (const auto camera = RE::PlayerCamera::GetSingleton()) {
				if (camera->QCameraEquals(RE::CameraState::kFree)) {
					camera->ToggleFreeCameraMode(false);
				}
				const auto restore = g_startedFirstPerson ? RE::CameraState::kFirstPerson : RE::CameraState::k3rdPerson;
				if (!camera->QCameraEquals(restore)) {
					camera->SetState(camera->cameraStates[restore].get());
				}
			}
			RestoreFreeCameraSpeed();
			SetHUDVisible(true);
			REX::INFO("Scene camera: restored ({})", g_startedFirstPerson ? "first person" : "third person");
		});
	}

	// ---- Speed and navigation (the player's current scene) ----
	// Both replay idles on the actors already in place: same spot, same
	// collision setup, both roles started on the same frame. Navigation keeps
	// the roles (destination scenes always have the same number of actors)
	// and keeps the current speed where the destination has that many.

	// Plays speed a_speed of a_scene on the player's scene actors, on the main
	// thread, and records the new state.
	void PlayPlayerScene(const SceneRegistry::Scene& a_scene, int a_speed, const PlayerScene& a_current)
	{
		const auto  idles = a_scene.speeds[a_speed];
		const int   count = static_cast<int>(a_scene.speeds.size());
		PlayerScene next = a_current;
		next.sceneID = a_scene.id;
		next.speed = a_speed;
		F4SE::GetTaskInterface()->AddTask([idles, next, count, previous = a_current]() {
			// The scene may have ended or moved on since this was queued (an
			// End right after a speed key, say): then don't bring it back.
			const auto now = GetPlayerScene();
			if (now.role0 != previous.role0 || now.role1 != previous.role1 || now.sceneID != previous.sceneID || now.speed != previous.speed) {
				REX::INFO("Player scene: change to \"{}\" speed {} dropped, the scene changed first", next.sceneID, next.speed + 1);
				return;
			}
			const std::uint32_t ids[2] = { next.role0, next.role1 };
			for (std::size_t role = 0; role < idles.size() && role < 2; ++role) {
				const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role]);
				const auto process = actor ? actor->currentProcess : nullptr;
				if (process) {
					process->PlayIdle(*actor, idles[role], nullptr);
				}
			}
			SetPlayerScene(next);
			REX::INFO("Player scene: \"{}\" speed {}", next.sceneID, next.speed + 1);
			TrackSceneChange(next.role0, next.sceneID, next.speed);
			if (next.sceneID != previous.sceneID) {
				SceneEvents::SceneChanged(next.ActorIDs(), previous.sceneID, next.sceneID);
			} else if (next.speed != previous.speed) {
				SceneEvents::SpeedChanged(next.ActorIDs(), next.sceneID, next.speed, count);
			}
		});
	}

	// Changes the player's scene speed by a_delta. Returns the new speed
	// (1-based), or 0 if the player isn't in a scene.
	std::int32_t ChangeSceneSpeedImpl(std::int32_t a_delta)
	{
		const auto current = GetPlayerScene();
		const auto scene = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		if (!scene) {
			return 0;
		}
		const int count = static_cast<int>(scene->speeds.size());
		const int speed = std::clamp(current.speed + a_delta, 0, count - 1);
		if (speed != current.speed) {
			PlayPlayerScene(*scene, speed, current);
		}
		return speed + 1;
	}

	std::int32_t ChangeSceneSpeed(std::monostate, std::int32_t a_delta)
	{
		return ChangeSceneSpeedImpl(a_delta);
	}

	// Moves the player's scene to a_destination (normally one of the current
	// scene's navigation links). Returns false if that isn't possible.
	bool NavigateSceneImpl(const std::string& a_destination)
	{
		const auto current = GetPlayerScene();
		const auto from = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;
		const auto to = SceneRegistry::Find(a_destination);
		if (!from || !to || to->actors.size() != from->actors.size()) {
			REX::WARN("NavigateScene: can't go to \"{}\"", a_destination);
			return false;
		}
		if (!SexesFit(*to, current.ActorIDs())) {
			REX::WARN("NavigateScene: \"{}\" is for other sexes than this scene's actors (bMatchSex)", a_destination);
			return false;
		}
		if (!FurnitureFits(to->furniture, current.furniture)) {
			REX::WARN("NavigateScene: \"{}\" is for {} furniture, this scene is on {}", a_destination,
				to->furniture.empty() ? "no" : to->furniture, current.furniture.empty() ? "none" : current.furniture);
			return false;
		}
		if (from->IsTransition()) {
			// Let the transition finish, then go there instead of its
			// destination (no jump out of the middle of it).
			F4SE::GetTaskInterface()->AddTask([actorID = current.role0, sceneID = to->id]() {
				if (const auto active = FindActiveScene(actorID)) {
					active->queuedScene = sceneID;
					REX::INFO("NavigateScene: \"{}\" queued until the transition ends", sceneID);
				}
			});
			return true;
		}
		const int speed = std::min(current.speed, static_cast<int>(to->speeds.size()) - 1);
		PlayPlayerScene(*to, speed, current);
		return true;
	}

	bool NavigateScene(std::monostate, std::string a_destination)
	{
		return NavigateSceneImpl(a_destination);
	}

	// Fills {n} in a navigation label with the name of the actor in role n,
	// and capitalizes the first letter.
	std::string FormatNavLabel(std::string a_label, const PlayerScene& a_scene)
	{
		const std::uint32_t ids[2] = { a_scene.role0, a_scene.role1 };
		for (int role = 0; role < 2; ++role) {
			const std::string token = "{" + std::to_string(role) + "}";
			std::string       name = "partner";
			if (const auto actor = RE::TESForm::GetFormByID<RE::Actor>(ids[role])) {
				if (const auto n = actor->GetDisplayFullName(); n && *n) {
					name = n;
				}
			}
			for (auto pos = a_label.find(token); pos != std::string::npos; pos = a_label.find(token, pos + name.size())) {
				a_label.replace(pos, token.size(), name);
			}
		}
		if (!a_label.empty()) {
			a_label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(a_label[0])));
		}
		return a_label;
	}

	// ---- Scene picker menu (AS3 front-end) ----
	// FourStimPickerMenu.swf, loaded like the game's own menus: AS3 calls
	// C++ through the BGSCodeObj the engine attaches to root1.Menu_mc
	// (RequestScenes, PickScene, CloseMenu), C++ calls AS3 by invoking
	// SetScenes on that clip, and navigation input arrives in AS3 as
	// ProcessUserEvent (forwarded by IMenu). The menu shows 2-actor scenes
	// when opened with a target, 1-actor scenes for a solo player scene.

	// Navigation key currently held (-1 up, +1 down, 0 none), when that hold
	// began, and a counter bumped on every press. Written by the input hook,
	// read by the picker's per-frame update.
	std::atomic<int>                                   g_navHeld = 0;
	std::atomic<std::chrono::steady_clock::time_point> g_navHeldSince{};
	std::atomic<std::uint32_t>                         g_navPressCount = 0;

	constexpr auto PICKER_MENU = "FourStimPickerMenu";
	constexpr auto END_SCENE_ID = "__end__";

	enum class PickerMode
	{
		kStart,     // start a new scene with g_pickerTarget (0 = solo)
		kNavigate,  // the focused scene's navigation links, plus "End scene" (used when the HUD isn't available)
		kSearch     // every scene with the focused scene's actor count; picking one moves the scene there
	};
	std::atomic<std::uint32_t> g_pickerTarget = 0;  // form ID of the partner, 0 = solo
	std::atomic<PickerMode>    g_pickerMode = PickerMode::kStart;

	class PickerMenu : public RE::IMenu
	{
	public:
		static RE::IMenu* Create(const RE::UIMessage&) { return new PickerMenu(); }

		PickerMenu()
		{
			using F = RE::UI_MENU_FLAGS;
			// kAdvancesUnderPauseMenu: the picker pauses the game, and a menu at
			// this depth otherwise stops advancing (no AS3 frame events, no
			// hover updates) while the game is paused.
			for (const auto f : { F::kPausesGame, F::kUsesCursor, F::kUsesMenuContext, F::kModal, F::kDisablePauseMenu,
					 F::kTopmostRenderedMenu, F::kUpdateUsesCursor, F::kAdvancesUnderPauseMenu, F::kRendersUnderPauseMenu }) {
				UpdateFlag(f, true);
			}
			depthPriority = RE::UI_DEPTH_PRIORITY::kGameMessage;
			inputContext = RE::UserEvents::INPUT_CONTEXT_ID::kBasicMenuNav;

			const auto scaleform = RE::BSScaleformManager::GetSingleton();
			// Empty path: find the menu clip ourselves below instead of
			// assuming the game's usual "root1.Menu_mc".
			if (!scaleform || !scaleform->LoadMovieEx(*this, "Interface/FourStimPickerMenu.swf"sv, ""sv)) {
				REX::WARN("Picker: couldn't load Interface/FourStimPickerMenu.swf");
				return;
			}
			LinkMenuObject();
			// Send the list right away rather than waiting for the menu to ask
			// (its RequestScenes call remains as a backup).
			SendScenes();
		}

		// Finds the AS3 menu clip and attaches BGSCodeObj to it. The game's
		// menus (built in Adobe's Flash tool) live at "root1.Menu_mc"; a
		// Flex-compiled movie's root may be named differently, so try the
		// likely paths and log which one resolved.
		void LinkMenuObject()
		{
			constexpr std::array paths{ "root1.Menu_mc", "root.Menu_mc", "Menu_mc", "_root.Menu_mc", "root1", "root" };
			for (const auto path : paths) {
				Scaleform::GFx::Value candidate;
				if (uiMovie->GetVariable(&candidate, path) && candidate.IsObject()) {
					menuObj = candidate;
					RegisterCodeObject(*uiMovie, menuObj);
					REX::INFO("Picker: menu clip found at \"{}\"", path);
					return;
				}
			}
			REX::WARN("Picker: couldn't find the menu clip (tried root1.Menu_mc, root.Menu_mc, Menu_mc, _root.Menu_mc, root1, root)");
		}

		// Menu navigation can arrive marked disabled: keyboard Enter always does
		// in this menu, and during a scene the player-controls layer blocks
		// menu actions. Accept them anyway, so IMenu passes their real names
		// on to the AS3 side.
		static bool IsNavAction(const RE::BSFixedString& a_action)
		{
			return a_action == "Accept"sv || a_action == "Cancel"sv || a_action == "Up"sv || a_action == "Down"sv ||
			       a_action == "Left"sv || a_action == "Right"sv;
		}

		bool CanHandleWhenDisabled(const RE::ButtonEvent* a_event) override
		{
			return a_event && IsNavAction(a_event->QRawUserEvent());
		}

		// Hold-to-scroll: menus only receive a key's press and release, so the
		// input hook tracks which navigation key is held (g_navHeld) and this
		// per-frame update sends AS3 a fresh Up/Down every REPEAT_RATE seconds
		// after an initial REPEAT_DELAY. Real-time clock: game time is frozen
		// while the picker pauses the game.
		void AdvanceMovie(float a_timeDelta, std::uint64_t a_time) override
		{
			RE::IMenu::AdvanceMovie(a_timeDelta, a_time);

			const int dir = g_navHeld;
			if (dir == 0 || !menuObj.IsObject()) {
				return;
			}
			constexpr double REPEAT_DELAY = 0.35;
			constexpr double REPEAT_RATE = 0.06;
			const double held = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_navHeldSince.load()).count();
			const int    due = held < REPEAT_DELAY ? 0 : static_cast<int>((held - REPEAT_DELAY) / REPEAT_RATE) + 1;
			const auto   press = g_navPressCount.load();
			if (press != _repeatPress) {  // a new press: start counting again
				_repeatPress = press;
				_repeatSteps = 0;
			}
			for (int i = 0; _repeatSteps < due && i < 3; ++i) {  // at most 3 per frame, so a hitch can't jump far
				++_repeatSteps;
				Scaleform::GFx::Value args[2];
				args[0] = dir < 0 ? "Up" : "Down";
				args[1] = true;
				menuObj.Invoke("ProcessUserEvent", nullptr, args, 2);
			}
		}

		void MapCodeObjectFunctions() override
		{
			MapCodeMethodToASFunction("RequestScenes", 0);
			MapCodeMethodToASFunction("PickScene", 1);
			MapCodeMethodToASFunction("CloseMenu", 2);
		}

		void Call(const Params& a_params) override
		{
			switch (reinterpret_cast<std::uintptr_t>(a_params.userData)) {
			case 0:
				SendScenes();
				break;
			case 1:
				if (a_params.argCount > 0 && a_params.args[0].IsString()) {
					StartPicked(a_params.args[0].GetString());
				}
				break;
			case 2:
				Close();
				break;
			default:
				break;
			}
		}

	private:
		int           _repeatSteps = 0;  // hold-to-scroll repeats sent for the current hold
		std::uint32_t _repeatPress = 0;  // which press _repeatSteps belongs to

		static void Close()
		{
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kHide);
			}
		}

		void SendScenes()
		{
			if (!uiMovie || !menuObj.IsObject()) {
				return;
			}
			const auto mode = g_pickerMode.load();
			if (mode == PickerMode::kNavigate) {
				SendNavigation();
				return;
			}
			const bool  search = mode == PickerMode::kSearch;
			const auto  focusedIDs = GetPlayerScene().ActorIDs();
			const auto  focusedCount = focusedIDs.size();
			const bool  solo = search ? focusedCount == 1 : g_pickerTarget == 0;
			const auto  actorCount = search ? focusedCount : solo ? 1 : 2;
			// Only what these actors can play: a running scene's in its role
			// order, a new one's in any.
			std::vector<std::uint32_t> ids = focusedIDs;
			if (!search) {
				ids = { RE::PlayerCharacter::GetSingleton()->GetFormID() };
				if (!solo) {
					ids.push_back(g_pickerTarget);
				}
			}
			const auto filter = SexFilter(ids, search);
			// Sequences first, as "Sequence: <name>" with SEQUENCE_PREFIX on
			// the ID, then the scenes.
			auto scenes = SceneRegistry::ListSequences(actorCount, filter);
			for (auto& seq : scenes) {
				seq.id = std::string(SEQUENCE_PREFIX) + seq.id;
				seq.name = "Sequence: " + seq.name;
				seq.tags = seq.tags.empty() ? "sequence" : "sequence, " + seq.tags;
			}
			std::ranges::move(SceneRegistry::List(actorCount, filter), std::back_inserter(scenes));

			// Furniture: a running scene stays on what it's on (or off
			// furniture); a new one can use what's near the player.
			const auto focusedFurniture = GetPlayerScene().furniture;
			const auto found = search ? std::vector<Furniture::Found>{} : PickerFurniture();
			std::erase_if(scenes, [&](SceneRegistry::SceneSummary& a_scene) {
				if (search) {
					return !FurnitureFits(a_scene.furniture, focusedFurniture);
				}
				if (a_scene.furniture.empty()) {
					return false;
				}
				const auto piece = FurnitureFor(found, a_scene.furniture);
				if (!piece) {
					return true;
				}
				a_scene.tags = a_scene.tags.empty() ? piece->name : piece->name + ", " + a_scene.tags;
				return false;
			});
			std::size_t sequencesLeft = 0;
			for (const auto& scene : scenes) {
				sequencesLeft += scene.id.starts_with(SEQUENCE_PREFIX) ? 1 : 0;
			}

			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			for (const auto& scene : scenes) {
				Scaleform::GFx::Value entry;
				uiMovie->CreateObject(&entry);
				Scaleform::GFx::Value id, name, tags;
				id = scene.id.c_str();
				name = scene.name.c_str();
				tags = scene.tags.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			}

			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = search ? "Change scene" : solo ? "Solo scenes" : "Scenes with your partner";
			menuObj.Invoke("SetScenes", nullptr, args, 2);
			REX::INFO("Picker: sent {} scene(s) and {} sequence(s) ({}{}), {} furniture piece(s) near", scenes.size() - sequencesLeft, sequencesLeft, search ? "search, " : "", solo ? "solo" : "pair", found.size());
		}

		// Navigation mode: the current scene's links, then "End scene".
		void SendNavigation()
		{
			const auto current = GetPlayerScene();
			const auto scene = current.role0 ? SceneRegistry::Find(current.sceneID) : nullptr;

			Scaleform::GFx::Value list;
			uiMovie->CreateArray(&list);
			auto add = [&](const std::string& a_id, const std::string& a_name, const std::string& a_detail) {
				Scaleform::GFx::Value entry, id, name, tags;
				uiMovie->CreateObject(&entry);
				id = a_id.c_str();
				name = a_name.c_str();
				tags = a_detail.c_str();
				entry.SetMember("id"sv, id);
				entry.SetMember("name"sv, name);
				entry.SetMember("tags"sv, tags);
				list.PushBack(entry);
			};
			std::string title = "Scene";
			if (scene) {
				// During a transition, the options of where it's going.
				const auto settled = SceneRegistry::Settled(scene);
				for (const auto& nav : settled->navigations) {
					const auto dest = SceneRegistry::Find(nav.to);
					if (dest && (!SexesFit(*dest, current.ActorIDs()) || !FurnitureFits(dest->furniture, current.furniture))) {
						continue;
					}
					add(nav.to, FormatNavLabel(nav.label, current), dest ? dest->name : nav.to);
				}
				title = scene->name + "   (speed " + std::to_string(current.speed + 1) + "/" + std::to_string(scene->speeds.size()) + ")";
			}
			add(END_SCENE_ID, "End scene", "");

			Scaleform::GFx::Value args[2];
			args[0] = list;
			args[1] = title.c_str();
			menuObj.Invoke("SetScenes", nullptr, args, 2);
			REX::INFO("Picker: sent {} navigation option(s)", scene ? SceneRegistry::Settled(scene)->navigations.size() : 0);
		}

		static void StartPicked(std::string a_sceneID)
		{
			REX::INFO("Picker: picked \"{}\"", a_sceneID);
			Close();
			std::vector<SceneRegistry::SceneActor> roles;  // what its roles ask for
			std::string                            furnitureType;  // what it's played on
			if (a_sceneID.starts_with(SEQUENCE_PREFIX)) {
				const auto sequence = SceneRegistry::FindSequence(std::string_view(a_sceneID).substr(SEQUENCE_PREFIX.size()));
				if (!sequence) {
					return;
				}
				roles = sequence->actors;
				furnitureType = sequence->furniture;
				if (g_pickerMode == PickerMode::kSearch) {
					// On the running scene.
					F4SE::GetTaskInterface()->AddTask([sequence]() { StartSequenceOnScene(GetPlayerScene().role0, sequence); });
					return;
				}
				// Start its first scene the usual way; the sequence attaches
				// when that scene starts.
				const auto player = RE::PlayerCharacter::GetSingleton();
				std::vector<std::uint32_t> actors{ player->GetFormID() };
				if (g_pickerTarget != 0) {
					actors.push_back(g_pickerTarget);
				}
				QueueSequence(sequence, std::move(actors));
				a_sceneID = sequence->entries.front().scene;
			}
			if (g_pickerMode == PickerMode::kSearch) {
				NavigateSceneImpl(a_sceneID);
				return;
			}
			if (g_pickerMode == PickerMode::kNavigate) {
				if (a_sceneID == END_SCENE_ID) {
					if (g_vm) {
						RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
						g_vm->DispatchStaticCall("FourStimMenu"sv, "EndPlayerScene"sv, callback);
					}
				} else {
					NavigateSceneImpl(a_sceneID);
				}
				return;
			}
			// On furniture: the nearest piece it fits, for when it starts.
			{
				if (furnitureType.empty() && !a_sceneID.starts_with(SEQUENCE_PREFIX)) {
					if (const auto scene = SceneRegistry::Find(a_sceneID)) {
						furnitureType = scene->furniture;
					}
				}
				std::scoped_lock lock(g_pendingFurnitureLock);
				g_pendingFurniture.reset();
				if (!furnitureType.empty()) {
					const auto found = PickerFurniture();
					if (const auto piece = FurnitureFor(found, furnitureType)) {
						PendingFurniture pending{ piece->ref, piece->type, { RE::PlayerCharacter::GetSingleton()->GetFormID() } };
						if (const auto scene = SceneRegistry::Find(a_sceneID)) {
							pending.offset = scene->furnitureOffset;
						}
						if (const auto target = g_pickerTarget.load(); target != 0) {
							pending.actors.push_back(target);
						}
						std::ranges::sort(pending.actors);
						REX::INFO("Picker: \"{}\" goes on {:08X} ({}, {:.0f} away)", a_sceneID, piece->ref, piece->type, piece->distance);
						g_pendingFurniture = std::move(pending);
					} else {
						REX::WARN("Picker: no {} near for \"{}\"", furnitureType, a_sceneID);
					}
				}
			}

			// Who takes which role: the player first unless only the other
			// way round fits the scene (e.g. a female player and a male
			// partner in a scene whose first role is male).
			bool targetFirst = false;
			if (const auto target = g_pickerTarget.load(); target != 0) {
				if (roles.empty()) {
					if (const auto scene = SceneRegistry::Find(a_sceneID)) {
						roles = scene->actors;
					}
				}
				const auto order = SceneRegistry::AssignRoles(roles, SexesOf({ RE::PlayerCharacter::GetSingleton()->GetFormID(), target }));
				targetFirst = !order.empty() && order.front() == 1;
			}
			if (g_vm) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
				g_vm->DispatchStaticCall("FourStimMenu"sv, "StartPickedScene"sv, callback,
					std::move(a_sceneID), static_cast<std::int32_t>(g_pickerTarget.load()), targetFirst);
			}
		}
	};

	void ShowPicker(PickerMode a_mode)
	{
		g_pickerMode = a_mode;
		if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kShow);
		}
	}

	// The focused scene's controls: the HUD, focused for input, or, if the
	// HUD isn't available (turned off or its movie missing), the picker in
	// navigation mode (navigation links plus "End scene").
	void OpenSceneNavigation(std::monostate)
	{
		F4SE::GetTaskInterface()->AddTask([]() {
			if (HUD::IsOpen()) {
				HUD::SetFocus(true);
			} else {
				ShowPicker(PickerMode::kNavigate);
			}
		});
	}

	// Opens the picker for the player plus a_targetID (0 = solo scenes).
	void OpenScenePicker(std::monostate, std::int32_t a_targetID)
	{
		g_pickerMode = PickerMode::kStart;
		g_pickerTarget = static_cast<std::uint32_t>(a_targetID);
		// Look for furniture first (on the main thread: it walks the loaded
		// cells), then open the picker.
		F4SE::GetTaskInterface()->AddTask([]() {
			std::vector<Furniture::Found> found;
			if (const auto player = RE::PlayerCharacter::GetSingleton()) {
				Furniture::Reload();
				found = Furniture::FindNear(player->GetPosition(), g_settings.furnitureRadius, g_settings.furnitureHeight, g_settings.logFurniture);
				for (const auto& f : found) {
					REX::INFO("Furniture: nearest {}: {:08X}, {:.0f} away", f.type, f.ref, f.distance);
				}
			}
			{
				std::scoped_lock lock(g_pickerFurnitureLock);
				g_pickerFurniture = std::move(found);
			}
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kShow);
			}
		});
	}

	// Returns the form ID of the actor currently selected in the console, or
	// 0 if nothing (or a non-actor) is selected. Papyrus turns it back into
	// an Actor with Game.GetForm().
	//
	// Deliberately returns an int, not an Actor: returning an object from a
	// native goes through the VM's CreateObject on this build, which crashed
	// the game (null read inside CreateObject). Receiving actors as
	// parameters works fine; only returning them is affected.
	std::int32_t GetSelectedActorID(std::monostate)
	{
		const auto ref = RE::Console::GetPickRef().get();
		const auto actor = ref ? ref->As<RE::Actor>() : nullptr;
		return actor ? static_cast<std::int32_t>(actor->GetFormID()) : 0;
	}

	// Form ID of the actor in the given role (0 or 1) of the scene the
	// player is in, or 0 if the player isn't in a scene / the role is empty.
	std::int32_t GetPlayerSceneActorID(std::monostate, std::int32_t a_role)
	{
		const auto scene = GetPlayerScene();
		return static_cast<std::int32_t>(a_role == 0 ? scene.role0 : a_role == 1 ? scene.role1 : 0);
	}

	// ---- Hotkey ----
	// Hooks MenuControls::PerformInputProcessing (vtable slot 0, from its
	// BSInputEventReceiver base), which receives the whole input queue every
	// frame during normal play. Registering a BSInputEventUser with
	// MenuControls::RegisterHandler succeeded but never received any input.

	void HandleButton(const RE::ButtonEvent* a_event)
	{
		// Hold-to-scroll tracking for the picker: arrow keys and d-pad up/down.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
			int        dir = 0;
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				dir = code == 0x26 ? -1 : code == 0x28 ? 1 : 0;  // arrow up / down
			} else if (device == RE::INPUT_DEVICE::kGamepad) {
				dir = code == 0x1 ? -1 : code == 0x2 ? 1 : 0;  // d-pad up / down
			}
			if (dir != 0) {
				if (a_event->QJustPressed()) {
					g_navHeldSince = std::chrono::steady_clock::now();
					++g_navPressCount;
					g_navHeld = dir;
				} else if (!a_event->QPressed() && g_navHeld == dir) {
					g_navHeld = 0;
				}
			}
		} else {
			g_navHeld = 0;
		}

		// The HUD, while focused, gets the navigation keys (both press and
		// release), unless the console, a pausing menu or the picker is up.
		if (const auto ui = RE::UI::GetSingleton(); HUD::IsFocused() && ui && !ui->GetMenuOpen(PICKER_MENU) &&
													ui->menuMode == 0 && !ui->GetMenuOpen("Console")) {
			if (HUD::HandleInput(a_event)) {
				return;
			}
		}

		if (!a_event->QJustPressed()) {
			return;
		}

		// Escape hatch: Esc (or the controller's B) always closes the picker,
		// whatever its AS3 side is doing.
		if (const auto ui = RE::UI::GetSingleton(); ui && ui->GetMenuOpen(PICKER_MENU)) {
			const auto code = static_cast<std::uint32_t>(a_event->idCode);
			const auto device = a_event->device.get();
			if ((device == RE::INPUT_DEVICE::kKeyboard && code == 0x1B) || (device == RE::INPUT_DEVICE::kGamepad && code == 0x2000)) {
				if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
					queue->AddMessage(PICKER_MENU, RE::UI_MESSAGE_TYPE::kHide);
				}
			}
			return;
		}

		// Ignore the hotkey while the console or a game-pausing menu is open.
		if (const auto ui = RE::UI::GetSingleton(); ui && (ui->menuMode > 0 || ui->GetMenuOpen("Console"))) {
			return;
		}

		// Keyboard codes are Windows virtual-key codes (N = 0x4E).
		if (a_event->device.get() != RE::INPUT_DEVICE::kKeyboard) {
			return;
		}

		// Speed keys, while the player is in a scene.
		if (const auto code = static_cast<std::uint32_t>(a_event->idCode); code == g_settings.speedUpKey || code == g_settings.speedDownKey) {
			if (GetPlayerScene().role0 != 0) {
				ChangeSceneSpeedImpl(code == g_settings.speedUpKey ? 1 : -1);
			}
			return;
		}
		if (static_cast<std::uint32_t>(a_event->idCode) != g_settings.hotkey) {
			return;
		}
		// In a scene with the HUD up, the hotkey switches the HUD's focus.
		if (GetPlayerScene().Active() && HUD::IsOpen()) {
			HUD::SetFocus(!HUD::IsFocused());
			return;
		}
		if (!g_vm) {
			return;
		}
		REX::INFO("Hotkey pressed");
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		g_vm->DispatchStaticCall("FourStimMenu"sv, "OnHotkey"sv, callback,
			g_settings.targetMode, g_settings.maxDistance, g_settings.crosshairCone, g_settings.proximityRadius);
	}

	struct InputHook
	{
		using func_t = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
		static inline func_t original = nullptr;

		static void Thunk(RE::BSInputEventReceiver* a_this, const RE::InputEvent* a_queueHead)
		{
			for (auto event = a_queueHead; event; event = event->next) {
				if (const auto button = event->As<RE::ButtonEvent>()) {
					HandleButton(button);
				}
			}
			original(a_this, a_queueHead);  // always let the game process the input too
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE::MenuControls[0] };
			original = reinterpret_cast<func_t>(vtbl.write_vfunc(0, Thunk));
			REX::INFO("Input hook installed");
		}
	};

	// ---- Animation events of scene actors ----
	// A transition idle played with the one-shot dyn_Activation event sends
	// "IdleStop" when its clip ends (and the actor starts blending back to
	// its base pose). If the timer hasn't moved the scene on by then, this
	// does, at once. With bLogAnimEvents=1, every event a scene actor gets
	// in the 5 s after a scene change is logged too (for animation authors).
	// Hooks ProcessEvent of the BSTEventSink<BSAnimationGraphEvent> base
	// (vtable 3, at +0x38) of Actor and PlayerCharacter.

	struct AnimWatch
	{
		std::string                           sceneID;
		std::chrono::steady_clock::time_point start;
	};
	std::mutex                                    g_animWatchLock;
	std::unordered_map<std::uint32_t, AnimWatch>  g_animWatch;

	void WatchAnimEvents(const std::vector<std::uint32_t>& a_actors, const std::string& a_sceneID)
	{
		std::scoped_lock lock(g_animWatchLock);
		const auto now = std::chrono::steady_clock::now();
		for (const auto id : a_actors) {
			g_animWatch[id] = { a_sceneID, now };
		}
	}

	struct AnimEventHook
	{
		using func_t = RE::BSEventNotifyControl (*)(RE::BSTEventSink<RE::BSAnimationGraphEvent>*, const RE::BSAnimationGraphEvent&, RE::BSTEventSource<RE::BSAnimationGraphEvent>*);
		static inline func_t originalActor = nullptr;
		static inline func_t originalPlayer = nullptr;

		static void Log(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event)
		{
			const auto ref = reinterpret_cast<RE::TESObjectREFR*>(reinterpret_cast<std::uintptr_t>(a_this) - 0x38);
			const auto id = ref->GetFormID();
			std::string sceneID;
			float       seconds = 0.0F;
			{
				std::scoped_lock lock(g_animWatchLock);
				const auto it = g_animWatch.find(id);
				if (it == g_animWatch.end()) {
					return;
				}
				seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - it->second.start).count();
				if (seconds > 60.0F) {  // long enough for any transition
					g_animWatch.erase(it);
					return;
				}
				sceneID = it->second.sceneID;
			}
			if (g_settings.logAnimEvents && seconds <= 5.0F) {
				REX::INFO("AnimEvent: {:08X} +{:.2f}s in \"{}\": \"{}\" ({})", id, seconds, sceneID,
					a_event.tag.c_str(), a_event.payload.c_str());
			}
			if (_stricmp(a_event.tag.c_str(), "IdleStop") == 0) {
				// Animation threads: hand it to the main thread.
				F4SE::GetTaskInterface()->AddTask([id, sceneID]() {
					const auto active = FindActiveScene(id);
					if (!active || active->remaining < 0.0F || _stricmp(active->sceneID.c_str(), sceneID.c_str()) != 0) {
						return;
					}
					const auto scene = SceneRegistry::Find(active->sceneID);
					if (!scene || !scene->IsTransition()) {
						return;
					}
					REX::INFO("Transition \"{}\": clip ended ({:.2f}s before the timer), moving on now", scene->id, active->remaining);
					active->remaining = -1.0F;
					AdvanceAutoplay(*active);
				});
			}
		}

		static RE::BSEventNotifyControl ThunkActor(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			Log(a_this, a_event);
			return originalActor(a_this, a_event, a_source);
		}

		static RE::BSEventNotifyControl ThunkPlayer(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, const RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
		{
			Log(a_this, a_event);
			return originalPlayer(a_this, a_event, a_source);
		}

		// After game data loads (the player exists then): checks the player's
		// own vtable pointer at +0x38 against the one to patch first, so a
		// wrong table index can't patch the wrong function.
		static void Install()
		{
			static bool installed = false;
			if (installed) {
				return;
			}
			installed = true;
			REL::Relocation<std::uintptr_t> check{ RE::VTABLE::PlayerCharacter[3] };
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto actual = player ? *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(player) + 0x38) : 0;
			if (actual != check.address()) {
				REX::WARN("Animation event log: vtable check failed ({:X} vs {:X}), not installed", actual, check.address());
				return;
			}
			REL::Relocation<std::uintptr_t> actorVtbl{ RE::VTABLE::Actor[3] };
			originalActor = reinterpret_cast<func_t>(actorVtbl.write_vfunc(1, ThunkActor));
			REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE::PlayerCharacter[3] };
			originalPlayer = reinterpret_cast<func_t>(playerVtbl.write_vfunc(1, ThunkPlayer));
			REX::INFO("Animation event log hook installed");
		}
	};

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		g_vm = a_vm;

		REX::INFO("RegisterPapyrusFunctions called, binding {}", SCRIPT_NAME);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartScene"sv, StartScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopScene"sv, StopScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SuppressInteraction"sv, SuppressInteraction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RestoreInteraction"sv, RestoreInteraction);
		a_vm->BindNativeMethod(SCRIPT_NAME, "LockActor"sv, LockActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnlockActor"sv, UnlockActor);
		a_vm->BindNativeMethod(SCRIPT_NAME, "MoveActorTo"sv, MoveActorTo);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSelectedActorID"sv, GetSelectedActorID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlacePair"sv, PlacePair);
		a_vm->BindNativeMethod(SCRIPT_NAME, "IgnorePairCollision"sv, IgnorePairCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RestoreCollision"sv, RestoreCollision);
		a_vm->BindNativeMethod(SCRIPT_NAME, "PlaySceneIdles"sv, PlaySceneIdles);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetSceneActorCount"sv, GetSceneActorCount);
		a_vm->BindNativeMethod(SCRIPT_NAME, "QueueSequence"sv, QueueSequenceNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StartSequenceOnScene"sv, StartSequenceOnSceneNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopSequence"sv, StopSequenceNative);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ReloadScenes"sv, ReloadScenes);
		a_vm->BindNativeMethod(SCRIPT_NAME, "StopPair"sv, StopPair);
		a_vm->BindNativeMethod(SCRIPT_NAME, "SaveStartView"sv, SaveStartView);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenScenePicker"sv, OpenScenePicker);
		a_vm->BindNativeMethod(SCRIPT_NAME, "OpenSceneNavigation"sv, OpenSceneNavigation);
		a_vm->BindNativeMethod(SCRIPT_NAME, "ChangeSceneSpeed"sv, ChangeSceneSpeed);
		a_vm->BindNativeMethod(SCRIPT_NAME, "NavigateScene"sv, NavigateScene);
		a_vm->BindNativeMethod(SCRIPT_NAME, "BeginSceneCamera"sv, BeginSceneCamera);
		a_vm->BindNativeMethod(SCRIPT_NAME, "EndSceneCamera"sv, EndSceneCamera);
		a_vm->BindNativeMethod(SCRIPT_NAME, "LockPlayerControls"sv, LockPlayerControls);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnlockPlayerControls"sv, UnlockPlayerControls);
		a_vm->BindNativeMethod(SCRIPT_NAME, "GetPlayerSceneActorID"sv, GetPlayerSceneActorID);
		a_vm->BindNativeMethod(SCRIPT_NAME, "RegisterForSceneEvents"sv, RegisterForSceneEvents);
		a_vm->BindNativeMethod(SCRIPT_NAME, "UnregisterForSceneEvents"sv, UnregisterForSceneEvents);
		REX::INFO("Papyrus functions bound");
		return true;
	}

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kPostLoad:
			REX::INFO("kPostLoad");
			break;
		case F4SE::MessagingInterface::kPreLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			// Scene-event registrations aren't saved: scripts register again on
			// load. Scenes don't survive loading either, so the HUD goes too.
			SceneEvents::Clear();
			SetPlayerScene({});
			HUD::Reset();
			// Running scenes and their timers end with the old game.
			F4SE::GetTaskInterface()->AddTask([]() {
				g_activeScenes.clear();
			});
			{
				std::scoped_lock lock(g_pendingSequenceLock);
				g_pendingSequence.reset();
			}
			break;
		case F4SE::MessagingInterface::kGameLoaded:
			{
				REX::INFO("kGameLoaded");
				AnimEventHook::Install();
				static bool menuRegistered = false;
				if (!menuRegistered) {
					if (const auto ui = RE::UI::GetSingleton()) {
						ui->RegisterMenu(PICKER_MENU, PickerMenu::Create);
						HUD::RegisterMenu();
						menuRegistered = true;
						REX::INFO("Picker menu registered");
					}
				}
				HUD::LoadConfig();
				// A loaded save starts with no scene running.
				HUD::Reset();
				g_activeScenes.clear();
				g_focusNextScene = false;
				SetPlayerScene({});
				g_sceneCameraActive = false;
				g_startViewSaved = false;
				RestoreFreeCameraSpeed();
				ReleaseSceneLayer();
			}
			break;
		default:
			break;
		}
	}
}

// ---- Bridge.h: what the HUD and scene events use ----

namespace FourStim
{
	RE::BSScript::IVirtualMachine* GetVM()
	{
		return g_vm;
	}

	FocusedScene GetFocusedScene()
	{
		return GetPlayerScene();
	}

	int ChangeFocusedSpeed(int a_delta)
	{
		return ChangeSceneSpeedImpl(a_delta);
	}

	bool NavigateFocused(const std::string& a_sceneID)
	{
		if (a_sceneID == END_SCENE_ID) {
			EndFocusedScene();
			return true;
		}
		return NavigateSceneImpl(a_sceneID);
	}

	void EndFocusedScene()
	{
		if (g_vm) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			g_vm->DispatchStaticCall("FourStimMenu"sv, "EndPlayerScene"sv, callback);
		}
	}

	void OpenSearchForFocused()
	{
		if (GetPlayerScene().Active()) {
			ShowPicker(PickerMode::kSearch);
		}
	}

	std::string FormatLabel(std::string a_label, const FocusedScene& a_scene)
	{
		return FormatNavLabel(std::move(a_label), a_scene);
	}

	bool FocusedCanPlay(const SceneRegistry::Scene& a_scene)
	{
		const auto focused = GetPlayerScene();
		return SexesFit(a_scene, focused.ActorIDs()) && FurnitureFits(a_scene.furniture, focused.furniture);
	}

	std::vector<RE::Actor*> ResolveActors(const std::vector<std::uint32_t>& a_ids)
	{
		std::vector<RE::Actor*> actors;
		actors.reserve(a_ids.size());
		for (const auto id : a_ids) {
			actors.push_back(RE::TESForm::GetFormByID<RE::Actor>(id));
		}
		return actors;
	}
}

F4SE_PLUGIN_PRELOAD(const F4SE::PreLoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	REX::INFO("4Stim preloaded");

	return true;
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	LoadSettings();
	HUD::SetOptions(g_settings.hudTheme, g_settings.hudEnabled);
	InputHook::Install();
	F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
	F4SE::GetPapyrusInterface()->Register(RegisterPapyrusFunctions);

	REX::INFO("4Stim loaded");

	return true;
}
