#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <source_location>
#include <memory>

#include "Renderer.h"
#include "Hominem/Threading/JobSystem.h"

struct GLFWwindow;

namespace Hominem {

	class RenderThread
	{
	public:
		RenderThread() = default;
		~RenderThread();

		RenderThread(const RenderThread&)            = delete;
		RenderThread& operator=(const RenderThread&) = delete;

		void Start(GLFWwindow* window, uint32_t initialWidth, uint32_t initialHeight);
		void Stop();
		void Submit(RecordedFrame&& frame);

		Renderer&     GetRenderer()      { return m_Renderer; }
		SceneRenderer& GetSceneRenderer() { return m_Renderer.GetSceneRenderer(); }

		static bool IsOnRenderThread() { return std::this_thread::get_id() == s_ThreadId; }
		static void AssertRenderThread(
			std::source_location loc = std::source_location::current())
		{
			HMN_CORE_ASSERT(IsOnRenderThread(),
				"GL call '{}' made outside the render thread — will crash without a current context!",
				loc.function_name());
		}

		static void QueueUpload(Job task);

		VulkanHandle CreateVulkanRenderTarget(uint32_t w, uint32_t h);
		VulkanHandle CreateVulkanStorageBuffer(uint32_t capacity);

		void WaitIdle();
		void WaitImGuiConsumed();
		void SignalImGuiReady();

		FrameArena& GetWriteArena() { return m_Arenas[m_WriteArenaIdx]; }

	private:
		void ThreadFunc();
		void ExecuteFrame(RecordedFrame& frame);

		Renderer    m_Renderer;

		GLFWwindow* m_Window        = nullptr;
		uint32_t    m_InitialWidth  = 0;
		uint32_t    m_InitialHeight = 0;
		std::thread m_Thread;

		inline static std::thread::id s_ThreadId;

		std::mutex              m_InitMutex;
		std::condition_variable m_InitCV;
		bool                    m_Initialized = false;

		RecordedFrame           m_Frame;
		bool                    m_Consumed = true;
		bool                    m_Shutdown = false;
		std::mutex              m_Mutex;
		std::condition_variable m_ReadyCV;
		std::condition_variable m_ConsumedCV;

		FrameArena m_Arenas[2];
		uint8_t    m_WriteArenaIdx = 0;

		std::mutex              m_ImGuiMutex;
		std::condition_variable m_ImGuiReadyCV;
		std::condition_variable m_ImGuiConsumedCV;
		bool                    m_ImGuiReady    = false;
		bool                    m_ImGuiConsumed = true;

		std::atomic<VulkanHandle> m_NextVulkanHandle { 0 };

		inline static std::vector<Job>  s_UploadQueue;
		inline static std::mutex        s_UploadMutex;
	};

}
