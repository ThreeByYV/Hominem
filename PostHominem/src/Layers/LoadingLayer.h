#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Renderer/Font.h"

/// @brief Shown right after the menu while CutscenePreload finishes importing assets
class LoadingLayer : public Hominem::Layer
{
public:
	LoadingLayer();

	void OnAttach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;

private:
	static constexpr const char* k_Quote =
		"\"What does not destroy me, makes me stronger.\"  - Nietzsche";

	Hominem::Ref<Hominem::Font> m_Font;
	float m_PulseT        = 0.f;
	bool  m_Transitioning = false;
};
