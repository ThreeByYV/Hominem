#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Hominem {

	/// Simple sprite actor — renders a textured or colored quad in the 2D pass.
	class SpriteActor : public Actor
	{
	public:
		SpriteActor() = default;
		SpriteActor(glm::vec3 pos, glm::vec3 scale,
		            Ref<Texture2D> texture = nullptr,
		            glm::vec4 color = glm::vec4(1.f))
		{
			Position = pos;
			Scale    = scale;
			Texture  = texture;
			Color    = color;
		}

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			QuadDraw q;
			q.transform = GetTransform();
			q.color     = Color;
			q.texture   = Texture;
			frame.quads.push_back(std::move(q));
		}

		Ref<Texture2D> Texture;
		glm::vec4      Color{ 1.f };
	};

}
