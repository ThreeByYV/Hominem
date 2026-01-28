#pragma once

#include "Hominem/Core/Hominem.h"

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
		Ref<Texture2D> m_BackgroundTexture;

		// Audio system
		AudioSystem m_AudioSystem;
	};

}

