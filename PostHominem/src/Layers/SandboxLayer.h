#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Game/Level.h"

class SandboxLayer : public Hominem::Layer
{
public:
	SandboxLayer();
	virtual ~SandboxLayer() = default;

	void OnAttach()                      override;
	void OnDetach()                      override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
	void OnEvent(Hominem::Event& e)                  override;

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);

	Hominem::Ref<Hominem::Scene> m_ActiveScene;
	Hominem::Scope<Level>        m_GameMode;
};
