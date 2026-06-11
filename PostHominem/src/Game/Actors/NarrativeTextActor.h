#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Font.h"
#include "Hominem/Renderer/MSDFData.h"

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class NarrativeTextActor : public Hominem::Actor
{
public:
	float     TextScale    = 0.35f;              // world-space scale for the font glyphs
	float     PanelAlpha   = 0.72f;              // opacity of the dark background panel
	glm::vec3 ScreenOffset = { 0.f, 2.75f, -4.f }; // offset from camera centre (world units)

	void OnCreate() override
	{
		m_Font = Hominem::Font::GetDefaultFont();
	}

	void Show(const std::string& text) { m_Text = text; m_Visible = true; }
	void Hide()                         { m_Visible = false; }
	bool IsVisible() const              { return m_Visible; }

	void OnUpdate(Hominem::Timestep) override
	{
		if (!m_Visible || !m_Scene) return;
		Position = m_Scene->GetCameraPosition() + ScreenOffset;
	}

	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override
	{
		if (!m_Visible || m_Text.empty()) return;

		Hominem::QuadDraw panel;
		panel.transform = glm::translate(glm::mat4(1.f), Position)
		                * glm::scale(glm::mat4(1.f), glm::vec3(14.f, 2.0f, 1.f));
		panel.color = glm::vec4(0.f, 0.f, 0.f, PanelAlpha);
		frame.quads.push_back(std::move(panel));

		if (!m_Font) return;

		// Compute text half-width for centering — CPU math only, no GL.
		float halfWidth = 0.f;
		{
			const auto& fg  = m_Font->GetMSDFData()->FontGeometry;
			const auto& met = fg.getMetrics();
			double fsS  = 1.0 / (met.ascenderY - met.descenderY);
			double xAcc = 0.0;
			for (size_t i = 0; i < m_Text.size(); i++)
			{
				auto g = fg.getGlyph(m_Text[i]);
				if (!g) g = fg.getGlyph('?');
				if (!g) continue;
				double adv = g->getAdvance();
				if (i + 1 < m_Text.size())
					fg.getAdvance(adv, m_Text[i], m_Text[i + 1]);
				xAcc += fsS * adv;
			}
			halfWidth = static_cast<float>(xAcc * 0.5 * TextScale);
		}

		glm::vec3 textPos = Position + glm::vec3(-halfWidth, 0.f, 0.5f);
		glm::mat4 textTransform = glm::translate(glm::mat4(1.f), textPos)
		                        * glm::scale(glm::mat4(1.f), glm::vec3(TextScale));

		frame.texts.push_back({ m_Text, m_Font, textTransform, glm::vec4(1.f) });
	}

private:
	std::string                  m_Text;
	bool                         m_Visible = false;
	Hominem::Ref<Hominem::Font>  m_Font;
};
