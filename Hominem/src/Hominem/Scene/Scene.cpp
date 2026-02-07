#include "hmnpch.h"

#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "Hominem/Core/Application.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include <glad/glad.h>

namespace Hominem {

	Scene::Scene()
	{
		entt::entity entity = m_Registry.create();
		m_Registry.emplace<TransformComponent>(entity, glm::mat4(1.0f));
	}

	Scene::~Scene()
	{
		// Stop all active audio sources before scene destruction
		auto view = m_Registry.view<AudioSourceComponent>();
		for (auto entity : view)
		{
			auto& audioSource = view.get<AudioSourceComponent>(entity);
			if (audioSource.IsPlaying && audioSource.ActiveHandle != InvalidSound)
			{
				Application::Get().GetAudioSystem().Stop(audioSource.ActiveHandle);
			}
		}
	}

	void Scene::OnUpdate(Timestep ts)
	{
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;

		// Find primary camera
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = transform.Transform;
		
					break;
				}
			}
		}

		if (!mainCamera)
		{
			HMN_CORE_WARN("Scene::OnUpdate - No primary camera found!");
		}

		// Update animation system
		{
			auto view = m_Registry.view<SkinnedMeshComponent, AnimationComponent>();
			for (auto entity : view)
			{
				auto&& [meshComp, animComp] = view.get<SkinnedMeshComponent, AnimationComponent>(entity);

				if (!meshComp.Mesh)
					continue;

				// Update animation time
				if (animComp.Playing)
				{
					animComp.AnimationTime += ts * animComp.AnimationSpeed;
				}

				// Compute and upload bone transforms
				if (meshComp.Mesh->HasSkeleton())
				{
					std::vector<glm::mat4> boneTransforms;
					meshComp.Mesh->GetBoneTransforms(animComp.AnimationTime, boneTransforms);

					//Bind shader before uploading bone transforms
					auto shader = meshComp.Mesh->GetShader();
					if (shader)
					{
						shader->Bind();

						for (size_t i = 0; i < boneTransforms.size(); i++)
						{
							meshComp.Mesh->UploadBoneTransform(static_cast<uint32_t>(i), boneTransforms[i]);
						}
					}
				}
				else
				{
					static bool loggedNoSkeleton = false;
					if (!loggedNoSkeleton)
					{
						HMN_CORE_WARN("Mesh has no skeleton!");
						loggedNoSkeleton = true;
					}

					// Upload identity matrices for bones if no skeleton
					auto shader = meshComp.Mesh->GetShader();
					if (shader)
					{
						shader->Bind();
						glm::mat4 identity(1.0f);
						for (uint32_t i = 0; i < 100; i++) // MAX_BONES
						{
							meshComp.Mesh->UploadBoneTransform(i, identity);
						}
					}
				}
			}
		}

		// Update audio system
		{
			auto& audioSystem = Application::Get().GetAudioSystem();
			auto view = m_Registry.view<AudioSourceComponent>();

			for (auto entity : view)
			{
				auto& audioSource = view.get<AudioSourceComponent>(entity);

				// Skip if no buffer loaded
				if (audioSource.Buffer == InvalidSoundBuffer)
					continue;

				// Handle playback start
				if (audioSource.ShouldPlay && !audioSource.IsPlaying)
				{
					audioSource.ActiveHandle = audioSystem.PlayEx(
						audioSource.Buffer,
						audioSource.Volume,
						audioSource.Pitch,
						audioSource.Pan,
						audioSource.Loop
					);
					audioSource.IsPlaying = true;
					audioSource.PropertiesDirty = false;
				}
				// Handle playback stop
				else if (!audioSource.ShouldPlay && audioSource.IsPlaying)
				{
					audioSystem.Stop(audioSource.ActiveHandle);
					audioSource.IsPlaying = false;
					audioSource.ActiveHandle = InvalidSound;
				}
				// Handle property updates for active sounds
				else if (audioSource.IsPlaying && audioSource.PropertiesDirty)
				{
					audioSystem.SetVolume(audioSource.ActiveHandle, audioSource.Volume);
					audioSystem.SetPitch(audioSource.ActiveHandle, audioSource.Pitch);
					audioSystem.SetPan(audioSource.ActiveHandle, audioSource.Pan);
					audioSource.PropertiesDirty = false;
				}
			}
		}

		// Render scene
		if (mainCamera)
		{
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			// Render sprites
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

					if (sprite.Texture)
					{
						Renderer2D::DrawQuad(transform.Transform, sprite.Texture, sprite.Color);
					}
					else
					{
						Renderer2D::DrawQuad(transform.Transform, sprite.Color);
					}
				}
			}

			// Render text
			{
				auto view = m_Registry.view<TransformComponent, TextRendererComponent>();
				for (auto entity : view)
				{
					auto [transform, text] = view.get<TransformComponent, TextRendererComponent>(entity);

					if (text.Font)
					{
						Renderer2D::DrawString(text.Text, text.Font, transform.Transform, text.Color);
					}
				}
			}

			Renderer2D::EndScene();

			// Render 3D meshes
			{
				// Begin 3D scene with camera
				glm::mat4 viewProjection = mainCamera->GetProjectionMatrix() * glm::inverse(cameraTransform);
				glm::vec3 cameraWorldPos = glm::vec3(cameraTransform[3]);
				
				Renderer3D::BeginScene(viewProjection, cameraWorldPos);


				auto view = m_Registry.view<TransformComponent, SkinnedMeshComponent>();
				int meshCount = 0;
				static int frameLog = 0;
				for (auto entity : view)
				{
					meshCount++;
					auto&& [transform, meshComp] = view.get<TransformComponent, SkinnedMeshComponent>(entity);

					if (!meshComp.Mesh)
						continue;

					// Set up shader uniforms
					auto shader = meshComp.Mesh->GetShader();
					if (shader)
					{
						shader->Bind();

						shader->SetMat4("u_ViewProjection", viewProjection);
						shader->SetMat4("u_Model", transform.Transform);
					}
					else
					{
						HMN_CORE_WARN("Mesh has no shader!");
					}

					// Render the mesh
					meshComp.Mesh->Render();
				}

				Renderer3D::EndScene();
			}
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		HMN_CORE_INFO("Scene::OnViewportResize - {}x{}", width, height);
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize non FixedAspectRatio cameras
		auto view = m_Registry.view<CameraComponent>();
		int cameraCount = 0;
		for (auto entity : view)
		{
			cameraCount++;
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
			{
				HMN_CORE_INFO("Scene::OnViewportResize - Resizing camera {} to {}x{}", cameraCount, width, height);
				cameraComponent.Camera.SetViewportSize(width, height);
			}
			else
			{
				HMN_CORE_WARN("Scene::OnViewportResize - Camera {} has FixedAspectRatio=true, skipping", cameraCount);
			}
		}
		HMN_CORE_INFO("Scene::OnViewportResize - Total cameras processed: {}", cameraCount);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };

		// All entities will have a name and a transform
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();

		tag.Tag = name.empty() ? "Unknown Entity" : name;

		return entity;
	}
}