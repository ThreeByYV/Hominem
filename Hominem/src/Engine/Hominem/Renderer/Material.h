#pragma once
#include <glm/glm.hpp>

namespace Hominem {

	struct Material
	{
		glm::vec3 AmbientColor  = glm::vec3(1.f); // tint on ambient term
		glm::vec3 DiffuseColor  = glm::vec3(1.f); // tint on diffuse term (texture provides base colour)
		glm::vec3 SpecularColor = glm::vec3(1.f); // colour of the specular highlight
		float     SpecIntensity = 0.5f;            // how strong the highlight is
		float     Shininess     = 32.f;            // how tight/sharp (32=plastic, 256=mirror)
	};

}
