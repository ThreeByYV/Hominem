#pragma once

#include "Hominem/Core/Layer.h"

namespace Hominem {

	class ImGuiLayer: public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnImGuiRender() override;

		//Layers can render ImGui things via starting with these two
		void Begin();
		void End();

	private:
		float m_Time = 0.0f;
	};

}
