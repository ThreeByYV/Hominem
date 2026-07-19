#pragma once

#include "Hominem/UI/Widget.h"
#include "Hominem/UI/UIRoot.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Utils/Transform.h"

namespace Hominem {

	/// A coloured rectangle, optionally textured. Size is the full extent, so a
	/// widget at Position {0,0} with Size {2,2} covers a square overlay exactly.
	class QuadWidget : public Widget
	{
	public:
		QuadWidget() = default;
		QuadWidget(glm::vec2 position, glm::vec2 size, glm::vec4 color)
			: Color(color)
		{
			Position = position;
			Size     = size;
		}

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			if (Color.a <= 0.f) return;

			QuadDraw draw;
			draw.transform = Transform::PosRotScale(glm::vec3(Position, Depth),
			                                        { 0.f, 0.f, RotationRadians },
			                                        glm::vec3(ResolvedSize(), 1.f)).ToMatrix();
			draw.color   = Color;
			draw.texture = Texture;
			frame.quads.push_back(std::move(draw));
		}

		bool ContainsPoint(glm::vec2 overlayPos) const override
		{
			const glm::vec2 half = ResolvedSize() * 0.5f;
			return overlayPos.x >= Position.x - half.x && overlayPos.x <= Position.x + half.x
			    && overlayPos.y >= Position.y - half.y && overlayPos.y <= Position.y + half.y;
		}

		glm::vec4      Color { 1.f };
		Ref<Texture2D> Texture;
		float          RotationRadians = 0.f;
		float          Depth           = 0.f;
		/// Recomputes Size from the UI root every frame so the quad always spans the
		/// window, whatever it is resized to.
		bool           StretchToViewport = false;

	protected:
		glm::vec2 ResolvedSize() const
		{
			if (!StretchToViewport || !m_UIRoot) return Size;
			return { UIRoot::kOverlayHeight * m_UIRoot->GetAspect(), UIRoot::kOverlayHeight };
		}
	};

}
