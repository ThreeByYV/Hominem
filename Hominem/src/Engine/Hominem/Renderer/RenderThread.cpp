#include "hmnpch.h"
#include "RenderThread.h"
#include "Hominem/Core/Profiler.h"

#include <GLFW/glfw3.h>

namespace Hominem {

	void RenderThread::Start(GLFWwindow* window)
	{
		m_Window = window;

		m_SceneRenderer.SetImGuiCallbacks(
			[this]
			{
				std::unique_lock lock(m_ImGuiMutex);
				m_ImGuiReadyCV.wait(lock, [this]{ return m_ImGuiReady || m_Shutdown; });
				m_ImGuiReady = false;
			},
			[this]
			{
				{
					std::lock_guard lock(m_ImGuiMutex);
					m_ImGuiConsumed = true;
				}
				m_ImGuiConsumedCV.notify_one();
			});

		m_Thread = std::thread(&RenderThread::ThreadFunc, this);
	}

	void RenderThread::Stop()
	{
		if (!m_Thread.joinable()) return;

		{
			std::lock_guard lock(m_Mutex);
			m_Shutdown = true;
		}
		m_ReadyCV.notify_one();
		m_ConsumedCV.notify_all();
		m_ImGuiReadyCV.notify_all();
		m_ImGuiConsumedCV.notify_all();
		m_Thread.join();
	}

	void RenderThread::Submit(RenderFrame&& frame)
	{
		std::unique_lock lock(m_Mutex);
		// Block only if render thread hasn't consumed the last frame yet (GPU-bound).
		m_ConsumedCV.wait(lock, [this]{ return m_Consumed || m_Shutdown; });
		if (m_Shutdown) return;

		m_Frame    = std::move(frame);
		m_Consumed = false;
		// Flip write arena — next frame's main-thread writes go into the other slot.
		m_WriteArenaIdx ^= 1;
		lock.unlock();
		m_ReadyCV.notify_one();
	}

	void RenderThread::ThreadFunc()
	{
		s_ThreadId = std::this_thread::get_id();
		glfwMakeContextCurrent(m_Window);

		m_SceneRenderer.Init();

		while (true)
		{
			RenderFrame frame;
			{
				std::unique_lock lock(m_Mutex);
				m_ReadyCV.wait(lock, [this]{ return !m_Consumed || m_Shutdown; });
				if (m_Shutdown) break;

				frame      = std::move(m_Frame);
				m_Consumed = true;
			}

			ExecuteFrame(frame);

			if (frame.arena)
				frame.arena->Reset();

			// Swap first — this blocks on VSync, naturally throttling the main thread.
			// Notifying main before swap lets it spin a full loop iteration while we
			// wait for VSync, defeating the throttle.
			glfwSwapBuffers(m_Window);

			// Notify main AFTER swap+VSync so it only wakes once the display is ready.
			m_ConsumedCV.notify_one();
		}

		m_SceneRenderer.Shutdown();
		glfwMakeContextCurrent(nullptr);
	}

	void RenderThread::WaitImGuiConsumed()
	{
		std::unique_lock lock(m_ImGuiMutex);
		m_ImGuiConsumedCV.wait(lock, [this]{ return m_ImGuiConsumed || m_Shutdown; });
	}

	void RenderThread::SignalImGuiReady()
	{
		{
			std::lock_guard lock(m_ImGuiMutex);
			m_ImGuiReady    = true;
			m_ImGuiConsumed = false;
		}
		m_ImGuiReadyCV.notify_one();
	}

	void RenderThread::QueueUpload(Job task)
	{
		std::lock_guard lock(s_UploadMutex);
		s_UploadQueue.push_back(std::move(task));
	}

	void RenderThread::ExecuteFrame(const RenderFrame& frame)
	{
		HMN_PROFILE_SCOPE("RenderThread::ExecuteFrame");
		// Drain pending GPU uploads — swap to avoid holding the lock while executing.
		{
			std::vector<Job> uploads;
			{
				std::lock_guard lock(s_UploadMutex);
				uploads.swap(s_UploadQueue);
			}
			for (auto& task : uploads)
				task();
		}

		m_SceneRenderer.RenderScene(frame);
	}

}
