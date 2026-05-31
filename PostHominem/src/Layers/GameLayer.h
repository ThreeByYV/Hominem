#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Game/Level.h"
#include "Game/WorldConfig.h"

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

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);
	bool OnKeyPressed(Hominem::KeyPressedEvent& e);

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
};
