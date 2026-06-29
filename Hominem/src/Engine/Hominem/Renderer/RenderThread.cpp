#include "hmnpch.h"
#include "RenderThread.h"
#include "Hominem/Core/Profiler.h"

#include <GLFW/glfw3.h>

namespace Hominem {

	void RenderThread::Start(GLFWwindow* window, uint32_t initialWidth, uint32_t initialHeight)
	{
		m_Window        = window;
		m_InitialWidth  = initialWidth;
		m_InitialHeight = initialHeight;

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

		// Wait for the render thread to finish Init() + the initial FBO resize so the
		// main thread's first Record() call sees valid (non-null) FBOs.
		std::unique_lock lock(m_InitMutex);
		m_InitCV.wait(lock, [this]{ return m_Initialized; });
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

	void RenderThread:: Submit(RecordedFrame&& frame)
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

		// Create FBOs at the window's current size before the main thread's first Record().
		m_SceneRenderer.GetRenderGraph().Resize(m_InitialWidth, m_InitialHeight);

		{
			std::lock_guard lock(m_InitMutex);
			m_Initialized = true;
		}
		m_InitCV.notify_one();

		while (true)
		{
			RecordedFrame frame;
			{
				std::unique_lock lock(m_Mutex);
				m_ReadyCV.wait(lock, [this]{ return !m_Consumed || m_Shutdown; });
				if (m_Shutdown) break;

				frame = std::move(m_Frame);
				// Don't signal consumed yet — ExecuteFrame writes GL resource IDs
				// (QueueUpload, FBO resize) that the main thread reads in Record().
				// Signal after ExecuteFrame so those writes are visible.
			}

			ExecuteFrame(frame);

			// All GL resource writes are done — main thread can safely call Record()
			// and read texture/mesh IDs. Signal before swap so main-thread CPU work
			// (OnUpdate/ImGui/Record) overlaps with the VSync wait below.
			{
				std::lock_guard lock(m_Mutex);
				m_Consumed = true;
			}
			m_ConsumedCV.notify_one();

			glfwSwapBuffers(m_Window);
		}

		m_SceneRenderer.Shutdown();
		glfwMakeContextCurrent(nullptr);
	}

	void RenderThread::WaitIdle()
	{
		std::unique_lock lock(m_Mutex);
		m_ConsumedCV.wait(lock, [this]{ return m_Consumed || m_Shutdown; });
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

	void RenderThread::ExecuteFrame(RecordedFrame& frame)
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

		for (auto& cmd : frame.passCmds)
			cmd.Submit();

		// FBO create/resize needs the GL context, so it happens here (render thread) using
		// this frame's now-replayed CommandLists. The main thread's next Record() call is
		// guaranteed (via the consumed-CV handshake below) to see the resized FBOs.
		auto& graph = m_SceneRenderer.GetRenderGraph();
		graph.SetRenderScale(frame.renderScale);
		graph.Resize(frame.viewportWidth, frame.viewportHeight);
	}

}
