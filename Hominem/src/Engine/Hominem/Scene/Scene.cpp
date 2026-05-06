#include "hmnpch.h"
#include "Scene.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"

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

	void Scene::OnDraw()
	{
		if (m_ViewportWidth == 0 || m_ViewportHeight == 0)
			return;

		// Build camera matrices from the stored viewport size.
		// OnViewportResize keeps m_Camera in sync; no GL calls needed here.
		glm::mat4 cameraTransform = glm::translate(glm::mat4(1.f), m_CameraPosition);
		glm::mat4 viewProjection  = m_Camera.GetProjectionMatrix() * glm::inverse(cameraTransform);

		Renderer2D::BeginScene(m_Camera, cameraTransform);
		for (auto& actor : m_Actors) actor->OnDraw2D();
		Renderer2D::EndScene();

		Renderer3D::BeginScene(viewProjection, m_CameraPosition);
		for (auto& actor : m_Actors) actor->OnDraw3D();
		Renderer3D::EndScene();
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth  = width;
		m_ViewportHeight = height;
		m_Camera.SetViewportSize(width, height);
	}

}
