#include "hmnpch.h"
#include "Hominem/Renderer/RHI/VertexArray.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Hominem {

	Ref<VertexArray> VertexArray::Create()
	{
		return CreateRef<OpenGLVertexArray>();
	}

}
