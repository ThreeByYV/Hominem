#pragma once

#include "Hominem/Core/Hominem.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/Entity.h"

namespace Hominem {

	class MenuLayer : public Layer
	{
	public:
		MenuLayer();

	    void OnAttach() override;
		void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
	private:
		OrthographicCameraController m_CameraController;

		Ref<Scene> m_Scene;
		Entity m_MusicEntity;
		Entity m_CameraEntity;
	};

}

