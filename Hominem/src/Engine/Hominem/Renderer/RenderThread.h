#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <source_location>

#include "RenderFrame.h"
#include "RenderGraph.h"
#include "AutoExposure.h"
#include "Hominem/Threading/JobSystem.h"

struct GLFWwindow;

namespace Hominem {

	class RenderThread
	{
	public:
		RenderThread() = default;
		~RenderThread() { Stop(); }

		RenderThread(const RenderThread&)            = delete;
		RenderThread& operator=(const RenderThread&) = delete;

		/// Takes the GL context and starts the render loop.
		void Start(GLFWwindow* window);

		/// Signals shutdown and blocks until the render thread exits.
		void Stop();

		/// Submits a completed frame for rendering. Blocks if the previous frame hasn't been consumed yet.
		void Submit(RenderFrame&& frame);

		/// Returns true when called from the render thread.
		static bool IsOnRenderThread() { return std::this_thread::get_id() == s_ThreadId; }

		/// Asserts that the caller is on the render thread. Use to guard raw GL calls.
		static void AssertRenderThread(
			std::source_location loc = std::source_location::current())
		{
			HMN_CORE_ASSERT(IsOnRenderThread(),
				"GL call '{}' made outside the render thread — will crash without a current context!",
				loc.function_name());
		}

		/// Queues a GL task to run at the start of the next frame on the render thread.
		/// Safe to call from any thread. Captured data must remain valid until the next frame.
		static void QueueUpload(Job task);

		/// Call before building ImGui each frame — waits for the render thread to finish
		/// reading the previous frame's draw data so main can safely overwrite it.
		void WaitImGuiConsumed();

		/// Call after ImGui::Render() — signals the render thread that draw data is ready to read.
		void SignalImGuiReady();

		/// Returns the arena the main thread should write into this frame.
		FrameArena& GetWriteArena()          { return m_Arenas[m_WriteArenaIdx]; }

		/// Returns the index of the current write arena (stored in RenderFrame::arenaIdx).
		uint8_t     GetWriteArenaIdx() const { return m_WriteArenaIdx; }

	private:
		void ThreadFunc();
		void ExecuteFrame(const RenderFrame& frame);
		void SetupPasses();

		GLFWwindow* m_Window = nullptr;
		std::thread m_Thread;

		inline static std::thread::id s_ThreadId;

		RenderGraph             m_RenderGraph;
		AutoExposure            m_AutoExposure;
		RenderFrame             m_Frame;
		bool                    m_Consumed = true;
		bool                    m_Shutdown = false;
		std::mutex              m_Mutex;
		std::condition_variable m_ReadyCV;
		std::condition_variable m_ConsumedCV;

		// Double-buffered arenas: main writes into arenas[m_WriteArenaIdx],
		// render thread reads from the other. Flipped each Submit().
		FrameArena m_Arenas[2];
		uint8_t    m_WriteArenaIdx = 0;

		// ImGui draw-data handoff — main writes, render reads, one slot, no concurrent access.
		std::mutex              m_ImGuiMutex;
		std::condition_variable m_ImGuiReadyCV;    // render waits here for main to signal ready
		std::condition_variable m_ImGuiConsumedCV; // main waits here for render to signal consumed
		bool                    m_ImGuiReady    = false; // main wrote draw data, render hasn't read yet
		bool                    m_ImGuiConsumed = true;  // render read draw data, main can write again

		inline static std::vector<Job>  s_UploadQueue;
		inline static std::mutex        s_UploadMutex;
	};

}
