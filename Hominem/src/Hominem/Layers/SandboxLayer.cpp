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
		// Size = 10 means camera shows 10 world units vertically
		// For 1920x1080, this shows ~17.78 units horizontally (10 * 16/9)
		auto& window = Application::Get().GetWindow();
		cameraComp.Camera.SetOrthographic(10.0f, -10.0f, 10.0f);
		cameraComp.Primary = true;

		// Set initial camera position
		auto& cameraTransform = m_CameraEntity.GetComponent<TransformComponent>();
		cameraTransform.Transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

		// Setup camera controller to control this entity
		m_CameraController.SetEntity(m_CameraEntity);
		m_CameraController.SetSpeed(5.0f);

		// Initialize scene viewport
		m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());

		// Load animated mesh
		auto mesh = CreateRef<SkinnedMesh>();
		std::string meshPath = "src/Hominem/Resources/Textures/test.fbx";

		if (!mesh->LoadFromFile(meshPath))
		{
			HMN_CORE_ERROR("Failed to load mesh from {}", meshPath);
			return;
		}

		// Use skinning shader for proper skeletal animation
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

		// Position the mesh in front of the camera
		m_MeshPosition = glm::vec3(0.0f, 0.0f, 3.0f);
		m_MeshScale = glm::vec3(0.01f, 0.01f, 0.01f);
		m_MeshRotation = glm::vec3(0.0f, 0.0f, 0.0f);

		auto& meshTransform = m_MeshEntity.GetComponent<TransformComponent>();
		meshTransform.Transform = glm::translate(glm::mat4(1.0f), m_MeshPosition)
			* glm::scale(glm::mat4(1.0f), m_MeshScale);

		m_MeshEntity.AddComponent<SkinnedMeshComponent>(mesh, meshPath);
		m_MeshEntity.AddComponent<AnimationComponent>(1.0f, true); // Play animation at 1x speed

		HMN_CORE_INFO("SandboxLayer initialized - Controls: WASD + Mouse, R to reload shaders, 1 for menu");
	}

	void SandboxLayer::OnDetach()
	{
	}

	void SandboxLayer::OnUpdate(Timestep ts)
	{
		// Update camera controller (modifies camera entity's transform directly)
		m_CameraController.OnUpdate(ts);

		// Handle input
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
			glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), m_MeshRotation.y, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::rotate(glm::mat4(1.0f), m_MeshRotation.x, glm::vec3(1.0f, 0.0f, 0.0f))
				* glm::rotate(glm::mat4(1.0f), m_MeshRotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
			meshTransform.Transform = glm::translate(glm::mat4(1.0f), m_MeshPosition)
				* rotation
				* glm::scale(glm::mat4(1.0f), m_MeshScale);
		}

		// Render
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		// Update Scene (handles all rendering via systems)
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
