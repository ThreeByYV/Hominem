#pragma once

#include "Hominem.h"

namespace Hominem {

	class VoidLayer : public Layer
	{
	public:
		VoidLayer();

		void OnUpdate(Timestep ts) override;
		void OnImGuiRender() override;
	};
}
