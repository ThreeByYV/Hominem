#include "hmnpch.h"
#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "RenderThread.h"
#include "Platform/OpenGL/OpenGLShader.h"


namespace Hominem {

	Renderer::SceneData* Renderer::m_SceneData = new Renderer::SceneData;

	void Renderer::Init()
	{
		RenderCommand::Init();
		Renderer2D::Init();
		Renderer3D::Init();
	}

	void Renderer::Shutdown()
	{
		Renderer3D::Shutdown();
		Renderer2D::Shutdown();
		delete m_SceneData;
		m_SceneData = nullptr;
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderThread::QueueUpload([width, height] {
			RenderCommand::SetViewport(0, 0, width, height);
		});
	}

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		m_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<VertexArray>& vertexArray, const Ref<Shader>& shader, const glm::mat4& transform)
	{
		shader->Bind();
		shader.As<OpenGLShader>()->UploadUniformMat4("u_ViewProjection", m_SceneData->ViewProjectionMatrix);
		shader.As<OpenGLShader>()->UploadUniformMat4("u_Transform", transform);

		vertexArray->Bind();

		auto cmd = RenderCommand::SetPipelineState(PipelineState::DepthTestWriteCull());
		cmd.DrawIndexed(vertexArray);
		cmd.Submit();
	}
}
