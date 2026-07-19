#pragma once

#include "Hominem/UI/Widget.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Utils/Transform.h"

#include <algorithm>
#include <string>

namespace Hominem {

	/// A single run of text. HorizontalAlignment decides what Position refers to:
	/// the left edge, the centre, or the right edge of the measured string.
	class TextWidget : public Widget
	{
	public:
		enum class Alignment { Left, Center, Right };
		enum class VerticalAnchor { Baseline, BottomOfBlock };

		TextWidget() = default;
		TextWidget(std::string text, glm::vec2 position, float scale,
		           glm::vec4 color = glm::vec4(1.f))
			: Text(std::move(text)), Scale(scale), Color(color)
		{
			Position = position;
		}

		void OnCreate() override
		{
			if (!FontRef) FontRef = Font::GetDefaultFont();
		}

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			if (!FontRef || Text.empty()) return;

			TextDraw draw;
			draw.text       = Text;
			draw.font       = FontRef;
			draw.transform  = Transform::PosScale(glm::vec3(AlignedPosition(), 0.f),
			                                      glm::vec3(Scale)).ToMatrix();
			draw.color      = Color;
			draw.colorRight = ColorRight;
			frame.texts.push_back(std::move(draw));
		}

		/// Width of the longest line, so centring works for multi-line strings.
		float MeasureWidth() const
		{
			if (!FontRef) return 0.f;

			float widest = 0.f;
			size_t lineStart = 0;
			for (size_t i = 0; i <= Text.size(); i++)
			{
				if (i == Text.size() || Text[i] == '\n')
				{
					widest = std::max(widest, FontRef->MeasureWidth(Text.substr(lineStart, i - lineStart)));
					lineStart = i + 1;
				}
			}
			return widest * Scale;
		}

		int CountLines() const
		{
			return 1 + (int)std::count(Text.begin(), Text.end(), '\n');
		}

		std::string    Text;
		Ref<Font>      FontRef;
		float          Scale      = 0.1f;
		glm::vec4      Color      { 1.f };
		glm::vec4      ColorRight { -1.f };
		Alignment      HorizontalAlignment = Alignment::Left;
		VerticalAnchor VerticalAlignment   = VerticalAnchor::Baseline;
		float          LineSpacing         = 1.3f;

	private:
		glm::vec2 AlignedPosition() const
		{
			glm::vec2 pos = Position;

			switch (HorizontalAlignment)
			{
				case Alignment::Center: pos.x -= MeasureWidth() * 0.5f; break;
				case Alignment::Right:  pos.x -= MeasureWidth();        break;
				default: break;
			}

			if (VerticalAlignment == VerticalAnchor::BottomOfBlock)
				pos.y += (CountLines() - 1) * Scale * LineSpacing;

			return pos;
		}
	};

}
