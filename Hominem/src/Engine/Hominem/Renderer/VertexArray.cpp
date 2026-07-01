#include "hmnpch.h"
#include "VertexArray.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Hominem {

	Ref<VertexArray> VertexArray::Create()
	{
		return CreateRef<OpenGLVertexArray>();
	}

}
