#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Game/Level.h"

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

	Hominem::Ref<Hominem::Scene> m_ActiveScene;
	Hominem::Scope<Level>        m_GameMode;
};
