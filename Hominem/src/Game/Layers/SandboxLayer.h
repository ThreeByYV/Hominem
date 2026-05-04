#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Scene/Scene.h"
#include "Game/GameMode.h"

namespace Hominem {

	class SandboxLayer : public Layer
	{
	public:
		SandboxLayer();
		virtual ~SandboxLayer() = default;

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
		void OnEvent(Event& e) override;

	private:
		bool OnWindowResize(WindowResizeEvent& e);

		Ref<Scene>      m_ActiveScene;
		Scope<GameMode> m_GameMode;
	};

}
