#include "hmnpch.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/common.hpp>

#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "Hominem/Core/Application.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Hominem/Physics/PhysicsTypes.h"

namespace Hominem {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
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
					cameraTransform = transform.GetTransform();
					break;
				}
			}
		}

		if (!mainCamera)
		{
			HMN_CORE_WARN("Scene::OnUpdate - No primary camera found!");
		}

		// Animation system
		{
			auto view = m_Registry.view<SkinnedMeshComponent, AnimationComponent>();
			for (auto entity : view)
			{
				auto&& [meshComp, animComp] = view.get<SkinnedMeshComponent, AnimationComponent>(entity);

				if (!meshComp.Mesh)
					continue;

				if (animComp.Playing)
				{
					animComp.AnimationTime += ts * animComp.AnimationSpeed;
				}

				// Update blend factor if blending is active
				if (animComp.UseBlending)
				{
					// BlendFactor: 0.0 = fully StartAnim, 1.0 = fully EndAnim
					// Determine which direction to blend based on target
					float targetBlend = (animComp.TargetAnimIndex == animComp.EndAnimIndex) ? 1.0f : 0.0f;
					float blendDelta = animComp.BlendSpeed * ts;

					// Smoothly interpolate blend factor towards target
					if (glm::abs(animComp.BlendFactor - targetBlend) > 0.01f)
					{
						if (animComp.BlendFactor < targetBlend)
						{
							animComp.BlendFactor = glm::min(animComp.BlendFactor + blendDelta, 1.0f);
						}
						else if (animComp.BlendFactor > targetBlend)
						{
							animComp.BlendFactor = glm::max(animComp.BlendFactor - blendDelta, 0.0f);
						}
					}
					else
					{
						// Close enough - snap to target
						animComp.BlendFactor = targetBlend;
					}
				}

				if (meshComp.Mesh->HasSkeleton())
				{
					std::vector<glm::mat4> boneTransforms;

					if (animComp.UseBlending)
					{
						meshComp.Mesh->GetBoneTransformsBlended(
							animComp.AnimationTime,
							boneTransforms,
							animComp.StartAnimIndex,
							animComp.EndAnimIndex,
							animComp.BlendFactor,
							animComp.DisableRootMotion
						);
					}
					else
					{
						meshComp.Mesh->GetBoneTransforms(animComp.AnimationTime, boneTransforms, animComp.DisableRootMotion);
					}

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

					// Upload identity matrices for meshes without skeletons
					auto shader = meshComp.Mesh->GetShader();
					if (shader)
					{
						shader->Bind();
						glm::mat4 identity(1.0f);
						for (uint32_t i = 0; i < 100; i++)
						{
							meshComp.Mesh->UploadBoneTransform(i, identity);
						}
					}
				}
			}
		}

		// Audio system
		{
			auto& audioSystem = Application::Get().GetAudioSystem();
			auto view = m_Registry.view<AudioSourceComponent>();

			for (auto entity : view)
			{
				auto& audioSource = view.get<AudioSourceComponent>(entity);

				if (audioSource.Buffer == InvalidSoundBuffer)
					continue;

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
				else if (!audioSource.ShouldPlay && audioSource.IsPlaying)
				{
					audioSystem.Stop(audioSource.ActiveHandle);
					audioSource.IsPlaying = false;
					audioSource.ActiveHandle = InvalidSound;
				}
				else if (audioSource.IsPlaying && audioSource.PropertiesDirty)
				{
					audioSystem.SetVolume(audioSource.ActiveHandle, audioSource.Volume);
					audioSystem.SetPitch(audioSource.ActiveHandle, audioSource.Pitch);
					audioSystem.SetPan(audioSource.ActiveHandle, audioSource.Pan);
					audioSource.PropertiesDirty = false;
				}
			}
		}

		// Physics simulation and syncing any dynamic transform only
		if (m_PhysicsWorld)
		{
			m_PhysicsWorld->Step(ts);

			auto view = m_Registry.view<TransformComponent, RigidbodyComponent>();
			for (auto e : view)
			{
				auto [transform, rb] = view.get<TransformComponent, RigidbodyComponent>(e);

				if (rb.RuntimeBody && rb.Type == RigidbodyComponent::BodyType::Dynamic)
				{
					transform.Translation = rb.RuntimeBody->GetPosition();
				}
			}
		}

		// Rendering
		if (mainCamera)
		{
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

					if (sprite.Texture)
					{
						Renderer2D::DrawQuad(transform.GetTransform(), sprite.Texture, sprite.Color);
					}
					else
					{
						Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
					}
				}
			}

			{
				auto view = m_Registry.view<TransformComponent, TextRendererComponent>();
				for (auto entity : view)
				{
					auto [transform, text] = view.get<TransformComponent, TextRendererComponent>(entity);

					if (text.Font)
					{
						Renderer2D::DrawString(text.Text, text.Font, transform.GetTransform(), text.Color);
					}
				}
			}

			Renderer2D::EndScene();

			// 3D rendering
			{
				glm::mat4 viewProjection = mainCamera->GetProjectionMatrix() * glm::inverse(cameraTransform);
				glm::vec3 cameraWorldPos = glm::vec3(cameraTransform[3]);

				Renderer3D::BeginScene(viewProjection, cameraWorldPos);

				auto view = m_Registry.view<TransformComponent, SkinnedMeshComponent>();
				for (auto entity : view)
				{
					auto&& [transform, meshComp] = view.get<TransformComponent, SkinnedMeshComponent>(entity);

					if (!meshComp.Mesh)
						continue;

					auto shader = meshComp.Mesh->GetShader();
					if (shader)
					{
						shader->Bind();

						shader->SetMat4("u_ViewProjection", viewProjection);
						shader->SetMat4("u_Model", transform.GetTransform());
					}
					else
					{
						HMN_CORE_WARN("Mesh has no shader!");
					}

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

		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();

		tag.Tag = name.empty() ? "Unknown Entity" : name;

		return entity;
	}

	void Scene::OnRuntimeStart()
	{
		m_PhysicsWorld = CreateRef<PhysicsWorld>(glm::vec3(0.0f, -9.8f, 0.0f));  // Box2D

		auto view = m_Registry.view<TransformComponent, RigidbodyComponent>();

		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb = entity.GetComponent<RigidbodyComponent>();

			RigidbodySpec spec;
			spec.Type             = (RigidbodyType)rb.Type;
			spec.Position         = transform.Translation;
			spec.LockRotationZ    = rb.FixedRotation;
			spec.Mass             = rb.Mass;
			spec.LinearDamping    = rb.LinearDamping;
			spec.AngularDamping   = rb.AngularDamping;

			rb.RuntimeBody = m_PhysicsWorld->CreateRigidbody(spec);

			rb.RuntimeBody->SetGravityEnabled(rb.GravityEnabled);

			if (entity.HasComponent<BoxColliderComponent>())
			{
				auto& collider = entity.GetComponent<BoxColliderComponent>();

				auto material = m_PhysicsWorld->CreateMaterial(
					collider.StaticFriction,
					collider.DynamicFriction,
					collider.Restitution
				);

				BoxColliderSpec colliderSpec;
				colliderSpec.HalfExtents = { collider.HalfExtents.x * transform.Scale.x,
				                             collider.HalfExtents.y * transform.Scale.y,
				                             0.0f };
				colliderSpec.Offset      = { collider.Offset.x, collider.Offset.y, 0.0f };
				colliderSpec.IsTrigger   = collider.IsTrigger;

				collider.RuntimeCollider = m_PhysicsWorld->CreateBoxCollider(colliderSpec, material);
				rb.RuntimeBody->AttachCollider(collider.RuntimeCollider);
			}
		}

		size_t rbCount = 0;
		for (auto e : view) { rbCount++; }
		HMN_CORE_INFO("Physics runtime started - {} rigidbodies created", rbCount);
	}

	void Scene::OnRuntimeStop()
 	{
		auto view = m_Registry.view<RigidbodyComponent>();
		for (auto e : view)
		{
			auto& rb = view.get<RigidbodyComponent>(e);
			if (rb.RuntimeBody)
			{
				m_PhysicsWorld->DestroyRigidbody(rb.RuntimeBody);
				rb.RuntimeBody = nullptr;
			}
		}

		m_PhysicsWorld = nullptr;

		HMN_CORE_INFO("Physics runtime stopped");
	}
}
