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

	struct PostProcessSettings
	{
		bool  bloomEnabled       = false;
		bool  toneMappingEnabled = false;
		float bloomStrength      = 1.0f;
		float bloomThreshold     = 0.8f;
		float renderScale        = 1.0f;
		bool  debugLights        = false;
	};

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

		/// Position + orthographic zoom, bundled so callers don't need two separate
		/// members and two separate get/set calls to snapshot and restore camera state.
		struct CameraSnapshot
		{
			glm::vec3 position{ 0.f };
			float     orthoSize = 10.f;
		};

		[[nodiscard]] CameraSnapshot GetCameraSnapshot() const
		{
			return { m_CameraPosition, m_Camera.GetOrthographicSize() };
		}

		void ApplyCameraSnapshot(const CameraSnapshot& snap)
		{
			m_CameraPosition = snap.position;
			m_Camera.SetOrthographicSize(snap.orthoSize);
		}

		/// Convert a screen-space pixel coordinate (origin top-left) to world space,
		/// assuming an orthographic camera centered on the viewport.
		[[nodiscard]] glm::vec2 ScreenToWorld(float screenX, float screenY) const
		{
			const float halfH = m_Camera.GetOrthographicSize() * 0.5f;
			const float halfW = halfH * (m_ViewportHeight > 0
				? (float)m_ViewportWidth / (float)m_ViewportHeight : 1.f);
			const float ndcX = m_ViewportWidth  > 0 ? (screenX / (float)m_ViewportWidth)  * 2.f - 1.f : 0.f;
			const float ndcY = m_ViewportHeight > 0 ? 1.f - (screenY / (float)m_ViewportHeight) * 2.f : 0.f;
			return { ndcX * halfW, ndcY * halfH };
		}

		[[nodiscard]] glm::vec2 ScreenToWorld(glm::vec2 screenPos) const
		{
			return ScreenToWorld(screenPos.x, screenPos.y);
		}

		void               SetPhysicsWorld(Ref<PhysicsWorld> world) { m_PhysicsWorld = world; }
		Ref<PhysicsWorld>  GetPhysicsWorld() const                  { return m_PhysicsWorld; }

		void SetClearColor(const glm::vec4& c)            { m_ClearColor = c; }
		void SetSkybox(Ref<Skybox> s, float intensity = 1.f) { m_Skybox = std::move(s); m_SkyboxIntensity = intensity; }
		void SetDirectionalLight(const DirectionalLight& l)  { m_DirectionalLight = l; }
		void SetPostProcess(const PostProcessSettings& pp)   { m_PostProcess = pp; }

		DirectionalLight&         GetDirectionalLight() { return m_DirectionalLight; }
		std::vector<Light>&       GetLights()           { return m_SceneLights; }
		PostProcessSettings&      GetPostProcess()      { return m_PostProcess; }

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

		glm::vec4           m_ClearColor      { 0.1f, 0.1f, 0.1f, 1.f };
		DirectionalLight    m_DirectionalLight;
		Ref<Skybox>         m_Skybox;
		float               m_SkyboxIntensity = 1.f;
		PostProcessSettings m_PostProcess;
		std::vector<Light>  m_SceneLights;

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
