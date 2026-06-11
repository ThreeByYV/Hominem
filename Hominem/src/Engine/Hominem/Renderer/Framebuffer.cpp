#include "hmnpch.h"
#include "Framebuffer.h"

#include "Hominem/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace Hominem {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is not supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLFramebuffer>(spec);
		}
		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}