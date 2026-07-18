#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Utils/MathUtils.h"

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class NarrativeTextActor : public Hominem::Actor
{
public:
	float     TextScale    = 0.35f;              // world-space scale for the font glyphs
	float     PanelAlpha   = 0.72f;              // opacity of the dark background panel
	glm::vec3 ScreenOffset = { 0.f, 2.75f, -4.f }; // offset from camera centre (world units)
	glm::vec2 PanelSize    = { 14.f, 2.0f };

	// Perspective scenes render 2D quads/text through a fixed screen-space ortho
	// (see Scene.cpp's viewProjection2D), decoupled from the 3D camera entirely — so
	// world-relative positioning (the default, correct for orthographic scenes) never
	// lands on screen. Set true to treat ScreenOffset as an absolute overlay-space position.
	bool      ScreenSpace  = false;
	float     DisplayDuration = 2.f; // seconds fully visible after Show() before fading; <=0 disables
	float     FadeDuration    = 0.5f; // seconds spent lerping alpha to 0 once DisplayDuration elapses

	void OnCreate() override
	{
		m_Font = Hominem::Font::GetDefaultFont();
	}

	void Show(const std::string& text) { m_Text = text; m_Visible = true; m_Timer = 0.f; }
	void Hide()                         { m_Visible = false; }
	bool IsVisible() const              { return m_Visible; }

	void OnUpdate(Hominem::Timestep ts) override
	{
		if (!m_Visible || !m_Scene) return;
		Position = ScreenSpace ? ScreenOffset : m_Scene->GetCameraPosition() + ScreenOffset;

		if (DisplayDuration > 0.f)
		{
			m_Timer += (float)ts;
			if (m_Timer >= DisplayDuration + FadeDuration)
				Hide();
		}
	}

	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override
	{
		if (!m_Visible || m_Text.empty()) return;

		float fadeAlpha = 1.f;
		if (DisplayDuration > 0.f && m_Timer > DisplayDuration)
			fadeAlpha = Hominem::Lerp(1.f, 0.f, Hominem::TimeProgress(m_Timer - DisplayDuration, FadeDuration));

		Hominem::QuadDraw panel;
		panel.transform = glm::translate(glm::mat4(1.f), Position)
		                * glm::scale(glm::mat4(1.f), glm::vec3(PanelSize, 1.f));
		panel.color = glm::vec4(0.f, 0.f, 0.f, PanelAlpha * fadeAlpha);
		frame.quads.push_back(std::move(panel));

		if (!m_Font) return;

		float halfWidth = m_Font->MeasureWidth(m_Text) * TextScale * 0.5f;
		glm::vec3 textPos = Position + glm::vec3(-halfWidth, 0.f, 0.5f);
		glm::mat4 textTransform = glm::translate(glm::mat4(1.f), textPos)
		                        * glm::scale(glm::mat4(1.f), glm::vec3(TextScale));

		frame.texts.push_back({ m_Text, m_Font, textTransform, glm::vec4(1.f, 1.f, 1.f, fadeAlpha) });
	}

private:
	std::string                  m_Text;
	bool                         m_Visible = false;
	float                        m_Timer   = 0.f;
	Hominem::Ref<Hominem::Font>  m_Font;
};
