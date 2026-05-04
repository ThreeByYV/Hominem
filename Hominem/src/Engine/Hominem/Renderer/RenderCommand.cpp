#include "hmnpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Hominem {

	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
}