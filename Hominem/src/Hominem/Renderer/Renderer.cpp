#include "hmnpch.h"
#include "Renderer.h"

namespace Hominem {

	void Renderer::BeginScene()
	{

	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const std::shared_ptr<VertexArray>& vertexArray)
	{
		//Render handles the step-by-step instructions for VAO, VBO, etc. setup
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}
