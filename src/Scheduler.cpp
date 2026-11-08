#include "Scheduler.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Scheduler
{
	namespace
	{
		struct Entry
		{
			Clock::time_point due;
			std::uint64_t     order;  // same due time: first in, first out
			Task              task;
		};

		struct Later
		{
			bool operator()(const Entry& a_left, const Entry& a_right) const
			{
				return a_left.due != a_right.due ? a_left.due > a_right.due : a_left.order > a_right.order;
			}
		};

		std::mutex                                       g_lock;
		std::condition_variable                          g_wake;
		std::priority_queue<Entry, std::vector<Entry>, Later> g_queue;
		std::uint64_t                                    g_order = 0;
		std::once_flag                                   g_started;

		void Run()
		{
			std::unique_lock lock(g_lock);
			while (true) {
				if (g_queue.empty()) {
					g_wake.wait(lock);
					continue;
				}
				const auto due = g_queue.top().due;
				if (Clock::now() < due) {
					g_wake.wait_until(lock, due);  // or until something sooner is queued
					continue;
				}
				auto task = std::move(const_cast<Entry&>(g_queue.top()).task);
				g_queue.pop();
				lock.unlock();
				try {
					task();
				} catch (const std::exception& e) {
					REX::ERROR("Scheduler: a task threw: {}", e.what());
				} catch (...) {
					REX::ERROR("Scheduler: a task threw");
				}
				lock.lock();
			}
		}

		void Start()
		{
			std::call_once(g_started, []() { std::thread(Run).detach(); });
		}
	}

	void After(Clock::duration a_delay, Task a_task)
	{
		Start();
		{
			std::scoped_lock lock(g_lock);
			g_queue.push({ Clock::now() + a_delay, g_order++, std::move(a_task) });
		}
		g_wake.notify_one();
	}

	void AfterOnMain(Clock::duration a_delay, Task a_task)
	{
		After(a_delay, [task = std::move(a_task)]() mutable { F4SE::GetTaskInterface()->AddTask(std::move(task)); });
	}

	namespace
	{
		struct Poll
		{
			Clock::duration       interval;
			int                   left;
			std::function<bool()> condition;
			Clock::duration       settle;
			Task                  then;
		};

		// As the old loops: check, wait an interval while still true, then settle.
		void PollStep(const std::shared_ptr<Poll>& a_poll)
		{
			if (a_poll->left-- > 0 && a_poll->condition()) {
				After(a_poll->interval, [a_poll]() { PollStep(a_poll); });
				return;
			}
			AfterOnMain(a_poll->settle, std::move(a_poll->then));
		}

		void EveryStep(const std::shared_ptr<std::function<bool()>>& a_task, Clock::duration a_interval)
		{
			if ((*a_task)()) {
				After(a_interval, [a_task, a_interval]() { EveryStep(a_task, a_interval); });
			}
		}
	}

	void WhileThenOnMain(Clock::duration a_interval, int a_maxChecks, std::function<bool()> a_condition, Clock::duration a_settle, Task a_then)
	{
		auto poll = std::make_shared<Poll>(Poll{ a_interval, a_maxChecks, std::move(a_condition), a_settle, std::move(a_then) });
		After(Clock::duration::zero(), [poll]() { PollStep(poll); });
	}

	void Every(Clock::duration a_interval, std::function<bool()> a_task)
	{
		auto task = std::make_shared<std::function<bool()>>(std::move(a_task));
		After(a_interval, [task, a_interval]() { EveryStep(task, a_interval); });
	}

	namespace
	{
		std::mutex              g_workLock;
		std::condition_variable g_workWake;
		std::deque<Task>        g_work;
		std::once_flag          g_workStarted;

		void RunWork()
		{
			std::unique_lock lock(g_workLock);
			while (true) {
				g_workWake.wait(lock, []() { return !g_work.empty(); });
				auto task = std::move(g_work.front());
				g_work.pop_front();
				lock.unlock();
				try {
					task();
				} catch (const std::exception& e) {
					REX::ERROR("Scheduler: a worker task threw: {}", e.what());
				} catch (...) {
					REX::ERROR("Scheduler: a worker task threw");
				}
				lock.lock();
			}
		}
	}

	void Work(Task a_task)
	{
		std::call_once(g_workStarted, []() { std::thread(RunWork).detach(); });
		{
			std::scoped_lock lock(g_workLock);
			g_work.push_back(std::move(a_task));
		}
		g_workWake.notify_one();
	}
}
