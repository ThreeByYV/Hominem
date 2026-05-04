#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
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
		void OnImGuiRender() override {}
		void OnEvent(Event& e) override;

	private:
		bool OnWindowResize(WindowResizeEvent& e);

		Ref<Scene> m_ActiveScene;
		Entity m_MusicEntity;
	};

}
