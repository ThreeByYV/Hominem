#include "hmnpch.h"
#include "Scene.h"
#include "Hominem/Renderer/Frustum.h"

namespace Hominem {

	Scene::Scene()  = default;
	Scene::~Scene() = default;

	void Scene::OnUpdate(Timestep ts)
	{
		if (m_PhysicsWorld)
			m_PhysicsWorld->Step(ts);

		for (auto& actor : m_Actors)
			actor->OnUpdate(ts);
	}

	void Scene::BakeEnvironment(const glm::vec3& capturePos, float intensity,
	                             float eta, uint32_t resolution)
	{
		m_BakeCapPos      = capturePos;
		m_BakeResolution  = resolution;
		m_EnvMapIntensity = intensity;
		m_ETA             = eta;
		m_BakedEnvMap     = std::make_shared<BakedEnvMap>();
		m_BakeEnvPending  = true;
	}

	void Scene::SetEnvMap(Ref<TextureCube> map, float intensity, float eta, float fresnelPower)
	{
		m_ExplicitEnvMap  = std::move(map);
		m_EnvMapIntensity = intensity;
		m_ETA             = eta;
		m_FresnelPower    = fresnelPower;
	}

	void Scene::BuildRenderFrame(RenderFrame& frame)
	{
		frame.clearColor         = m_ClearColor;
		frame.light              = m_DirectionalLight;
		frame.skybox             = m_Skybox;
		frame.skyboxIntensity    = m_SkyboxIntensity;
		frame.lights             = m_SceneLights;
		frame.bloomEnabled       = m_PostProcess.bloomEnabled;
		frame.toneMappingEnabled = m_PostProcess.toneMappingEnabled;
		frame.bloomStrength      = m_PostProcess.bloomStrength;
		frame.bloomThreshold     = m_PostProcess.bloomThreshold;
		frame.renderScale        = m_PostProcess.renderScale;
		frame.debugLights        = m_PostProcess.debugLights;
		frame.bottomBarFraction  = m_PostProcess.bottomBarFraction;

		frame.viewportWidth  = m_ViewportWidth;
		frame.viewportHeight = m_ViewportHeight;

		if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
			return;

		glm::mat4 view           = glm::lookAt(m_CameraPosition, m_CameraPosition + m_CameraFront, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 proj           = m_Camera.GetProjectionMatrix();
		glm::mat4 viewProjection = proj * view;

		// Perspective matrices warp full-screen 2D quads into 3D space, so give
		// perspective scenes a separate screen-space ortho projection for 2D content.
		if (m_Camera.GetProjectionType() == Camera::ProjectionType::Perspective)
		{
			const float aspect = (m_ViewportHeight > 0)
				? (float)m_ViewportWidth / (float)m_ViewportHeight : 1.f;
			constexpr float kOverlayHeight = 2.0f; // matches MenuLayer/PlayLevel ortho size
			frame.viewProjection2D = glm::ortho(-kOverlayHeight * aspect * 0.5f, kOverlayHeight * aspect * 0.5f,
			                                     -kOverlayHeight * 0.5f,         kOverlayHeight * 0.5f, -1.f, 1.f);
		}
		else
		{
			frame.viewProjection2D = viewProjection;
		}
		frame.viewProjection3D = viewProjection;
		frame.view3D           = view;
		frame.proj3D           = proj;
		frame.cameraWorldPos   = m_CameraPosition;
		frame.frustum3D        = Frustum::FromViewProjection(viewProjection);

		// Each actor's draws are matched against the ones it pushed last frame, by position
		// within its own contribution, to recover the transform they had then. TAA needs it
		// to tell an object that moved from one the camera merely moved past. Falls back to
		// the current transform (zero velocity) whenever an actor's draw count changes.
		for (auto& actor : m_Actors)
		{
			const size_t staticBegin = frame.staticMeshes.size();
			const size_t skinnedBegin = frame.meshes.size();

			actor->OnBuildRenderFrame(frame);

			auto& prev = m_PrevDrawTransforms[actor.get()];
			const size_t staticCount  = frame.staticMeshes.size() - staticBegin;
			const size_t skinnedCount = frame.meshes.size()       - skinnedBegin;
			const bool   matches      = prev.size() == staticCount + skinnedCount;

			for (size_t i = 0; i < staticCount; i++)
			{
				auto& draw = frame.staticMeshes[staticBegin + i];
				draw.prevTransform = matches ? prev[i] : draw.transform;
			}
			for (size_t i = 0; i < skinnedCount; i++)
			{
				auto& draw = frame.meshes[skinnedBegin + i];
				draw.prevTransform = matches ? prev[staticCount + i] : draw.transform;
			}

			prev.clear();
			prev.reserve(staticCount + skinnedCount);
			for (size_t i = 0; i < staticCount; i++)
				prev.push_back(frame.staticMeshes[staticBegin + i].transform);
			for (size_t i = 0; i < skinnedCount; i++)
				prev.push_back(frame.meshes[skinnedBegin + i].transform);
		}

		// Sort static meshes by key: groups same shader + same mesh together,
		// minimising redundant shader/texture binds on the render thread.
		//todo: why does the scene knwo about shaders? feel like scene should be higher lvl then shaders
		std::sort(frame.staticMeshes.begin(), frame.staticMeshes.end(),
			[](const StaticMeshDraw& a, const StaticMeshDraw& b) {
				return a.sortKey < b.sortKey;
			});

		// Env map — pass bake request on the frame it was requested, then track the result.
		if (m_BakeEnvPending)
		{
			frame.bakeEnvMap     = true;
			frame.bakeCapPos     = m_BakeCapPos;
			frame.bakeResolution = m_BakeResolution;
			frame.bakedEnvMapOut = m_BakedEnvMap;
			m_BakeEnvPending     = false;
		}

		// Pick the active env map: baked result takes priority over explicit assignment.
		// Irradiance is only available from a runtime bake (explicit cubemaps have no convolution).
		Ref<TextureCube> activeMap;
		Ref<TextureCube> activeIrradiance;
		Ref<TextureCube> activePrefiltered;
		if (m_BakedEnvMap && m_BakedEnvMap->ready.load(std::memory_order_acquire))
		{
			activeMap         = m_BakedEnvMap->map;
			activeIrradiance  = m_BakedEnvMap->irradiance;
			activePrefiltered = m_BakedEnvMap->prefiltered;
		}
		else if (m_ExplicitEnvMap)
			activeMap = m_ExplicitEnvMap;

		if (activeMap)
		{
			frame.envMap          = activeMap;
			frame.irradianceMap   = activeIrradiance;
			frame.prefilteredMap  = activePrefiltered;
			frame.envMapIntensity = m_EnvMapIntensity;
			frame.eta             = m_ETA;
			frame.fresnelPower    = m_FresnelPower;
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth  = width;
		m_ViewportHeight = height;
		m_Camera.SetViewportSize(width, height);
	}

}
