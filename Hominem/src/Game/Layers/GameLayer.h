#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/CameraController.h"
#include "Game/GameMode.h"

namespace Hominem {

	class GameLayer : public Layer
	{
	public:
		GameLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override {}
		void OnEvent(Event& e) override;

	private:
		bool OnWindowResize(WindowResizeEvent& e);

		Ref<Scene> m_ActiveScene;
		Entity m_CameraEntity;
		EntityCameraController m_CameraController;
	};

}
