#include "hmnpch.h"
#include "StaticMesh.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLStaticMesh.h"

namespace Hominem {

    Ref<StaticMesh> StaticMesh::Create()
    {
        switch (Renderer::GetAPI())
        {
            case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
            case RendererAPI::API::OpenGL: return CreateRef<OpenGLStaticMesh>();
        }

        HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }

    AsyncLoad<StaticMesh> StaticMesh::LoadAsync(const std::string& path)
    {
        AsyncLoad<StaticMesh> load;
        load.Begin([path]() -> Ref<StaticMesh>
        {
            auto mesh = StaticMesh::Create();
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
