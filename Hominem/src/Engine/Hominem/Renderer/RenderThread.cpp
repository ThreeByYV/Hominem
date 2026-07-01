#include "hmnpch.h"
#include "RenderThread.h"
#include "Hominem/Core/Profiler.h"

#include <GLFW/glfw3.h>

namespace Hominem {

RenderThread::~RenderThread() { Stop(); }

void RenderThread::Start(GLFWwindow* window, uint32_t initialWidth, uint32_t initialHeight)
{
	m_Window        = window;
	m_InitialWidth  = initialWidth;
	m_InitialHeight = initialHeight;

	m_Renderer.GetSceneRenderer().SetImGuiCallbacks(
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

void RenderThread::Submit(RecordedFrame&& frame)
{
	std::unique_lock lock(m_Mutex);
	m_ConsumedCV.wait(lock, [this]{ return m_Consumed || m_Shutdown; });
	if (m_Shutdown) return;

	m_Frame    = std::move(frame);
	m_Consumed = false;
	m_WriteArenaIdx ^= 1;
	lock.unlock();
	m_ReadyCV.notify_one();
}

void RenderThread::ThreadFunc()
{
	s_ThreadId = std::this_thread::get_id();
	glfwMakeContextCurrent(m_Window);

	m_Renderer.Init(m_InitialWidth, m_InitialHeight);

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
		}

		ExecuteFrame(frame);

		{
			std::lock_guard lock(m_Mutex);
			m_Consumed = true;
		}
		m_ConsumedCV.notify_one();

		glfwSwapBuffers(m_Window);
	}

	m_Renderer.Shutdown();
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

VulkanHandle RenderThread::CreateVulkanRenderTarget(uint32_t w, uint32_t h)
{
	VulkanHandle handle = m_NextVulkanHandle.fetch_add(1, std::memory_order_relaxed);
	QueueUpload([this, handle, w, h] { m_Renderer.RegisterRenderTarget(handle, w, h); });
	return handle;
}

VulkanHandle RenderThread::CreateVulkanStorageBuffer(uint32_t capacity)
{
	VulkanHandle handle = m_NextVulkanHandle.fetch_add(1, std::memory_order_relaxed);
	QueueUpload([this, handle, capacity] { m_Renderer.RegisterStorageBuffer(handle, capacity); });
	return handle;
}

void RenderThread::ExecuteFrame(RecordedFrame& frame)
{
	HMN_PROFILE_SCOPE("RenderThread::ExecuteFrame");
	{
		std::vector<Job> uploads;
		{
			std::lock_guard lock(s_UploadMutex);
			uploads.swap(s_UploadQueue);
		}
		for (auto& task : uploads)
			task();
	}

	m_Renderer.ExecuteFrame(frame);
}

}
