#pragma once

// 4Stim's C++ interface for other F4SE plugins (docs/API.md), as OStim NG's
// PluginInterface (skse/src/PluginInterface): copy this one header into your
// plugin. No 4Stim code or libraries are needed to build against it.
//
// Getting the interface map, at F4SE's kPostLoad message or later (OStim:
// SKSE's kPostLoad):
//
//     FourStimAPI::InterfaceExchangeMessage message;
//     F4SE::GetMessagingInterface()->Dispatch(FourStimAPI::InterfaceExchangeMessage::MESSAGE_TYPE,
//         &message, sizeof(message), "4Stim");
//     if (message.interfaceMap) {
//         auto threads = static_cast<FourStimAPI::ThreadInterface*>(
//             message.interfaceMap->queryInterface(FourStimAPI::ThreadInterface::NAME));
//     }
//
// Departures from OStim, which Fallout 4 doesn't force but 4Stim's code does:
// - The namespace is FourStimAPI and the message type '4ST' (OStim: OStim,
//   'OST'), so a plugin can talk to both.
// - Scenes are passed by ID (const char*), not as OStim's Graph::Node
//   objects; scene data is read through FourStim's Papyrus functions or the
//   scene files. OStim's GraphInterface, FurnitureInterface and
//   SettingInterface aren't here; 4Stim adds an "Alignment" interface.
// - ThreadActor has no undressPartial / redressPartial (4Stim undresses
//   whole items, docs/UNDRESS.md).
//
// Every function is called on the game's main thread unless it says
// otherwise, and listeners are called on it. Pointers 4Stim hands out
// (Thread*, ThreadActor*) stay valid for the whole game session; a stopped
// thread's getThreadID still answers, but its actors are gone.

#include <cstdint>

namespace FourStimAPI
{
	// ---- Base ----

	class PluginInterface
	{
	public:
		PluginInterface() {}
		virtual ~PluginInterface() {}

		virtual std::uint32_t getVersion() = 0;
	};

	class InterfaceMap
	{
	public:
		virtual PluginInterface* queryInterface(const char* name) = 0;
		virtual bool             addInterface(const char* name, PluginInterface* pluginInterface) = 0;
		virtual PluginInterface* removeInterface(const char* name) = 0;
	};

	struct InterfaceExchangeMessage
	{
		enum : std::uint32_t
		{
			MESSAGE_TYPE = '4ST'
		};

		InterfaceMap* interfaceMap = nullptr;
	};

	// ---- Threads (OStim's ThreadInterface) ----

	class ThreadActor
	{
	public:
		// --- ABI version 1 ---
		// cast this to RE::Actor*
		virtual void* getGameActor() = 0;

		virtual void undress() = 0;
		virtual void removeWeapons() = 0;
		virtual void redress() = 0;
		virtual void addWeapons() = 0;

		// 4Stim: excitement (0-100) and climaxing.
		virtual float getExcitement() = 0;
		virtual void  setExcitement(float excitement) = 0;
		virtual void  climax() = 0;
	};

	class ThreadActorVisitor
	{
	public:
		// return false to stop visiting
		virtual bool visit(ThreadActor* actor) = 0;
	};

	class Thread
	{
	public:
		// --- ABI version 1 ---
		virtual std::int32_t getThreadID() = 0;
		virtual bool         isPlayerThread() = 0;
		// false once it has stopped
		virtual bool isRunning() = 0;

		virtual std::uint32_t getActorCount() = 0;
		virtual ThreadActor*  getActor(std::uint32_t position) = 0;
		virtual void          forEachThreadActor(ThreadActorVisitor* visitor) = 0;

		// The scene playing (OStim: getCurrentNode()->getNodeID()), lowercase.
		virtual const char*  getCurrentSceneID() = 0;
		virtual std::int32_t getSpeed() = 0;  // 0-based

		// "" when not on furniture (OStim: a FurnitureType object)
		virtual const char* getFurnitureType() = 0;
		// cast this to RE::TESObjectREFR*, nullptr if none
		virtual void* getFurnitureObject() = 0;

		// Changes, done on the main thread a moment later.
		virtual void navigateTo(const char* sceneID) = 0;
		virtual void warpTo(const char* sceneID) = 0;
		virtual void setSpeed(std::int32_t speed) = 0;
		virtual void stop() = 0;
	};

	class ThreadEventListener
	{
	public:
		virtual void listen(Thread* thread) = 0;
	};

	class ThreadActorEventListener
	{
	public:
		virtual void listen(Thread* thread, ThreadActor* actor) = 0;
	};

	class ThreadBuilder
	{
	public:
		// --- ABI version 1 ---
		// actors are RE::Actor*
		virtual void         setDominantActors(std::uint32_t actorCount, void** actors) = 0;
		virtual void         setFurniture(void* furniture) = 0;  // RE::TESObjectREFR*
		virtual void         setDuration(float seconds) = 0;     // OStim: int32_t
		virtual void         setStartingScene(const char* sceneID) = 0;
		virtual void         addStartingScene(const char* sceneID, float duration, bool navigateTo) = 0;
		virtual void         setStartingSequence(const char* sequence) = 0;
		virtual void         concatStartingSequence(const char* sequence, bool navigateTo) = 0;
		virtual void         endAfterSequence() = 0;
		virtual void         undressActors() = 0;
		virtual void         noAutoMode() = 0;
		virtual void         noPlayerControl() = 0;
		virtual void         noUndressing() = 0;
		virtual void         noFurniture() = 0;
		virtual void         setMetadataCSV(const char* metadata) = 0;
		// The thread ID it will have (0 = the player's), or -1. The builder is
		// gone after start or cancel.
		virtual std::int32_t start() = 0;
		virtual void         cancel() = 0;
	};

	class ThreadInterface : public PluginInterface
	{
	public:
		inline static const char* NAME = "Threads";

		// --- ABI version 1 ---
		// nullptr if no thread with that ID is running
		virtual Thread* getThread(std::int32_t threadID) = 0;

		virtual void registerThreadStartListener(ThreadEventListener* listener) = 0;
		virtual void registerSpeedChangedListener(ThreadEventListener* listener) = 0;
		virtual void registerNodeChangedListener(ThreadEventListener* listener) = 0;  // the scene changed
		virtual void registerClimaxListener(ThreadActorEventListener* listener) = 0;
		virtual void registerThreadStopListener(ThreadEventListener* listener) = 0;

		// actors are RE::Actor*
		virtual ThreadBuilder* createThreadBuilder(std::uint32_t actorCount, void** actors) = 0;

		// the actor (RE::Actor*) in a running thread, or nullptr
		virtual ThreadActor* getActor(void* gameActor) = 0;
	};

	// ---- Alignment (4Stim's own; OStim has none) ----

	struct AlignmentOffset
	{
		float x = 0.0F;      // sideways
		float y = 0.0F;      // forward
		float z = 0.0F;      // up
		float rot = 0.0F;    // degrees
		float scale = 1.0F;
		float bend = 0.0F;   // penis bend steps, -9 to +9
	};

	class AlignmentInterface : public PluginInterface
	{
	public:
		inline static const char* NAME = "Alignment";

		// --- ABI version 1 ---
		// The offset of the actor at position in the thread's current scene,
		// for its current actors (docs/ALIGNMENT.md). False if there's no such
		// thread or position.
		virtual bool getOffset(std::int32_t threadID, std::uint32_t position, AlignmentOffset* offset) = 0;
		// Sets and saves it, applied at once (main thread).
		virtual bool setOffset(std::int32_t threadID, std::uint32_t position, const AlignmentOffset* offset) = 0;
		// Back to the scene's own.
		virtual bool resetOffset(std::int32_t threadID, std::uint32_t position) = 0;
	};

	// ---- Sex toys (OStim's SexToyInterface, the same) ----

	class SexToy
	{
	public:
		// unique; used for the toy's settings
		virtual const char* getID() = 0;
		// shown to the user
		virtual const char* getName() = 0;

		// milliseconds that must pass before another update; updates are
		// always sent on a peak; 0 = one per peak; without peaks an update is
		// sent every second
		virtual std::uint16_t getUpdateInterval() = 0;

		// called on scene start / end, from detached threads
		virtual void turnOn() = 0;
		virtual void turnOff() = 0;

		// baseline: 0-1 strength between peaks; peak: 0-1 strength of peaks
		// (filled even with peaks off); peakInterval: ms between peaks, -1 with
		// peaks off. From detached threads.
		virtual void update(float baseline, float peak, std::int16_t peakInterval) = 0;

		// a climax or other event; overrides earlier commands. From detached threads.
		virtual void peak(float magnitude, float duration) = 0;

		// abort the latest update (the scene changed and the toy isn't used in
		// the new one). From detached threads.
		virtual void stop() = 0;
	};

	class SexToyReloadListener
	{
	public:
		// called once on a save load and again every time the player's thread
		// starts; every toy registered before is dropped right before it, so
		// register them all each time (4Stim doesn't delete them). From a
		// detached thread.
		virtual void reloadToys() = 0;
	};

	class SexToyInterface : public PluginInterface
	{
	public:
		inline static const char* NAME = "SexToys";

		// --- ABI version 1 ---
		// once, at kPostLoad
		virtual void registerReloadListener(SexToyReloadListener* listener) = 0;
		// from reloadToys
		virtual void addToy(SexToy* toy) = 0;
	};
}
