#pragma once

#include "Hominem/Cinematics/Cue.h"
#include "Hominem/Renderer/Texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <string>
#include <utility>

namespace Hominem {

	/**
	 * @brief Draws a textured quad for the cue's duration, with optional flicker
	 *        (a cheap brightness/scale wobble for fire, torches, etc.).
	 */
	class SpriteCue : public Cue
	{
	public:
		SpriteCue() = default;
		SpriteCue(Ref<Texture2D> tex, glm::vec3 pos, glm::vec2 size,
		          glm::vec4 tint = glm::vec4(1.f), bool flicker = false)
			: Tex(std::move(tex)), Pos(pos), Size(size), Tint(tint), Flicker(flicker)
		{
		}

		const char* TypeName() const override { return "sprite"; }

		void OnBuildRenderFrame(RenderFrame& frame, float localT) override
		{
			if (!Tex)
				return;

			const float f = Flicker ? (0.9f + 0.1f * std::sin(localT * 30.f)) : 1.f;

			QuadDraw q;
			q.transform = glm::translate(glm::mat4(1.f), Pos)
			            * glm::scale(glm::mat4(1.f), { Size.x * f, Size.y * f, 1.f });
			q.color   = Tint * glm::vec4(f, f, f, 1.f);
			q.texture = Tex;
			frame.quads.push_back(std::move(q));
		}

		Ref<Texture2D> Tex;
		std::string    TexPath;          ///< Source path, for save/reload.
		glm::vec3      Pos{ 0.f };
		glm::vec2      Size{ 1.f };
		glm::vec4      Tint{ 1.f };
		bool           Flicker = false;
	};

}
