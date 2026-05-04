#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <future>

#include "Queue.h"

namespace Hominem {

	using Job = std::function<void()>;

	/// @brief Manages a pool of worker threads for parallel task execution.
	class JobSystem
	{
	public:
		/// @param numThreads Number of workers. Default: hardware_concurrency - 1
		explicit JobSystem(uint32_t numThreads = 0);
		~JobSystem();

		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		/// @brief Submits a job to be executed by a worker thread.
		void Submit(Job job);

		/// @brief Submits a job that returns a value using std::packaged_task.
		/// @tparam F Callable type (lambda, function, functor)
		/// @return std::future<R> where R is the return type of F
		template<typename F>
		auto SubmitWithResult(F&& func) -> std::future<std::invoke_result_t<F>>
		{
			using R = std::invoke_result_t<F>;

			auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(func));
			std::future<R> future = task->get_future();

			// Submit the task to be executed by a worker thread
			Submit([task]() { (*task)(); });

			return future;
		}

		/// @brief Blocks until all submitted jobs are complete.
		void Wait();

		uint32_t GetWorkerCount() const { return static_cast<uint32_t>(m_Workers.size()); }
		size_t GetPendingJobCount() const { return m_PendingJobs.load(); }

	private:
		void WorkerThread();

		std::vector<std::thread> m_Workers;
		Queue<Job> m_JobQueue;
		std::atomic<uint32_t> m_PendingJobs{ 0 };
		std::atomic<bool> m_Shutdown{ false };
	};

}
