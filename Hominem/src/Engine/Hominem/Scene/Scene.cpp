#include "hmnpch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"

namespace Hominem {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
		// Safety: stop all systems if runtime is still active
		for (auto& system : m_Systems)
			system->OnRuntimeStop(m_Registry);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<TagComponent>().Tag = name.empty() ? "Unknown Entity" : name;
		return entity;
	}

	Entity Scene::FindEntityByName(const std::string& name)
	{
		auto view = m_Registry.view<TagComponent>();
		for (auto entity : view)
		{
			if (view.get<TagComponent>(entity).Tag == name)
				return { entity, this };
		}
		return {};
	}

	void Scene::OnRuntimeStart()
	{
		for (auto& system : m_Systems)
			system->OnRuntimeStart(m_Registry);
	}

	void Scene::OnRuntimeStop()
	{
		for (auto& system : m_Systems)
			system->OnRuntimeStop(m_Registry);
	}

	void Scene::OnUpdate(Timestep ts)
	{
		for (auto& system : m_Systems)
			system->OnUpdate(ts, m_Registry);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth  = width;
		m_ViewportHeight = height;

		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cam = view.get<CameraComponent>(entity);
			if (!cam.FixedAspectRatio)
				cam.Camera.SetViewportSize(width, height);
		}
	}

}
