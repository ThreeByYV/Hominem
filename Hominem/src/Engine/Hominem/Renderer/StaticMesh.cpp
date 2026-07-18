#include "hmnpch.h"
#include "StaticMesh.h"
#include "Platform/OpenGL/OpenGLStaticMesh.h"

namespace Hominem {

    Ref<StaticMesh> StaticMesh::Create()
    {
        return CreateRef<OpenGLStaticMesh>();
    }

}
