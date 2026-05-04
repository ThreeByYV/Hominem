#pragma once
#include "ISystem.h"

namespace Hominem {

	class AudioSourceSystem : public ISystem
	{
	public:
		void OnRuntimeStop(entt::registry& registry) override;
		void OnUpdate(Timestep ts, entt::registry& registry) override;
	};

}
