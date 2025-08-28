#include "hmnpch.h"
#include "VertexArray.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

#include "Renderer.h"

namespace Hominem {

	Ref<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared<OpenGLVertexArray>();
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
