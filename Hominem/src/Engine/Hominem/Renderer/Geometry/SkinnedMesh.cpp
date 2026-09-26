#include "hmnpch.h"
#include "Hominem/Renderer/Geometry/SkinnedMesh.h"
#include "Platform/OpenGL/OpenGLSkinnedMesh.h"

namespace Hominem {

	Ref<SkinnedMesh> SkinnedMesh::Create()
	{
		return CreateRef<OpenGLSkinnedMesh>();
	}

}
