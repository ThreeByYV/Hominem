#include "hmnpch.h"

#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "Hominem/Renderer/Renderer2D.h"

namespace Hominem {

	Scene::Scene()
	{
		entt::entity entity = m_Registry.create();
		m_Registry.emplace<TransformComponent>(entity, glm::mat4(1.0f));
		
	}

	void Scene::OnUpdate(Timestep ts)
	{
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);	
		for (auto entity : group)
		{
			auto&& [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			
			Renderer2D::DrawQuad(transform, sprite.Color);
		}
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };

		//all entities will have a name and a transform
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();

		tag.Tag = name.empty() ? "Unknown Entity" : name;

		return entity;
	}

	Scene::~Scene()
	{

	}


}