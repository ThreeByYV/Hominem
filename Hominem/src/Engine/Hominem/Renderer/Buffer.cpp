#include "hmnpch.h"
#include "Buffer.h"

#include "RendererAPI.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Hominem {

	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(vertices, size);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(size);
		}
		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}


	Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t size)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLIndexBuffer>(indices, size);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}