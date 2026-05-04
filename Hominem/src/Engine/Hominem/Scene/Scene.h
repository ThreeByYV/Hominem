#pragma once

#include "entt.hpp"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Scene/Systems/ISystem.h"

#include <vector>

namespace Hominem {

	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = "");
		Entity FindEntityByName(const std::string& name);

		template<typename T, typename... Args>
		T& RegisterSystem(Args&&... args)
		{
			auto system = CreateScope<T>(std::forward<Args>(args)...);
			T& ref = *system;
			m_Systems.push_back(std::move(system));
			return ref;
		}

		void OnRuntimeStart();
		void OnRuntimeStop();
		void OnUpdate(Timestep ts);
		void OnViewportResize(uint32_t width, uint32_t height);

	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		std::vector<Scope<ISystem>> m_Systems;

		friend class Entity;
	};

}
