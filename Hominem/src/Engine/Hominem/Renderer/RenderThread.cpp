#include "hmnpch.h"
#include "RenderThread.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>

#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"

namespace Hominem {

	void RenderThread::Start(GLFWwindow* window)
	{
		m_Window = window;
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
		lock.unlock();
		m_ReadyCV.notify_one();
	}

	void RenderThread::ThreadFunc()
	{
		s_ThreadId = std::this_thread::get_id();
		glfwMakeContextCurrent(m_Window);

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

			// Notify main AFTER ImGui draw data is consumed — if we notify earlier
			// the main thread can call ImGui::Render() while we're still reading
			// GetDrawData(), corrupting the heap (0xC0000005).
			m_ConsumedCV.notify_one();

			glfwSwapBuffers(m_Window);
		}

		glfwMakeContextCurrent(nullptr);
	}

	void RenderThread::QueueUpload(std::function<void()> task)
	{
		std::lock_guard lock(s_UploadMutex);
		s_UploadQueue.push_back(std::move(task));
	}

	void RenderThread::ExecuteFrame(const RenderFrame& frame)
	{
		// Drain pending GPU uploads — swap to avoid holding the lock while executing.
		{
			std::vector<std::function<void()>> uploads;
			{
				std::lock_guard lock(s_UploadMutex);
				uploads.swap(s_UploadQueue);
			}

			for (auto& task : uploads)
			{
				task();
			}
		}

		// Reset scissor/viewport — ImGui leaves glScissor set to its last window rect,
		// which would clip both glClear and 3D rendering to that region next frame.
		if (frame.viewportWidth > 0 && frame.viewportHeight > 0)
			RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);

		RenderCommand::SetScissorEnabled(false);

		RenderCommand::SetClearColor(frame.clearColor);
		RenderCommand::Clear();

		// 2D pass — Flush() draws quads, then DrawString draws text on top. Both stay
		// inside BeginScene/EndScene so shader+VP state set by BeginScene is still live
		// when DrawString runs. Depth test disabled so Z value controls draw-order only.
		RenderCommand::SetDepthTestEnabled(false);

		Renderer2D::BeginScene(frame.viewProjection2D);

		for (const auto& q : frame.quads)
		{
			Renderer2D::PushQuad(q.transform, q.color, q.uvMin, q.uvMax, q.texture);
		}

		Renderer2D::Flush(); // draw quads (backgrounds, sprites) first

		for (const auto& t : frame.texts)
		{
			Renderer2D::DrawString(t.text, t.font, t.transform, t.color);
		}

		Renderer2D::EndScene(); // cleanup — Flush is a no-op here since batch is empty

		RenderCommand::SetDepthTestEnabled(true);

		// 3D pass — frustum culling was already applied on the main thread so every
		// entry in staticMeshes and meshes is guaranteed visible.
		Renderer3D::BeginScene(frame.viewProjection3D, frame.cameraWorldPos);

		for (const auto& sm : frame.staticMeshes)
		{
			Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform);
		}

		for (const auto& m : frame.meshes)
		{
			m.mesh->DispatchSkinning(m.bones);
			Renderer3D::DrawSkinnedMesh(*m.mesh, m.transform);
		}

		Renderer3D::EndScene();

		// ImGui — draw data built on main thread, submitted to GL here.
		ImGui_ImplOpenGL3_NewFrame();
		if (ImDrawData* drawData = ImGui::GetDrawData())
			ImGui_ImplOpenGL3_RenderDrawData(drawData);
	}

}
