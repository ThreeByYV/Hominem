#include "hmnpch.h"
#include "SandboxLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Scene/Components.h"
#include "MenuLayer.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

namespace Hominem {

	SandboxLayer::SandboxLayer()
		: Layer("Sandbox")
	{
		HMN_CORE_INFO("Created new SandboxLayer!");
	}

	void SandboxLayer::OnAttach()
	{
		m_ActiveScene = CreateRef<Scene>();

		// Create camera entity with proper ECS setup
		m_CameraEntity = m_ActiveScene->CreateEntity("Main Camera");
		auto& cameraComp = m_CameraEntity.AddComponent<CameraComponent>();

		// Set orthographic camera for 2D/2.5D gameplay
		auto& window = Application::Get().GetWindow();
		cameraComp.Camera.SetOrthographic(10.0f, -10.0f, 10.0f);
		cameraComp.Primary = true;

		// Set initial camera position
		auto& cameraTransform = m_CameraEntity.GetComponent<TransformComponent>();
		cameraTransform.Translation = glm::vec3(0.0f, 0.0f, 0.0f);

		// Setup camera controller to control this entity
		m_CameraController.SetEntity(m_CameraEntity);
		m_CameraController.SetSpeed(5.0f);

		// Initialize scene viewport
		m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

		Entity floor = m_ActiveScene->CreateEntity("Invisible Floor");

		auto& floorTransform = floor.GetComponent<TransformComponent>();
		floorTransform.Translation = { 0.0f, -5.0f, 0.0f };
		floorTransform.Scale = { 20.0f, 0.5f, 20.0f };

		auto& floorRb = floor.AddComponent<Rigidbody3DComponent>();
		floorRb.Type = Rigidbody3DComponent::BodyType::Static;

		auto& floorCollider = floor.AddComponent<BoxCollider3DComponent>();
		floorCollider.HalfExtents = { 1.0f, 1.0f, 1.0f };
		floorCollider.StaticFriction = 0.8f;
		floorCollider.DynamicFriction = 0.6f;

		auto& debugSprite = floor.AddComponent<SpriteRendererComponent>();
		debugSprite.Color = { 0.0f, 1.0f, 0.0f, 0.3f }; // Semi-transparent green

		// Load animated mesh
		//component should auto load mesh no manually LoadFromFile needed todo
		auto mesh = CreateRef<SkinnedMesh>();
		std::string meshPath = "src/Hominem/Resources/Textures/test.fbx";

		if (!mesh->LoadFromFile(meshPath))
		{
			HMN_CORE_ERROR("Failed to load mesh from {}", meshPath);
			return;
		}

		auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
		if (!skinningShader)
		{
			HMN_CORE_ERROR("Skinning shader not found!");
			return;
		}

		mesh->SetShader(skinningShader);
		mesh->InitBoneUniforms(skinningShader);

		// Create entity with mesh component
		m_MeshEntity = m_ActiveScene->CreateEntity("Animated Character");

		m_MeshPosition = glm::vec3(0.0f, 1.0f, 3.0f); // Start high so it falls
		m_MeshScale = glm::vec3(0.01f, 0.01f, 0.01f);
		m_MeshRotation = glm::vec3(0.0f, 0.0f, 0.0f);

		auto& meshTransform = m_MeshEntity.GetComponent<TransformComponent>();
		meshTransform.Translation = m_MeshPosition;
		meshTransform.Scale = m_MeshScale;
		meshTransform.Rotation = m_MeshRotation;

		m_MeshEntity.AddComponent<SkinnedMeshComponent>(mesh, meshPath);
		m_MeshEntity.AddComponent<AnimationComponent>(1.0f, true);

		// Add physics to the 3D model
		auto& meshRb = m_MeshEntity.AddComponent<Rigidbody3DComponent>();
		meshRb.Type = Rigidbody3DComponent::BodyType::Dynamic;
		meshRb.Mass = 10.0f; // Character mass
	

		auto& meshCollider = m_MeshEntity.AddComponent<BoxCollider3DComponent>();
		meshCollider.HalfExtents = { 0.5f, 0.5f, 0.3f }; // Character-shaped box (width, height, depth)
		meshCollider.StaticFriction = 0.5f;
		meshCollider.DynamicFriction = 0.5f;
		meshCollider.Offset = { 0.0f, 0.25f, 0.00 };

		// Start physics runtime AFTER creating all entities
		m_ActiveScene->OnRuntimeStart();

		HMN_CORE_INFO("SandboxLayer initialized - Red box will fall onto green floor!");
		HMN_CORE_INFO("Controls: WASD + Mouse, R to reload shaders, 1 for menu");
	}

	void SandboxLayer::OnDetach()
	{
		m_ActiveScene->OnRuntimeStop();
	}

	void SandboxLayer::OnUpdate(Timestep ts)
	{
		m_CameraController.OnUpdate(ts);

		if (Input::IsKeyPressed(HMN_KEY_R))
		{
			Renderer2D::GetShaderLibrary()->ReloadAll();
			Renderer3D::GetShaderLibrary()->ReloadAll();

			// Re-initialize bone uniforms after shader reload
			auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
			if (m_MeshEntity && m_MeshEntity.HasComponent<SkinnedMeshComponent>())
			{
				auto& meshComp = m_MeshEntity.GetComponent<SkinnedMeshComponent>();
				if (meshComp.Mesh)
				{
					meshComp.Mesh->SetShader(skinningShader);
					meshComp.Mesh->InitBoneUniforms(skinningShader);
				}
			}
		}

		if (Input::IsKeyPressed(HMN_KEY_1))
		{
			TransitionTo<MenuLayer>();
			return;
		}

		// Update mesh transform from ImGui controls
		if (m_MeshEntity && m_MeshEntity.HasComponent<TransformComponent>())
		{
			auto& meshTransform = m_MeshEntity.GetComponent<TransformComponent>();
			meshTransform.Translation = m_MeshPosition;
			meshTransform.Rotation = m_MeshRotation;
			meshTransform.Scale = m_MeshScale;
		}

		// Render
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		m_ActiveScene->OnUpdate(ts);
	}

	void SandboxLayer::OnImGuiRender()
	{
		ImGui::Begin("Settings");

		// Debug rendering controls
		if (ImGui::Checkbox("Use Basic Shader (no skinning)", &m_UseBasicShader))
		{
			// Switch shader when checkbox changes
			if (m_MeshEntity && m_MeshEntity.HasComponent<SkinnedMeshComponent>())
			{
				auto& meshComp = m_MeshEntity.GetComponent<SkinnedMeshComponent>();
				if (meshComp.Mesh)
				{
					if (m_UseBasicShader)
					{
						auto basicShader = Renderer3D::GetShaderLibrary()->Get("basic");
						if (basicShader)
							meshComp.Mesh->SetShader(basicShader);
					}
					else
					{
						auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
						if (skinningShader)
						{
							meshComp.Mesh->SetShader(skinningShader);
							meshComp.Mesh->InitBoneUniforms(skinningShader);
						}
					}
				}
			}
		}
		ImGui::Separator();

		// Mesh transform controls
		ImGui::Text("Mesh Transform");
		ImGui::DragFloat3("Position", &m_MeshPosition.x, 0.1f, -50.0f, 50.0f);
		ImGui::DragFloat3("Scale", &m_MeshScale.x, 0.001f, 0.001f, 10.0f);
		ImGui::DragFloat3("Rotation", &m_MeshRotation.x, 0.01f, -3.14159f, 3.14159f);

		if (ImGui::Button("Reset Transform"))
		{
			m_MeshPosition = glm::vec3(0.0f, 0.0f, 3.0f);
			m_MeshScale = glm::vec3(0.01f, 0.01f, 0.01f);
			m_MeshRotation = glm::vec3(0.0f, 0.0f, 0.0f);
		}

		ImGui::Separator();

		// Mesh debug info
		if (m_MeshEntity && m_MeshEntity.HasComponent<SkinnedMeshComponent>())
		{
			auto& meshComp = m_MeshEntity.GetComponent<SkinnedMeshComponent>();
			if (meshComp.Mesh)
			{
				ImGui::Text("Mesh Info:");
				ImGui::Text("  VAO: %u", meshComp.Mesh->GetVAO());
				ImGui::Text("  Vertices: %u", meshComp.Mesh->GetVertexCount());
				ImGui::Text("  Indices: %u", meshComp.Mesh->GetIndexCount());
				ImGui::Text("  Submeshes: %u", meshComp.Mesh->GetSubmeshCount());
				ImGui::Text("  Has Skeleton: %s", meshComp.Mesh->HasSkeleton() ? "Yes" : "No");
				ImGui::Text("  Bones: %d", meshComp.Mesh->GetBoneCount());
			}
			else
			{
				ImGui::Text("Mesh: NULL");
			}
		}

		ImGui::Separator();

		// Animation controls
		if (m_MeshEntity && m_MeshEntity.HasComponent<AnimationComponent>())
		{
			auto& animComp = m_MeshEntity.GetComponent<AnimationComponent>();
			ImGui::Checkbox("Play Animation", &animComp.Playing);
			ImGui::DragFloat("Animation Speed", &animComp.AnimationSpeed, 0.1f, 0.0f, 5.0f);
			ImGui::Text("Animation Time: %.2f", animComp.AnimationTime);
		}

		ImGui::Separator();

		// Camera settings
		ImGui::Text("Camera Settings");
		if (ImGui::DragFloat("Ortho Size", &m_OrthoSize, 0.5f, 1.0f, 50.0f))
		{
			if (m_CameraEntity && m_CameraEntity.HasComponent<CameraComponent>())
			{
				auto& cameraComp = m_CameraEntity.GetComponent<CameraComponent>();
				cameraComp.Camera.SetOrthographicSize(m_OrthoSize);
			}
		}
		ImGui::Checkbox("Cinematic Mode (Future)", &m_UseCinematicCamera);
		if (m_UseCinematicCamera)
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Cinematic camera not implemented yet");

		// Camera info
		ImGui::Separator();
		ImGui::Text("Camera Controls: WASD + Mouse");
		ImGui::Text("Press R to reload shaders");

		ImGui::End();
	}

	void SandboxLayer::OnEvent(Event& e)
	{
		m_CameraController.OnEvent(e);
	}

	long long SandboxLayer::GetCurrentTimeMillis()
	{
		const auto now = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	}

}
