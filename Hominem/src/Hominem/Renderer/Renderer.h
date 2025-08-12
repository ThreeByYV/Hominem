#pragma once
#include "RenderCommand.h"

namespace Hominem {

	class Renderer 
	{
	public:
		static void BeginScene(); // todo take in scene params here
		static void EndScene();

		static void Submit(const std::shared_ptr<VertexArray>& vertexArray);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	};

}
