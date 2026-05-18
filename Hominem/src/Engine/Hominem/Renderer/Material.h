#pragma once
#include <glm/glm.hpp>

namespace Hominem {

	struct Material
	{
		float Roughness = 0.5f; // 0 = mirror smooth, 1 = fully rough
		float Metalness = 0.0f; // 0 = dielectric, 1 = metal
	};

}
