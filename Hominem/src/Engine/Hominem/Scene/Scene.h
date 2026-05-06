#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/SceneCamera.h"
#include "Hominem/Physics/PhysicsWorld.h"

#include <vector>

namespace Hominem {

	class Scene
	{
	public:
		Scene();
		~Scene();

		/// Construct a T, insert it into the scene, call OnCreate(), and return a ref.
		template<typename T, typename... Args>
		T& SpawnActor(Args&&... args)
		{
			auto actor   = CreateScope<T>(std::forward<Args>(args)...);
			actor->m_Scene = this;
			T& ref = *actor;
			m_Actors.push_back(std::move(actor));
			ref.OnCreate();
			return ref;
		}

		/// Step physics (if set) then update all actors.
		void OnUpdate(Timestep ts);

		/// Render all actors (2D pass then 3D pass).
		void OnDraw();

		/// Resize the scene camera's viewport.
		void OnViewportResize(uint32_t width, uint32_t height);

		// Legacy stubs so SandboxLayer, MenuLayer, GameLayer compile unchanged.
		void OnRuntimeStart() {}
		void OnRuntimeStop()  {}

		SceneCamera&       GetCamera()         { return m_Camera; }
		const SceneCamera& GetCamera() const   { return m_Camera; }
		glm::vec3&         GetCameraPosition() { return m_CameraPosition; }

		void               SetPhysicsWorld(Ref<PhysicsWorld> world) { m_PhysicsWorld = world; }
		Ref<PhysicsWorld>  GetPhysicsWorld() const                  { return m_PhysicsWorld; }

	private:
		std::vector<Scope<Actor>> m_Actors;

		SceneCamera m_Camera;
		glm::vec3   m_CameraPosition{ 0.f };
		uint32_t    m_ViewportWidth  = 0;
		uint32_t    m_ViewportHeight = 0;

		Ref<PhysicsWorld> m_PhysicsWorld;
	};

}
