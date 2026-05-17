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
		SetupPasses();
		m_Thread = std::thread(&RenderThread::ThreadFunc, this);
	}

	void RenderThread::SetupPasses()
	{
		m_RenderGraph.CreateRenderTarget("hdr", FramebufferFormat::RGBA16F);

		m_RenderGraph.AddPass("scene", [](RenderGraph& graph, const RenderFrame& frame)
		{
			auto hdr = graph.GetFBO("hdr");
			hdr->Bind();

			RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
			RenderCommand::SetScissorEnabled(false);
			RenderCommand::SetClearColor(frame.clearColor);
			RenderCommand::Clear();

			if (frame.viewportWidth == 0 || frame.viewportHeight == 0)
			{
				hdr->Unbind();
				return;
			}

			RenderCommand::SetDepthTestEnabled(false);
			Renderer2D::BeginScene(frame.viewProjection2D);
			for (const auto& q : frame.quads)
				Renderer2D::PushQuad(q.transform, q.color, q.uvMin, q.uvMax, q.texture);
			Renderer2D::Flush();
			for (const auto& t : frame.texts)
				Renderer2D::DrawString(t.text, t.font, t.transform, t.color);
			Renderer2D::EndScene();

			RenderCommand::SetDepthTestEnabled(true);
			Renderer3D::BeginScene(frame.viewProjection3D, frame.cameraWorldPos, frame.light);
			for (const auto& sm : frame.staticMeshes)
				Renderer3D::DrawStaticMesh(*sm.mesh, sm.transform);
			for (const auto& m : frame.meshes)
			{
				m.mesh->DispatchSkinning(m.bones);
				Renderer3D::DrawSkinnedMesh(*m.mesh, m.transform);
			}
			Renderer3D::EndScene();

			hdr->Unbind();
		});

		// ImGui pass — renders into the HDR FBO so all shaders always target RGBA16F.
		m_RenderGraph.AddPass("imgui", [this](RenderGraph& graph, const RenderFrame& /*frame*/)
		{
			// Block until main has finished writing draw data for this frame.
			{
				std::unique_lock lock(m_ImGuiMutex);
				m_ImGuiReadyCV.wait(lock, [this]{ return m_ImGuiReady || m_Shutdown; });
				m_ImGuiReady = false;
			}

			graph.GetFBO("hdr")->Bind();
			ImGui_ImplOpenGL3_NewFrame();
			if (ImDrawData* drawData = ImGui::GetDrawData())
				ImGui_ImplOpenGL3_RenderDrawData(drawData);
			graph.GetFBO("hdr")->Unbind();

			// Signal main that draw data is consumed — safe to write next frame.
			{
				std::lock_guard lock(m_ImGuiMutex);
				m_ImGuiConsumed = true;
			}
			m_ImGuiConsumedCV.notify_one();
		});

		auto compositeShader = Renderer3D::GetShaderLibrary()->Get("composite");
		m_RenderGraph.AddPass("composite", [compositeShader](RenderGraph& graph, const RenderFrame& frame)
		{
			if (frame.viewportWidth > 0 && frame.viewportHeight > 0)
				RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
			RenderCommand::SetDepthTestEnabled(false);
			RenderCommand::SetScissorEnabled(false);
			compositeShader->Bind();
			compositeShader->SetInt("u_HDR", 0);
			RenderCommand::BindTexture(0, graph.GetFBO("hdr")->GetColorAttachmentRendererID());
			RenderCommand::DrawFullscreenTriangle();
			RenderCommand::SetDepthTestEnabled(true);
		});
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

			// Notify main AFTER ImGui draw data is consumed — if we notify earlier
			// the main thread can call ImGui::Render() while we're still reading
			// GetDrawData(), corrupting the heap (0xC0000005).
			m_ConsumedCV.notify_one();

			// frame destructs here — no return-to-pool. Application builds a fresh
			// RenderFrame each iteration so returning capacity provides no benefit,
			// and the return-to-m_Frame write races with main's Submit.

			glfwSwapBuffers(m_Window);
		}

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

		m_RenderGraph.Execute(frame);
	}

}
