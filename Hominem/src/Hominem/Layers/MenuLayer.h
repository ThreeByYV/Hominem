#pragma once

#include "Hominem/Core/Hominem.h"

namespace Hominem {

	class MenuLayer : public Layer
	{
	public:
		MenuLayer();

		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
	};

}

