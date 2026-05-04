#pragma once
#include "ISystem.h"

namespace Hominem {

	class RenderSystem : public ISystem
	{
	public:
		void OnUpdate(Timestep ts, entt::registry& registry) override;
	};

}
