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

	void Scene::BuildRenderFrame(RenderFrame& frame)
	{
		frame.viewportWidth  = m_ViewportWidth;
		frame.viewportHeight = m_ViewportHeight;

		if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
			return;

		glm::mat4 view            = glm::lookAt(m_CameraPosition, m_CameraPosition + m_CameraFront, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 viewProjection  = m_Camera.GetProjectionMatrix() * view;

		frame.viewProjection2D = viewProjection;
		frame.viewProjection3D = viewProjection;
		frame.cameraWorldPos   = m_CameraPosition;
		frame.frustum3D        = Frustum::FromViewProjection(viewProjection);

		for (auto& actor : m_Actors)
			actor->OnBuildRenderFrame(frame);

		// Sort static meshes by key: groups same shader + same mesh together,
		// minimising redundant shader/texture binds on the render thread.
		std::sort(frame.staticMeshes.begin(), frame.staticMeshes.end(),
			[](const StaticMeshDraw& a, const StaticMeshDraw& b) {
				return a.sortKey < b.sortKey;
			});
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth  = width;
		m_ViewportHeight = height;
		m_Camera.SetViewportSize(width, height);
	}

}
