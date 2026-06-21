#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/MouseEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/Actors/SpriteActor.h"
#include "Hominem/Renderer/Font.h"

#include <array>

class MenuLayer : public Hominem::Layer
{
public:
	MenuLayer();

	void OnAttach()                                  override;
	void OnDetach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override {}
	void OnEvent(Hominem::Event& e)                  override;

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);
	bool OnMouseButtonPressed(Hominem::MouseButtonPressedEvent& e);

	void FireMenuAction(int index);

	static constexpr float k_OrthoSize  = 2.0f;

	// Items start flush with the title's left edge.
	static constexpr float k_TitleX     = -1.54f;
	static constexpr float k_ItemX      = k_TitleX;
	static constexpr float k_ArrowX     = k_TitleX - 0.16f;
	static constexpr float k_TitleY     =  0.38f;
	static constexpr float k_ItemScale  =  0.11f;
	static constexpr float k_TitleScale =  0.17f;

	// Divider line + diamond ornament between the title and the menu items.
	static constexpr float k_DividerGap    = 0.05f;  // vertical gap below the title baseline
	static constexpr float k_DividerWidth  = 0.85f;
	static constexpr float k_DividerHeight = 0.004f;
	static constexpr float k_DiamondSize   = 0.022f;
	static constexpr float k_HoverBand  =  0.085f;
	static constexpr float k_LerpSpeed  =  15.f;

	struct MenuItem { const char* label; float y; };
	static constexpr std::array<MenuItem, 5> k_Items = {{
		{ "Continue",  0.18f },
		{ "New Game",  0.01f },
		{ "Options",  -0.16f },
		{ "Credits",  -0.33f },
		{ "Quit",     -0.50f },
	}};

	Hominem::SpriteActor* m_Background = nullptr;
	Hominem::Ref<Hominem::Font>   m_Font;

	int      m_HoveredIndex = -1;
	float    m_ArrowY       = 0.18f;
	uint32_t m_ViewportW    = 1280;
	uint32_t m_ViewportH    = 720;
};
