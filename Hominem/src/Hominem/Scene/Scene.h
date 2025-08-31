#pragma once

#include "entt.hpp"
#include "Hominem/Core/Timestep.h"

namespace Hominem {

	class Entity;
		
	class Scene
	{
	public:
		Scene();

		Entity CreateEntity(const std::string& name = "");

		void OnUpdate(Timestep ts);

		~Scene();

	private:
		entt::registry m_Registry;

		friend class Entity;
	};
}
