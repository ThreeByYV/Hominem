#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <source_location>

#include "RenderFrame.h"

struct GLFWwindow;

namespace Hominem {

	class RenderThread
	{
	public:
		RenderThread() = default;
		~RenderThread() { Stop(); }

		RenderThread(const RenderThread&)            = delete;
		RenderThread& operator=(const RenderThread&) = delete;

		void Start(GLFWwindow* window);
		void Stop();
		void Submit(RenderFrame&& frame);

		static bool IsOnRenderThread() { return std::this_thread::get_id() == s_ThreadId; }

		static void AssertRenderThread(
			std::source_location loc = std::source_location::current())
		{
			HMN_CORE_ASSERT(IsOnRenderThread(),
				"GL call '{}' made outside the render thread — will crash without a current context!",
				loc.function_name());
		}

		/**
		 * @brief Queue a GL task to run at the start of the next frame, on the render thread.
		 *
		 * @details Safe to call from any thread. Use this for any GL call that originates
		 * outside ExecuteFrame — e.g. runtime resource creation (textures, meshes), viewport
		 * resize, wrap mode changes. Init-time resources created before RenderThread::Start()
		 * are fine on the main thread since the render thread has not taken the GL context yet.
		 *
		 * @param task GL work to execute; captured data must remain valid until the next frame.
		 */
		static void QueueUpload(std::function<void()> task);

	private:
		void ThreadFunc();
		void ExecuteFrame(const RenderFrame& frame);

		GLFWwindow* m_Window = nullptr;
		std::thread m_Thread;

		inline static std::thread::id s_ThreadId;

		RenderFrame             m_Frame;
		bool                    m_Consumed = true;
		bool                    m_Shutdown = false;
		std::mutex              m_Mutex;
		std::condition_variable m_ReadyCV;
		std::condition_variable m_ConsumedCV;

		inline static std::vector<std::function<void()>> s_UploadQueue;
		inline static std::mutex                         s_UploadMutex;
	};

} // namespace Hominem
