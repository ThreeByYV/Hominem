#include "hmnpch.h"
#include "StorageBuffer.h"
#include "Platform/OpenGL/OpenGLStorageBuffer.h"

namespace Hominem {

	Ref<StorageBuffer> StorageBuffer::Create(uint32_t capacity)
	{
		return CreateRef<OpenGLStorageBuffer>(capacity);
	}

	Ref<DrawIndirectBuffer> DrawIndirectBuffer::Create(uint32_t capacity)
	{
		return CreateRef<OpenGLDrawIndirectBuffer>(capacity);
	}
}
