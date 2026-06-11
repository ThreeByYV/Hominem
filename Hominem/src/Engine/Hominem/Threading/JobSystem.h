#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>
#include <condition_variable>
#include <type_traits>
#include <concepts>
#include <cstring>

#include "Queue.h"

namespace Hominem {

	/**
	 * Replaces std::function to avoid per-job heap allocation.
	 * std::function mallocs for captures over ~16 bytes — bone transform jobs exceed
	 * that every frame. Functor is stored inline in 56 bytes. Compile error if exceeded.
	 */
	struct Job
	{
		alignas(8) uint8_t storage[56];
		void (*invoke)(void*)  = nullptr;
		void (*destroy)(void*) = nullptr;

		template<typename F>
		requires std::invocable<F> && (sizeof(F) <= sizeof(storage))
		// Capture too large? Reduce captures or store a pointer instead of a value.
		Job(F&& f)
		{
			new(storage) F(std::forward<F>(f));
			invoke  = [](void* p){ (*static_cast<F*>(p))(); };
			destroy = [](void* p){ static_cast<F*>(p)->~F(); };
		}

		Job() = default;
		~Job() { if (destroy) destroy(storage); }

		Job(Job&& o) noexcept : invoke(o.invoke), destroy(o.destroy)
		{
			std::memcpy(storage, o.storage, sizeof(storage));
			o.invoke = o.destroy = nullptr;
		}

		Job& operator=(Job&& o) noexcept
		{
			if (this != &o)
			{
				if (destroy) destroy(storage);
				std::memcpy(storage, o.storage, sizeof(storage));
				invoke   = o.invoke;
				destroy  = o.destroy;
				o.invoke = o.destroy = nullptr;
			}
			return *this;
		}

		Job(const Job&)            = delete;
		Job& operator=(const Job&) = delete;

		void operator()() { invoke(storage); }
	};

	/**
	 * Per-group completion counter so WaitOn() only blocks on a logical subset of jobs.
	 * Without this, Wait() stalls on everything including unrelated audio streaming.
	 */
	struct JobBatch
	{
		std::atomic<uint32_t>   pending{ 0 };
		std::condition_variable cv;
		std::mutex              mtx;
	};

	/**
	 * Thread pool — spawns (hardware_concurrency - 1) workers by default.
	 * Workers sleep via condition variable when idle, no busy-spinning.
	 */
	class JobSystem
	{
	public:
		/// @param numThreads Workers to spawn. Defaults to hardware_concurrency - 1.
		explicit JobSystem(uint32_t numThreads = 0);
		~JobSystem();

		JobSystem(const JobSystem&)            = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		/// @param batch Optional — if set, WaitOn(batch) syncs only this group.
		void Submit(Job job, JobBatch* batch = nullptr);

		/// @warning Two heap allocs + mutex per call. One-off asset loads only — never per-frame.
		template<typename F>
		auto SubmitWithResult(F&& func) -> std::future<std::invoke_result_t<F>>
		{
			using R = std::invoke_result_t<F>;
			auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(func));
			std::future<R> future = task->get_future();
			Submit(Job([task]() { (*task)(); }));
			return future;
		}

		/// Blocks until all submitted jobs complete. Calling thread helps drain the queue.
		void Wait();

		/// Blocks until all jobs in @p batch complete.
		/// Calling thread helps drain the queue; sleeps on cv if workers are still executing.
		void WaitOn(JobBatch& batch);

		uint32_t GetWorkerCount()     const { return static_cast<uint32_t>(m_Workers.size()); }
		size_t   GetPendingJobCount() const { return m_PendingJobs.load(); }

	private:
		void WorkerThread();

		/// Runs job, decrements global + batch counters, notifies batch cv when it hits zero.
		void CompleteJob(Job& job, JobBatch* batch);

		struct QueuedJob
		{
			Job       callable;
			JobBatch* batch = nullptr;
		};

		std::vector<std::thread> m_Workers;
		Queue<QueuedJob>         m_JobQueue;
		std::atomic<uint32_t>    m_PendingJobs{ 0 };
		std::atomic<bool>        m_Shutdown{ false };
	};

}
