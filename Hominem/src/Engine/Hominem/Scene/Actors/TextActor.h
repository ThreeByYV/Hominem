#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Font.h"

#include <string>
#include <glm/glm.hpp>

namespace Hominem {

	/// Simple text actor — renders a string with a font in the 2D pass.
	class TextActor : public Actor
	{
	public:
		TextActor() = default;
		TextActor(glm::vec3 pos, glm::vec3 scale,
		          const std::string& text,
		          Ref<Font> font = nullptr,
		          glm::vec4 color = glm::vec4(1.f))
		{
			Position = pos;
			Scale    = scale;
			Text     = text;
			FontRef  = font ? font : Font::GetDefaultFont();
			Color    = color;
		}

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			if (!FontRef || Text.empty()) return;
			frame.texts.push_back({ Text, FontRef, GetTransform(), Color });
		}

		std::string  Text;
		Ref<Font>    FontRef;
		glm::vec4    Color{ 1.f };
	};

}
