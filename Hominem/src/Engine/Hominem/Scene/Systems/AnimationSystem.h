#pragma once
#include "ISystem.h"
#include <glm/glm.hpp>
#include <vector>

namespace Hominem {

	class AnimationSystem : public ISystem
	{
	public:
		void OnUpdate(Timestep ts, entt::registry& registry) override;

	private:
		std::vector<glm::mat4> m_BoneCache; // reused per entity per frame — no heap alloc
	};

}
