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
		floorTransform.Translation = { 0.0f, -6.0f, 0.0f };
		floorTransform.Scale = { 20.0f, 0.5f, 20.0f };

		auto& floorRb = floor.AddComponent<Rigidbody3DComponent>();
		floorRb.Type = Rigidbody3DComponent::BodyType::Static;

		auto& floorCollider = floor.AddComponent<BoxCollider3DComponent>();
		floorCollider.HalfExtents = { 1.0f, 1.0f, 1.0f };
		floorCollider.StaticFriction = 0.8f;
		floorCollider.DynamicFriction = 0.6f;

		auto& debugSprite = floor.AddComponent<SpriteRendererComponent>();
		debugSprite.Color = { 0.0f, 1.0f, 0.0f, 0.3f }; // Semi-transparent green

		// Load animated mesh from Idle.fbx (contains geometry + idle animation)
		auto mesh = CreateRef<SkinnedMesh>();
		std::string idlePath = "src/Hominem/Resources/Textures/Idle.fbx";

		if (!mesh->LoadFromFile(idlePath))
		{
			HMN_CORE_ERROR("Failed to load mesh from {}", idlePath);
			return;
		}

		HMN_CORE_INFO("Loaded base mesh with {} animations", mesh->GetAnimationCount());

		// Load the running animation from Running.fbx
		std::string runningPath = "src/Hominem/Resources/Textures/Running.fbx";
		if (!mesh->LoadAdditionalAnimation(runningPath))
		{
			HMN_CORE_ERROR("Failed to load running animation from {}", runningPath);
			// Continue anyway - we'll just use idle animation only
		}

		uint32_t animCount = mesh->GetAnimationCount();
		HMN_CORE_INFO("Total animations loaded: {}", animCount);

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
		// Rotate 90 degrees to face right by default (so idle faces sideways, not at camera)
		m_MeshRotation = glm::vec3(0.0f, glm::radians(90.0f), 0.0f);

		auto& meshTransform = m_MeshEntity.GetComponent<TransformComponent>();
		meshTransform.Translation = m_MeshPosition;
		meshTransform.Scale = m_MeshScale;
		meshTransform.Rotation = m_MeshRotation;

		m_MeshEntity.AddComponent<SkinnedMeshComponent>(mesh, idlePath);

		// Set up animation component with blending support
		auto& animComp = m_MeshEntity.AddComponent<AnimationComponent>(1.0f, true);

		// Only enable blending if we have at least 2 animations
		if (animCount >= 2)
		{
			HMN_CORE_INFO("Setting up blended animations: Idle(0) <-> Running(1)");
			animComp.UseBlending = true;
			animComp.StartAnimIndex = 0;  // Idle animation
			animComp.EndAnimIndex = 1;    // Running animation
			animComp.TargetAnimIndex = 0; // Start with idle
			animComp.BlendFactor = 0.0f;  // Fully idle
			animComp.BlendSpeed = 5.0f;   // Smooth transition
		}
		else
		{
			HMN_CORE_WARN("Only {} animation(s) found - blending disabled. Load a mesh with both Idle and Running animations!", animCount);
			animComp.UseBlending = false;
		}

		// Add physics to the 3D model
		auto& meshRb = m_MeshEntity.AddComponent<Rigidbody3DComponent>();
		meshRb.Type = Rigidbody3DComponent::BodyType::Dynamic;
		meshRb.Mass = 10.0f; // Character mass
	

		auto& meshCollider = m_MeshEntity.AddComponent<BoxCollider3DComponent>();
		meshCollider.HalfExtents = { 0.5f, 0.5f, 0.3f }; // Character-shaped box (width, height, depth)
		meshCollider.StaticFriction = 0.5f;
		meshCollider.DynamicFriction = 0.5f;
		meshCollider.Offset = { 0.0f, -0.75f, 0.00 };

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

		// Handle animation state transitions and character rotation based on input
		if (m_MeshEntity && m_MeshEntity.HasComponent<AnimationComponent>())
		{
			auto& animComp = m_MeshEntity.GetComponent<AnimationComponent>();

			// Check individual movement keys
			bool pressingA = Input::IsKeyPressed(HMN_KEY_A);
			bool pressingD = Input::IsKeyPressed(HMN_KEY_D);
			bool isMoving = pressingA || pressingD;

			// Handle character rotation and movement (face left or right)
			float moveSpeed = 5.0f; // Units per second
			if (pressingA)
			{
				// Face left (-90 degrees around Y axis)
				m_MeshRotation.y = glm::radians(-90.0f);
				// Move left
				m_MeshPosition.x -= moveSpeed * ts;
			}
			else if (pressingD)
			{
				// Face right (90 degrees around Y axis - flipped)
				m_MeshRotation.y = glm::radians(90.0f);
				// Move right
				m_MeshPosition.x += moveSpeed * ts;
			}

			// Handle animation state transitions
			// Keep Start=Idle(0), End=Running(1) always
			// BlendFactor: 0.0 = Idle, 1.0 = Running
			if (isMoving && m_CurrentState == CharacterState::Idle)
			{
				// Transition to running - blend towards 1.0
				m_CurrentState = CharacterState::Running;
				animComp.TargetAnimIndex = 1; // Running

				HMN_CORE_INFO("Transitioning to Running (Blend={} -> 1.0)", animComp.BlendFactor);
			}
			else if (!isMoving && m_CurrentState == CharacterState::Running)
			{
				// Transition back to idle - blend towards 0.0
				m_CurrentState = CharacterState::Idle;
				animComp.TargetAnimIndex = 0; // Idle

				HMN_CORE_INFO("Transitioning to Idle (Blend={} -> 0.0)", animComp.BlendFactor);
			}
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

			ImGui::Separator();
			ImGui::Text("Blended Animation System:");
			ImGui::Checkbox("Use Blending", &animComp.UseBlending);
			ImGui::Text("Current State: %s", m_CurrentState == CharacterState::Idle ? "Idle" : "Running");
			ImGui::Text("Start Anim Index: %d", animComp.StartAnimIndex);
			ImGui::Text("End Anim Index: %d", animComp.EndAnimIndex);
			ImGui::Text("Target Anim Index: %d", animComp.TargetAnimIndex);
			ImGui::SliderFloat("Blend Factor", &animComp.BlendFactor, 0.0f, 1.0f);
			ImGui::DragFloat("Blend Speed", &animComp.BlendSpeed, 0.1f, 0.1f, 10.0f);
			ImGui::Text("Press A or D to switch to Running");
			ImGui::Text("Release keys to return to Idle");
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
