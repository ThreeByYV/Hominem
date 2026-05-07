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

		// First-frame: create font atlas texture on the render thread where the GL
		// context lives. After this call it becomes a no-op every subsequent frame.
		ImGui_ImplOpenGL3_NewFrame();

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

	void RenderThread::ExecuteFrame(const RenderFrame& frame)
	{
		RenderCommand::SetClearColor(frame.clearColor);
		RenderCommand::Clear();

		// 2D pass
		Renderer2D::BeginScene(frame.viewProjection2D);
		for (const auto& q : frame.quads)
			Renderer2D::PushQuad(q.transform, q.color, q.uvMin, q.uvMax, q.texture);
		for (const auto& t : frame.texts)
			Renderer2D::DrawString(t.text, t.font, t.transform, t.color);
		Renderer2D::EndScene();

		// 3D pass — skinning dispatch happens here, not in OnUpdate, so GPU compute
		// overlaps with the 2D pass above on drivers that support async compute.
		Renderer3D::BeginScene(frame.viewProjection3D, frame.cameraWorldPos);
		for (const auto& m : frame.meshes)
		{
			m.mesh->DispatchSkinning(m.bones);
			Renderer3D::DrawSkinnedMesh(*m.mesh, m.transform);
		}
		Renderer3D::EndScene();

		// ImGui — draw data built on main thread, submitted to GL here.
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

}
