#include "hmnpch.h"
#include "Hominem/Renderer/RHI/RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Hominem {

	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
}
