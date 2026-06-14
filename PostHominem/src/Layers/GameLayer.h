#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Game/Level.h"
#include "Game/WorldConfig.h"
#include "Hominem/Events/MouseEvent.h"
#include <glm/glm.hpp>

class GameLayer : public Hominem::Layer
{
public:
	GameLayer();

	void OnAttach()                                  override;
	void OnDetach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;


	static inline bool      s_SkipIntro  = false;
	static inline glm::vec3 s_EyeTarget  = { 0.084f, 0.8f, 40.0f };

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);
	bool OnKeyPressed(Hominem::KeyPressedEvent& e);
	bool OnMouseMoved(Hominem::MouseMovedEvent& e);

	enum class IntroPhase { Wait, ZoomIn, Flash, ZoomOut, Done };
	IntroPhase  m_IntroPhase  = IntroPhase::Wait;
	float       m_IntroTimer  = 0.f;
	glm::vec3   m_IntroFromPos{ 0.f };
	float       m_IntroFromZoom = 10.f;

	static constexpr glm::vec3 k_EyeTarget  = { 0.084f, 0.8f, 29.0f };
	static constexpr float     k_EyeZoom    = 0.08f;  // ortho size at peak
	static constexpr float     k_WaitDur    = 2.5f;   // seconds of normal play before zoom starts
	static constexpr float     k_ZoomDur    = 0.7f;   // seconds to zoom in/out
	static constexpr float     k_FlashDur   = 0.15f;  // white hold before cut

	Hominem::Ref<Hominem::Scene>    m_ActiveScene;
	Hominem::Scope<Level>           m_GameMode;
	bool  m_ShowDebugUI   = false;
	bool  m_ShowPerfPanel = true;
	float m_FPS           = 0.f;
	float m_FrameTimeMs   = 0.f;
	Hominem::DirectionalLight      m_Light;
	std::vector<Hominem::Light>    m_Lights;
	int                            m_SelectedLight = -1;
	bool                           m_DebugLights   = false;

	bool  m_BloomEnabled       = false;
	bool  m_ToneMappingEnabled = false;
	float m_BloomStrength      = 1.0f;
	float m_BloomThreshold     = 0.8f;
	float m_RenderScale        = 1.0f;
	float m_EnvMapIntensity    = 1.0f;
};
