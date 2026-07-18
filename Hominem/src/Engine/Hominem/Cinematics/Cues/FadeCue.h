#pragma once

#include "Hominem/Cinematics/Cue.h"
#include "Hominem/Utils/MathUtils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <utility>

namespace Hominem {

	class FadeCue : public Cue
	{
	public:
		FadeCue() = default;
		FadeCue(glm::vec3 color, float fromAlpha, float toAlpha)
			: Color(color), From(fromAlpha), To(toAlpha)
		{
		}

		const char* TypeName() const override { return "fade"; }

		void OnBuildRenderFrame(RenderFrame& frame, float localT) override
		{
			const float t     = TimeProgress(localT, Duration);
			const float alpha = Lerp(From, To, t);
			if (alpha <= 0.001f)
				return;

			QuadDraw q;

			q.transform = glm::translate(glm::mat4(1.f), { 0.f, 0.f, k_Z })
			            * glm::scale(glm::mat4(1.f), { k_Overscan, k_Overscan, 1.f });
			q.color   = { Color.r, Color.g, Color.b, alpha };

			q.texture = nullptr; // solid color

			frame.quads.push_back(std::move(q));
		}

		glm::vec3 Color{ 0.f };
		float     From = 0.f;
		float     To   = 1.f;

	private:
		static constexpr float k_Z        = 0.0f;  ///< In front of sprite content (which sits at z < 0).
		static constexpr float k_Overscan = 20.0f; ///< Far larger than the ortho view; covers any aspect.
	};

}
