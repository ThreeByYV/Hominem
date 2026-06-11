#include "hmnpch.h"
#include "StorageBuffer.h"
#include "RendererAPI.h"
#include "Platform/OpenGL/OpenGLStorageBuffer.h"

namespace Hominem {

	Ref<StorageBuffer> StorageBuffer::Create(uint32_t capacity)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLStorageBuffer>(capacity);
			default: HMN_CORE_ASSERT(false, "StorageBuffer: unsupported API"); return nullptr;
		}
	}

	Ref<DrawIndirectBuffer> DrawIndirectBuffer::Create(uint32_t capacity)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLDrawIndirectBuffer>(capacity);
			default: HMN_CORE_ASSERT(false, "DrawIndirectBuffer: unsupported API"); return nullptr;
		}
	}

}
