#include "hmnpch.h"
#include "RenderSystem.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"

namespace Hominem {

	void RenderSystem::OnUpdate(Timestep ts, entt::registry& registry)
	{
		// Find primary camera
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;

		auto cameraView = registry.view<TransformComponent, CameraComponent>();
		for (auto entity : cameraView)
		{
			auto [transform, camera] = cameraView.get<TransformComponent, CameraComponent>(entity);
			if (camera.Primary)
			{
				mainCamera     = &camera.Camera;
				cameraTransform = transform.GetTransform();
				break;
			}
		}

		if (!mainCamera)
		{
			HMN_CORE_WARN("RenderSystem: No primary camera found");
			return;
		}

		// 2D pass
		Renderer2D::BeginScene(*mainCamera, cameraTransform);

		{
			auto group = registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				if (sprite.Texture)
					Renderer2D::DrawQuad(transform.GetTransform(), sprite.Texture, sprite.Color);
				else
					Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
			}
		}

		{
			auto view = registry.view<TransformComponent, TextRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, text] = view.get<TransformComponent, TextRendererComponent>(entity);
				if (text.Font)
					Renderer2D::DrawString(text.Text, text.Font, transform.GetTransform(), text.Color);
			}
		}

		Renderer2D::EndScene();

		// 3D pass
		glm::mat4 viewProjection = mainCamera->GetProjectionMatrix() * glm::inverse(cameraTransform);
		glm::vec3 cameraWorldPos = glm::vec3(cameraTransform[3]);

		Renderer3D::BeginScene(viewProjection, cameraWorldPos);

		{
			auto view = registry.view<TransformComponent, SkinnedMeshComponent>();
			for (auto entity : view)
			{
				auto&& [transform, meshComp] = view.get<TransformComponent, SkinnedMeshComponent>(entity);
				if (!meshComp.Mesh) continue;

				auto shader = meshComp.Mesh->GetShader();
				if (shader)
				{
					shader->Bind();
					shader->SetMat4("u_ViewProjection", viewProjection);
					shader->SetMat4("u_Model", transform.GetTransform());
				}
				else
				{
					HMN_CORE_WARN("RenderSystem: Mesh has no shader");
				}

				meshComp.Mesh->Render();
			}
		}

		Renderer3D::EndScene();
	}

}
