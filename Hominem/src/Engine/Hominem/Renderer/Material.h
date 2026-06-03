#pragma once
#include <glm/glm.hpp>
#include "Hominem/Core/Ref.h"
#include "Texture.h"

namespace Hominem {

	struct Material
	{
		float Roughness = 0.5f;
		float Metalness = 0.0f;

		Ref<Texture2D> NormalMap;
		Ref<Texture2D> MetalRoughnessMap;
	};

}
