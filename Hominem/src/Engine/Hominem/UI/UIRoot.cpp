#include "hmnpch.h"
#include "Hominem/UI/UIRoot.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/MouseCodes.h"
#include "Hominem/Renderer/RenderFrame.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

void UIRoot::OnViewportResize(uint32_t width, uint32_t height)
{
	m_ViewportWidth  = width;
	m_ViewportHeight = height;
	m_Aspect         = height > 0 ? (float)width / (float)height : 1.f;
}

glm::vec2 UIRoot::ScreenToOverlay(float screenX, float screenY) const
{
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0) return { 0.f, 0.f };

	const float halfWidth = kOverlayHeight * m_Aspect * 0.5f;
	const float ndcX      = screenX / (float)m_ViewportWidth  * 2.f - 1.f;
	const float ndcY      = 1.f - screenY / (float)m_ViewportHeight * 2.f;
	return { ndcX * halfWidth, ndcY * kOverlayHeight * 0.5f };
}

void UIRoot::OnUpdate(Timestep ts)
{
	for (auto& widget : m_Widgets)
		if (widget->Visible)
			widget->OnUpdate(ts);

	OnPointerMove(Input::GetMouseX(), Input::GetMouseY());

	const bool pointerDown = Input::IsMouseButtonPressed(Mouse::ButtonLeft);
	if (pointerDown && !m_PointerWasDown)
		OnPointerDown();
	m_PointerWasDown = pointerDown;
}

void UIRoot::BuildRenderFrame(RenderFrame& frame)
{
	if (m_ViewportWidth == 0 || m_ViewportHeight == 0) return;

	const float halfWidth = kOverlayHeight * m_Aspect * 0.5f;
	frame.viewProjection2D = glm::ortho(-halfWidth, halfWidth,
	                                    -kOverlayHeight * 0.5f, kOverlayHeight * 0.5f,
	                                    -1.f, 1.f);

	frame.viewportWidth  = m_ViewportWidth;
	frame.viewportHeight = m_ViewportHeight;

	if (ClearColor) frame.clearColor = *ClearColor;

	for (auto& widget : m_Widgets)
		if (widget->Visible)
			widget->OnBuildRenderFrame(frame);
}

void UIRoot::OnPointerMove(float screenX, float screenY)
{
	m_PointerOverlayPos = ScreenToOverlay(screenX, screenY);

	Widget* hovered = nullptr;
	for (auto it = m_Widgets.rbegin(); it != m_Widgets.rend(); ++it)
	{
		Widget& widget = **it;
		if (widget.Visible && widget.ContainsPoint(m_PointerOverlayPos))
		{
			hovered = &widget;
			break;
		}
	}

	if (hovered == m_HoveredWidget) return;

	if (m_HoveredWidget) m_HoveredWidget->OnPointerExit();
	m_HoveredWidget = hovered;
	if (m_HoveredWidget) m_HoveredWidget->OnPointerEnter();
}

bool UIRoot::OnPointerDown()
{
	return m_HoveredWidget && m_HoveredWidget->OnPointerDown();
}

}
