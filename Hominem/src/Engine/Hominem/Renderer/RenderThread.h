#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <source_location>

#include "RenderFrame.h"
#include "SceneRenderer.h"
#include "Hominem/Threading/JobSystem.h"

struct GLFWwindow;

namespace Hominem {

	/// A frame's worth of recorded CommandLists, ready to be submitted by the render thread.
	/// Built on the main thread via SceneRenderer::Record.
	struct RecordedFrame
	{
		std::vector<CommandList> passCmds;
		uint32_t                 viewportWidth  = 0;
		uint32_t                 viewportHeight = 0;
		float                    renderScale    = 1.0f;
	};

	class RenderThread
	{
	public:
		RenderThread() = default;
		~RenderThread() { Stop(); }

		RenderThread(const RenderThread&)            = delete;
		RenderThread& operator=(const RenderThread&) = delete;

		/// Takes the GL context and starts the render loop. initialWidth/Height size the
		/// FBOs before the main thread's first Record() call (queried from Window, not GL).
		void Start(GLFWwindow* window, uint32_t initialWidth, uint32_t initialHeight);

		/// Signals shutdown and blocks until the render thread exits.
		void Stop();

		/// Submits a frame's recorded CommandLists for rendering. Blocks if the previous frame hasn't been consumed yet.
		void Submit(RecordedFrame&& frame);

		/// Returns the scene renderer — used by the main thread to record CommandLists
		/// and read the render graph (FBO ids/specs are stable for reading once the
		/// previous frame's resize, if any, has completed — see ExecuteFrame).
		SceneRenderer& GetSceneRenderer() { return m_SceneRenderer; }

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

		/// Returns the arena the main thread should write bone matrices into this frame.
		FrameArena& GetWriteArena() { return m_Arenas[m_WriteArenaIdx]; }

	private:
		void ThreadFunc();
		void ExecuteFrame(RecordedFrame& frame);

		GLFWwindow* m_Window = nullptr;
		uint32_t    m_InitialWidth  = 0;
		uint32_t    m_InitialHeight = 0;
		std::thread m_Thread;

		inline static std::thread::id s_ThreadId;

		// Start() blocks until ThreadFunc has run SceneRenderer::Init() and the initial
		// FBO resize — Record() (main thread) reads FBO ids and must never see nulls.
		std::mutex              m_InitMutex;
		std::condition_variable m_InitCV;
		bool                    m_Initialized = false;

		SceneRenderer           m_SceneRenderer;
		RecordedFrame           m_Frame;
		bool                    m_Consumed = true;
		bool                    m_Shutdown = false;
		std::mutex              m_Mutex;
		std::condition_variable m_ReadyCV;
		std::condition_variable m_ConsumedCV;

		// Double-buffered arenas: main writes into arenas[m_WriteArenaIdx] then resets it
		// after Record(). Flipped each Submit() so the next frame gets a fresh slot.
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
