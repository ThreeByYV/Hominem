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
		{
			m_Workers.emplace_back(&JobSystem::WorkerThread, this);
		}
	}

	JobSystem::~JobSystem()
	{
		HMN_CORE_INFO("JobSystem: Shutting down...");

		m_Shutdown = true;
		m_JobQueue.Shutdown();

		for (auto& worker : m_Workers)
		{
			if (worker.joinable())
				worker.join();
		}

		HMN_CORE_INFO("JobSystem: Shutdown complete");
	}

	void JobSystem::Submit(Job job)
	{
		m_PendingJobs.fetch_add(1, std::memory_order_relaxed);
		m_JobQueue.Push(std::move(job));
	}

	void JobSystem::Wait()
	{
		while (m_PendingJobs.load(std::memory_order_relaxed) > 0)
		{
			// Help process jobs on main thread while waiting
			Job job;
			if (m_JobQueue.TryPop(job))
			{
				job();
				m_PendingJobs.fetch_sub(1, std::memory_order_relaxed);
			}
			else
			{
				std::this_thread::yield();
			}
		}
	}

	void JobSystem::WorkerThread()
	{
		while (!m_Shutdown)
		{
			auto job = m_JobQueue.WaitAndPop();
			if (job.has_value())
			{
				job.value()();
				m_PendingJobs.fetch_sub(1, std::memory_order_relaxed);
			}
		}
	}

}
