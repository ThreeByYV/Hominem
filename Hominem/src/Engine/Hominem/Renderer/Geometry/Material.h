#pragma once
#include <glm/glm.hpp>
#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/RHI/Texture.h"

namespace Hominem {

	class CommandList;
	class Shader;

	/// glTF metallic-roughness. Null textures bind neutral fallbacks.
	struct Material
	{
		Ref<Texture2D> BaseColor;
		Ref<Texture2D> MetalRoughness; // G roughness, B metallic
		Ref<Texture2D> Normal;
		Ref<Texture2D> Emissive;

		glm::vec4 BaseColorFactor{ 1.f };
		float     Metallic  = 0.f;    // scales the map, or the value without one
		float     Roughness = 0.5f;
		glm::vec3 EmissiveFactor{ 0.f };
	};

	namespace MaterialSlot
	{
		constexpr uint32_t BaseColor      = 0;
		constexpr uint32_t MetalRoughness = 1;
		constexpr uint32_t Normal         = 2;
		constexpr uint32_t Emissive       = 9; // 3-8 hold environment and DDGI
	}

	void BindMaterial(CommandList& cmd, const Ref<Shader>& shader, const Material& material);

}
