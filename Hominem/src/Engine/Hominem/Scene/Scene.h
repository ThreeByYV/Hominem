#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/SceneCamera.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Renderer/Texture.h"

#include <vector>
#include <concepts>
#include <memory>

namespace Hominem {

	class Scene : public RefCounted
	{
	public:
		Scene();
		~Scene();

		/// Construct a T, insert it into the scene, call OnCreate(), and return a ref.
		/// If a world transform has been set via SetWorldTransform, the actor's
		/// Position is transformed into world space automatically.
		template<typename T, typename... Args>
		requires std::derived_from<T, Actor>
		// T must derive from Actor — did you forget to inherit from Actor?
		T& SpawnActor(Args&&... args)
		{
			auto actor     = CreateScope<T>(std::forward<Args>(args)...);
			actor->m_Scene = this;
			actor->Position = glm::vec3(m_WorldTransform * glm::vec4(actor->Position, 1.f));
			T& ref = *actor;
			m_Actors.push_back(std::move(actor));
			ref.OnCreate();
			return ref;
		}

		/// Set a world-space transform for this scene. All actors spawned after this
		/// call will have their Position automatically transformed to world space,
		/// so actors can be authored in scene-local coordinates.
		void SetWorldTransform(const glm::mat4& transform) { m_WorldTransform = transform; }

		/// Step physics (if set) then update all actors.
		void OnUpdate(Timestep ts);

		/// Collect draw commands from all actors into frame — no GL calls.
		void BuildRenderFrame(RenderFrame& frame);

		/// Resize the scene camera's viewport.
		void OnViewportResize(uint32_t width, uint32_t height);

		// Legacy stubs so SandboxLayer, MenuLayer, GameLayer compile unchanged.
		void OnRuntimeStart() {}
		void OnRuntimeStop()  {}

		SceneCamera&       GetCamera()         { return m_Camera; }
		const SceneCamera& GetCamera() const   { return m_Camera; }
		glm::vec3&         GetCameraPosition() { return m_CameraPosition; }
		glm::vec3&         GetCameraFront()    { return m_CameraFront; }

		void               SetPhysicsWorld(Ref<PhysicsWorld> world) { m_PhysicsWorld = world; }
		Ref<PhysicsWorld>  GetPhysicsWorld() const                  { return m_PhysicsWorld; }

		// Request a runtime cubemap bake from capturePos (e.g. room center).
		// Bake executes on the render thread during the first frame after this call.
		// Reflections activate automatically once the bake completes.
		void BakeEnvironment(const glm::vec3& capturePos,
		                     float intensity   = 1.f,
		                     float eta         = 0.667f,
		                     uint32_t resolution = 512);

		// Assign a pre-loaded cubemap (e.g. TextureCube::Create from 6 files).
		void SetEnvMap(Ref<TextureCube> map,
		               float intensity   = 1.f,
		               float eta         = 0.667f,
		               float fresnelPower = 5.f);

		void SetEnvMapIntensity(float intensity) { m_EnvMapIntensity = intensity; }

	private:
		std::vector<Scope<Actor>> m_Actors;

		SceneCamera m_Camera;
		glm::vec3   m_CameraPosition{ 0.f };
		glm::vec3   m_CameraFront{ 0.f, 0.f, -1.f }; // default: looking down -Z
		uint32_t    m_ViewportWidth  = 0;
		uint32_t    m_ViewportHeight = 0;

		Ref<PhysicsWorld> m_PhysicsWorld;
		glm::mat4         m_WorldTransform{ 1.f };

		// Env map state
		bool                         m_BakeEnvPending  = false;
		glm::vec3                    m_BakeCapPos;
		uint32_t                     m_BakeResolution  = 512;
		std::shared_ptr<BakedEnvMap> m_BakedEnvMap;      // result from runtime bake
		Ref<TextureCube>             m_ExplicitEnvMap;   // assigned via SetEnvMap
		float                        m_EnvMapIntensity  = 1.f;
		float                        m_ETA              = 0.667f;
		float                        m_FresnelPower     = 5.f;
	};

}
