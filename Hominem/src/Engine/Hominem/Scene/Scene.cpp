#include "hmnpch.h"
#include "Scene.h"

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
		if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
			return;

		glm::mat4 cameraTransform = glm::translate(glm::mat4(1.f), m_CameraPosition);
		glm::mat4 viewProjection  = m_Camera.GetProjectionMatrix() * glm::inverse(cameraTransform);

		frame.viewProjection2D = viewProjection;
		frame.viewProjection3D = viewProjection;
		frame.cameraWorldPos   = m_CameraPosition;
		frame.viewportWidth    = m_ViewportWidth;
		frame.viewportHeight   = m_ViewportHeight;

		for (auto& actor : m_Actors)
			actor->OnBuildRenderFrame(frame);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth  = width;
		m_ViewportHeight = height;
		m_Camera.SetViewportSize(width, height);
	}

}
