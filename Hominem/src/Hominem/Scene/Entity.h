// #pragma once
//
//#include "Scene.h"
//#include <entt/entt.hpp>
// 
//namespace Hominem {
//
//	class Entity
//	{
//	public:
//		Entity(entt::entity handle, Scene* scene);
//		Entity(const Entity& other) = default;
//
//		template<typename T, typename... Args>
//		T& AddComponent(Args&&... args)
//		{
//			return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
//		}
//
//		template<typename T>
//		T& GetComponent()
//		{
//			return m_Scene->m_Registry.get<T>(m_EntityHandle);
//		}
//
//		template<typename T>
//		bool HasComponent()
//		{
//			return m_Scene->m_Registry.has<T>(m_EntityHandle);
//		}
//
//
//		template<typename T>
//		void RemoveComponent()
//		{
//			return m_Scene->m_Registry.remove<T>(m_EntityHandle);
//		}
//
//	private:
//		entt::entity m_EntityHandle;
//		Scene* m_Scene; 
//	};
//};