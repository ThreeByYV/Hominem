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

	AsyncLoad<SkinnedMesh> SkinnedMesh::LoadAsync(const std::string& path)
	{
		AsyncLoad<SkinnedMesh> load;
		load.Begin([path]() -> Ref<SkinnedMesh>
		{
			auto mesh = SkinnedMesh::Create();
			if (auto res = mesh->LoadFromFile(path); !res)
			{
				HMN_CORE_ERROR("{}", res.error());
				return nullptr;
			}
			return mesh;
		});
		return load;
	}

}
