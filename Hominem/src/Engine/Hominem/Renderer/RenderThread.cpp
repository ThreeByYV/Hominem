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
		m_RenderGraph.CreateRenderTarget("hdr",        FramebufferFormat::RGBA16F);
		m_RenderGraph.CreateRenderTarget("bloom",      FramebufferFormat::RGBA16F, 0.25f);
		m_RenderGraph.CreateRenderTarget("bloom_temp", FramebufferFormat::RGBA16F, 0.25f);

		m_RenderGraph.AddPass("scene", [](RenderGraph& graph, const RenderFrame& frame)
		{
			if (frame.viewportWidth == 0 || frame.viewportHeight == 0)
				return;

			auto hdr = graph.GetFBO("hdr");
			hdr->Bind();

			RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
			RenderCommand::SetScissorEnabled(false);
			RenderCommand::SetClearColor(frame.clearColor);
			RenderCommand::Clear();

			RenderCommand::SetDepthTestEnabled(false);
			Renderer2D::BeginScene(frame.viewProjection2D);

			for (const auto& q : frame.quads)
			{
				Renderer2D::PushQuad(q.transform, q.color, q.uvMin, q.uvMax, q.texture);
			}

			Renderer2D::Flush();

			for (const auto& t : frame.texts)
			{
				Renderer2D::DrawString(t.text, t.font, t.transform, t.color);
			}

			Renderer2D::EndScene();

			RenderCommand::SetDepthTestEnabled(true);
			Renderer3D::BeginScene(frame.viewProjection3D, frame.cameraWorldPos, frame.light, frame.pointLights);

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

			if (frame.debugPointLights && !frame.pointLights.empty())
				Renderer3D::DrawDebugPointLights(frame.pointLights);

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

		m_RenderGraph.AddPass("auto_exposure", [this](RenderGraph& graph, const RenderFrame& frame)
		{
			if (!frame.toneMappingEnabled) return; // skip when tone mapping is off
			auto hdr = graph.GetFBO("hdr");
			m_AutoExposure.Compute(hdr->GetColorAttachmentRendererID(),
			                       frame.viewportWidth, frame.viewportHeight);
		});

		auto thresholdShader = Renderer3D::GetShaderLibrary()->Get("bloom_threshold");
		m_RenderGraph.AddPass("bloom_threshold", [thresholdShader](RenderGraph& graph, const RenderFrame& frame)
		{
			if (!frame.bloomEnabled) return;
			auto bloom = graph.GetFBO("bloom");
			bloom->Bind();
			auto& spec = bloom->GetSpecification();
			RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
			RenderCommand::SetClearColor({ 0.f, 0.f, 0.f, 1.f });
			RenderCommand::Clear();
			RenderCommand::SetDepthTestEnabled(false);
			RenderCommand::SetScissorEnabled(false);
			thresholdShader->Bind();
			thresholdShader->SetInt("u_HDR", 0);
			thresholdShader->SetFloat("u_Threshold", frame.bloomThreshold);
			RenderCommand::BindTexture(0, graph.GetFBO("hdr")->GetColorAttachmentRendererID());
			RenderCommand::DrawFullscreenTriangle();
			bloom->Unbind();
		});

		auto blurShader = Renderer3D::GetShaderLibrary()->Get("bloom_blur");
		m_RenderGraph.AddPass("bloom_blur_h", [blurShader](RenderGraph& graph, const RenderFrame& frame)
		{
			if (!frame.bloomEnabled) return;
			auto dst = graph.GetFBO("bloom_temp");
			dst->Bind();
			auto& spec = dst->GetSpecification();
			RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
			RenderCommand::SetDepthTestEnabled(false);
			RenderCommand::SetScissorEnabled(false);
			blurShader->Bind();
			blurShader->SetInt("u_Src", 0);
			blurShader->SetInt("u_Horizontal", 1);
			RenderCommand::BindTexture(0, graph.GetFBO("bloom")->GetColorAttachmentRendererID());
			RenderCommand::DrawFullscreenTriangle();
			dst->Unbind();
		});

		m_RenderGraph.AddPass("bloom_blur_v", [blurShader](RenderGraph& graph, const RenderFrame& frame)
		{
			if (!frame.bloomEnabled) return;
			auto dst = graph.GetFBO("bloom");
			dst->Bind();
			auto& spec = dst->GetSpecification();
			RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
			RenderCommand::SetDepthTestEnabled(false);
			RenderCommand::SetScissorEnabled(false);
			blurShader->Bind();
			blurShader->SetInt("u_Src", 0);
			blurShader->SetInt("u_Horizontal", 0);
			RenderCommand::BindTexture(0, graph.GetFBO("bloom_temp")->GetColorAttachmentRendererID());
			RenderCommand::DrawFullscreenTriangle();
			dst->Unbind();
		});

		auto compositeShader = Renderer3D::GetShaderLibrary()->Get("composite");
		m_RenderGraph.AddPass("composite", [compositeShader, this](RenderGraph& graph, const RenderFrame& frame)
		{
			if (frame.viewportWidth == 0 || frame.viewportHeight == 0) return;
			auto hdrFBO = graph.GetFBO("hdr");
			if (!hdrFBO) return;
			RenderCommand::SetViewport(0, 0, frame.viewportWidth, frame.viewportHeight);
			RenderCommand::SetDepthTestEnabled(false);
			RenderCommand::SetScissorEnabled(false);
			compositeShader->Bind();
			compositeShader->SetInt("u_HDR", 0);
			compositeShader->SetInt("u_Bloom", 1);
			compositeShader->SetFloat("u_Exposure",     m_AutoExposure.GetExposure());
			compositeShader->SetFloat("u_BloomStrength", frame.bloomStrength);
			compositeShader->SetInt("u_BloomEnabled",       frame.bloomEnabled       ? 1 : 0);
			compositeShader->SetInt("u_ToneMappingEnabled", frame.toneMappingEnabled ? 1 : 0);
			RenderCommand::BindTexture(0, hdrFBO->GetColorAttachmentRendererID());
			RenderCommand::BindTexture(1, graph.GetFBO("bloom")->GetColorAttachmentRendererID());
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

		m_AutoExposure.Init(ComputeShader::Create("Resources/Shaders/luminance.comp"));

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

			// Swap first — this blocks on VSync, naturally throttling the main thread.
			// Notifying main before swap lets it spin a full loop iteration while we
			// wait for VSync, defeating the throttle.
			glfwSwapBuffers(m_Window);

			// Notify main AFTER swap+VSync so it only wakes once the display is ready.
			// Also, safe for ImGui: draw data is fully consumed before swap returns.
			m_ConsumedCV.notify_one();
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
