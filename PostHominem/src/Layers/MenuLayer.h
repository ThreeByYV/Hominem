#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/Actors/SpriteActor.h"

class MenuLayer : public Hominem::Layer
{
public:
	MenuLayer();

	void OnAttach()                      override;
	void OnDetach()                      override;
	void OnUpdate(Hominem::Timestep ts)  override;
	void OnImGuiRender()                 override {}
	void OnEvent(Hominem::Event& e)      override;

private:
	bool OnWindowResize(Hominem::WindowResizeEvent& e);

	static constexpr float k_OrthoSize = 2.0f;

	Hominem::Ref<Hominem::Scene> m_ActiveScene;
	Hominem::SpriteActor*        m_Background = nullptr;
};
