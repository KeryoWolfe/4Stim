#pragma once

#include <chrono>
#include <functional>

// One thread for every "a moment later" in 4Stim (roadmap 2b.5, a modder's
// review): before, each delay started its own detached thread that slept and
// then queued a task, 25 of them, several every scene, each with its own stack.
// Now they wait in one queue on one thread.
//
// Tasks given to After run on the scheduler thread: keep them short and never
// block (no waiting on futures, no device or file calls), or every other
// delay waits behind them. Game work goes through AfterOnMain, which hands
// the task to F4SE's task interface when it's due (the game's main thread,
// as the old threads did).
namespace Scheduler
{
	using Clock = std::chrono::steady_clock;
	using Task = std::function<void()>;

	// a_task on the scheduler thread after a_delay.
	void After(Clock::duration a_delay, Task a_task);

	// a_task on the game's main thread after a_delay.
	void AfterOnMain(Clock::duration a_delay, Task a_task);

	// Checks a_condition every a_interval (on the scheduler thread, as the old
	// polling threads did) while it's true, at most a_maxChecks times, then
	// runs a_then on the game's main thread after a_settle. For "wait until
	// the loading screen is gone, then...".
	void WhileThenOnMain(Clock::duration a_interval, int a_maxChecks, std::function<bool()> a_condition, Clock::duration a_settle, Task a_then);

	// a_task on the scheduler thread every a_interval while it returns true.
	void Every(Clock::duration a_interval, std::function<bool()> a_task);

	// a_task on the worker thread, now: for work that may block (sex toy
	// plugins' device calls, reading a file) and so mustn't hold up the
	// scheduler or the game. One worker, first in first out: calls to the
	// same device keep their order (on, update, peak, off), where a thread
	// each could overtake one another.
	void Work(Task a_task);
}
