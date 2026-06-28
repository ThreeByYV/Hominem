#include "hmnpch.h"
#include "SkinnedMesh.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLSkinnedMesh.h"

namespace Hominem {

	Ref<SkinnedMesh> SkinnedMesh::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLSkinnedMesh>();
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}


}
