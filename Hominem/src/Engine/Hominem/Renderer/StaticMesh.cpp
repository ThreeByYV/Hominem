#include "hmnpch.h"
#include "StaticMesh.h"
#include "RendererAPI.h"
#include "Platform/OpenGL/OpenGLStaticMesh.h"

namespace Hominem {

    Ref<StaticMesh> StaticMesh::Create()
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
            case RendererAPI::API::OpenGL: return CreateRef<OpenGLStaticMesh>();
        }

        HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }


}
