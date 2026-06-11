#include "hmnpch.h"
#include "JobSystem.h"

namespace Hominem {

	JobSystem::JobSystem(uint32_t numThreads)
	{
		if (numThreads == 0)
		{
			numThreads = std::thread::hardware_concurrency();
			if (numThreads > 1)
				numThreads -= 1;  // Leave one core for main thread
		}

		HMN_CORE_INFO("JobSystem: Starting {} worker threads", numThreads);

		m_Workers.reserve(numThreads);
		for (uint32_t i = 0; i < numThreads; i++)
			m_Workers.emplace_back(&JobSystem::WorkerThread, this);
	}

	JobSystem::~JobSystem()
	{
		HMN_CORE_INFO("JobSystem: Shutting down...");

		m_Shutdown = true;
		m_JobQueue.Shutdown();

		for (auto& worker : m_Workers)
			if (worker.joinable()) worker.join();

		HMN_CORE_INFO("JobSystem: Shutdown complete");
	}

	void JobSystem::Submit(Job job, JobBatch* batch)
	{
		m_PendingJobs.fetch_add(1, std::memory_order_relaxed);
		if (batch)
			batch->pending.fetch_add(1, std::memory_order_relaxed);

		m_JobQueue.Push({ std::move(job), batch });
	}

	void JobSystem::Wait()
	{
		while (m_PendingJobs.load(std::memory_order_relaxed) > 0)
		{
			QueuedJob queued;
			if (m_JobQueue.TryPop(queued))
				CompleteJob(queued.callable, queued.batch);
			else
				std::this_thread::yield();
		}
	}

	void JobSystem::WaitOn(JobBatch& batch)
	{
		while (batch.pending.load(std::memory_order_relaxed) > 0)
		{
			QueuedJob queued;
			if (m_JobQueue.TryPop(queued))
			{
				CompleteJob(queued.callable, queued.batch);
			}
			else
			{
				// Queue is drained but jobs are still mid-execution on workers.
				// Sleep on the condition variable rather than spinning.
				std::unique_lock<std::mutex> lock(batch.mtx);
				batch.cv.wait_for(lock, std::chrono::microseconds(100),
					[&batch] { return batch.pending.load(std::memory_order_relaxed) == 0; });
			}
		}
	}

	void JobSystem::WorkerThread()
	{
		while (!m_Shutdown)
		{
			auto queued = m_JobQueue.WaitAndPop();
			if (queued.has_value())
				CompleteJob(queued->callable, queued->batch);
		}
	}

	void JobSystem::CompleteJob(Job& job, JobBatch* batch)
	{
		job();
		m_PendingJobs.fetch_sub(1, std::memory_order_relaxed);

		if (batch && batch->pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
			batch->cv.notify_all();
	}

}
