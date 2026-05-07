#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "RenderFrame.h"

struct GLFWwindow; // opaque GLFW handle — full include in .cpp

namespace Hominem {

	/**
	 *  Owns the OpenGL context and executes RenderFrames on a dedicated thread.
	 *
	 *  Since SwapBuffers blocks until vblank on the main thread – that would stall input,
	 *  physics, and game logic for the full GPU submission + vblank wait (~6-10ms with
	 *  post-processing).
	 *
	 *	Moving it here lets the main thread stay unblocked.
	 */
	class RenderThread
	{
	public:
		RenderThread() = default;
		~RenderThread() { Stop(); }

		RenderThread(const RenderThread&)            = delete;
		RenderThread& operator=(const RenderThread&) = delete;

		/// Acquires the GL context on the render thread and starts the loop.
		/// Must be called after OpenGLContext::Init() releases the context on main.
		void Start(GLFWwindow* window);

		/// Flushes any pending frame and joins the render thread.
		void Stop();

		/// Hands a frame to the render thread.
		/// Blocks only if the previous frame hasn't been consumed yet (GPU-bound case).
		void Submit(RenderFrame&& frame);

		/// Returns true if the calling thread is the render thread.
		static bool IsOnRenderThread() { return std::this_thread::get_id() == s_ThreadId; }

		/// Call from any GL-touching function in debug builds to catch context violations.
		static void AssertRenderThread(const char* func)
		{
			HMN_CORE_ASSERT(IsOnRenderThread(),
				"GL call '{}' made outside the render thread — will crash without a current context!", func);
		}

	private:
		void ThreadFunc();
		void ExecuteFrame(const RenderFrame& frame);

		GLFWwindow* m_Window = nullptr;
		std::thread m_Thread;

		inline static std::thread::id s_ThreadId; // set once in ThreadFunc

		RenderFrame             m_Frame;
		bool                    m_Consumed = true;  // true = no frame waiting
		bool                    m_Shutdown = false;
		std::mutex              m_Mutex;
		std::condition_variable m_ReadyCV;    // render waits here for a frame
		std::condition_variable m_ConsumedCV; // main waits here when GPU-bound
	};

}
