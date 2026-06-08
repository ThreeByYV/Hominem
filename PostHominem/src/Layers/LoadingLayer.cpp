#include "hmnpch.h"
#include "LoadingLayer.h"
#include "GameLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Renderer/Font.h"
#include "Game/CutscenePreload.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Hominem;

LoadingLayer::LoadingLayer()
	: Layer("Loading")
{
}

void LoadingLayer::OnAttach()
{
	m_Font = Font::GetDefaultFont();
}

void LoadingLayer::OnUpdate(Timestep ts)
{
	m_PulseT += (float)ts;

	if (!m_Transitioning && CutscenePreload::TryGet())
	{
		m_Transitioning = true;
		TransitionTo<GameLayer>();
	}
}

void LoadingLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	auto& window = Application::Get().GetWindow();
	frame.viewportWidth  = window.GetWidth();
	frame.viewportHeight = window.GetHeight();
	frame.clearColor     = { 0.f, 0.f, 0.f, 1.f };


	if (!m_Font || frame.viewportHeight == 0)
		return;

	// No Scene here to derive a view-projection from, so build a screen-space ortho
	// directly for the 2D text.
	const float aspect = (float)frame.viewportWidth / (float)frame.viewportHeight;
	frame.viewProjection2D = glm::ortho(-aspect, aspect, -1.f, 1.f, -1.f, 1.f);

	const std::string text = k_Quote;

	constexpr float kTextScale   = 0.1f;
	constexpr float kBottomMargin = 0.15f;
	const float halfWidth = m_Font->MeasureWidth(text) * 0.5f * kTextScale;

	// Slow breathing fade on text here
	const float pulse = 0.3f + 0.5f * (0.5f + 0.5f * std::sin(m_PulseT * 1.5f));

	glm::mat4 transform = glm::translate(glm::mat4(1.f), { -halfWidth, -1.f + kBottomMargin, 0.f })
	                    * glm::scale(glm::mat4(1.f), glm::vec3(kTextScale));
	frame.texts.push_back({ text, m_Font, transform, { 1.f, 1.f, 1.f, pulse } });
}
