#include "hmnpch.h"
#include "GBuffer.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLGBuffer.h"

namespace Hominem {

	Ref<GBuffer> GBuffer::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared<OpenGLGBuffer>(width, height);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
