#pragma once

#include "entt.hpp"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Physics/PhysicsWorld.h"

namespace Hominem {

	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = "");

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnUpdate(Timestep ts);
		void OnViewportResize(uint32_t width, uint32_t height);
	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		Ref<PhysicsWorld> m_PhysicsWorld = nullptr;

		friend class Entity;
	};
}
