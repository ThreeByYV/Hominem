#include "hmnpch.h"
#include "Hominem/Renderer/RHI/Framebuffer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace Hominem {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		return CreateRef<OpenGLFramebuffer>(spec);
	}

}
