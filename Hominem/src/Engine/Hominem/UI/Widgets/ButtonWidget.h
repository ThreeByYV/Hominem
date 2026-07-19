#pragma once

#include "Hominem/UI/Widgets/TextWidget.h"

#include <functional>

namespace Hominem {

	/// Text that reacts to the pointer and reports clicks. Hit area comes from
	/// Widget::Size, which is independent of the measured text so rows can have a
	/// comfortable band without changing the glyph layout.
	class ButtonWidget : public TextWidget
	{
	public:
		using ClickHandler = std::function<void()>;

		ButtonWidget() = default;
		ButtonWidget(std::string label, glm::vec2 position, float scale, ClickHandler onClick)
			: TextWidget(std::move(label), position, scale), OnClick(std::move(onClick))
		{}

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			Color      = m_Hovered ? HoverColor      : NormalColor;
			ColorRight = m_Hovered ? HoverColorRight : glm::vec4(-1.f);
			TextWidget::OnBuildRenderFrame(frame);
		}

		void OnPointerEnter() override { m_Hovered = true; }
		void OnPointerExit()  override { m_Hovered = false; }

		bool OnPointerDown() override
		{
			if (OnClick) OnClick();
			return true;
		}

		bool IsHovered() const { return m_Hovered; }

		glm::vec4    NormalColor     { 0.75f, 0.75f, 0.75f, 1.f };
		glm::vec4    HoverColor      { 0.95f, 0.55f, 0.25f, 1.f };
		glm::vec4    HoverColorRight { 0.75f, 0.75f, 0.75f, 1.f };
		ClickHandler OnClick;

	private:
		bool m_Hovered = false;
	};

}
