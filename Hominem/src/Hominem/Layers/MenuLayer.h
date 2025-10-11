#pragma once

#include "Hominem/Core/Hominem.h"

namespace Hominem {

	class MenuLayer : public Layer
	{
	public:
		MenuLayer();

	    void OnAttach() override;

		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
	private: 
		OrthographicCameraController m_CameraController;
		Ref<Texture2D> m_BackgroundTexture;
	};

}

