#pragma once
#include "ISystem.h"
#include "Hominem/Physics/PhysicsWorld.h"

namespace Hominem {

	class PhysicsSystem : public ISystem
	{
	public:
		void OnRuntimeStart(entt::registry& registry) override;
		void OnRuntimeStop(entt::registry& registry) override;
		void OnUpdate(Timestep ts, entt::registry& registry) override;

	private:
		Ref<PhysicsWorld> m_World;
	};

}
