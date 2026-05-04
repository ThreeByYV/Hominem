#include "hmnpch.h"
#include "Player.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Physics/Rigidbody.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

	Player::Player(Scene& scene, const PlayerConfig& config)
		: m_Scene(scene), m_Config(config)
	{
		// Load base mesh (Idle.fbx contains geometry + idle animation)
		auto mesh = CreateRef<SkinnedMesh>();
		std::string idlePath = "Resources/Textures/Idle.fbx";

		if (!mesh->LoadFromFile(idlePath))
		{
			HMN_CORE_ERROR("Player: Failed to load mesh from {}", idlePath);
			return;
		}

		HMN_CORE_INFO("Player: Loaded base mesh with {} animations", mesh->GetAnimationCount());

		// Load running animation
		std::string runningPath = "Resources/Textures/Running.fbx";
		if (!mesh->LoadAdditionalAnimation(runningPath))
		{
			HMN_CORE_ERROR("Player: Failed to load running animation from {}", runningPath);
		}

		uint32_t animCount = mesh->GetAnimationCount();
		HMN_CORE_INFO("Player: Total animations loaded: {}", animCount);

		auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
		if (!skinningShader)
		{
			HMN_CORE_ERROR("Player: Skinning shader not found!");
			return;
		}

		mesh->SetShader(skinningShader);

		// Create ECS entity
		m_Entity = m_Scene.CreateEntity("Animated Character");

		m_Scale    = config.Spawn.Scale;
		m_Rotation = glm::vec3(0.0f, glm::radians(90.0f), 0.0f); // Face right by default

		auto& transform = m_Entity.GetComponent<TransformComponent>();
		transform.Translation = config.Spawn.Position;
		transform.Scale       = m_Scale;
		transform.Rotation    = m_Rotation;

		m_Entity.AddComponent<SkinnedMeshComponent>(mesh, idlePath);

		// Animation component with blending
		auto& animComp = m_Entity.AddComponent<AnimationComponent>(1.0f, true);

		if (animCount >= 2)
		{
			HMN_CORE_INFO("Player: Setting up blended animations: Idle(0) <-> Running(1)");
			animComp.UseBlending    = true;
			animComp.StartAnimIndex = 0;   // Idle
			animComp.EndAnimIndex   = 1;   // Running
			animComp.TargetAnimIndex = 0;
			animComp.BlendFactor    = 0.0f;
			animComp.BlendSpeed     = 5.0f;
		}
		else
		{
			HMN_CORE_WARN("Player: Only {} animation(s) found — blending disabled.", animCount);
			animComp.UseBlending = false;
		}

		// Physics
		auto& rb    = m_Entity.AddComponent<RigidbodyComponent>();
		rb.Type     = RigidbodyComponent::BodyType::Dynamic;
		rb.Mass     = config.Movement.Mass;

		auto& collider              = m_Entity.AddComponent<BoxColliderComponent>();
		collider.HalfExtents        = config.Collider.Extents;
		collider.StaticFriction     = config.Collider.StaticFriction;
		collider.DynamicFriction    = config.Collider.DynamicFriction;
		collider.Offset             = config.Collider.Offset;
	}

	void Player::OnUpdate(Timestep ts)
	{
		if (!m_Entity || !m_Entity.HasComponent<AnimationComponent>())
			return;

		auto& animComp = m_Entity.GetComponent<AnimationComponent>();
		auto& rb       = m_Entity.GetComponent<RigidbodyComponent>();

		bool pressingA = Input::IsKeyPressed(HMN_KEY_A);
		bool pressingD = Input::IsKeyPressed(HMN_KEY_D);
		bool isMoving  = pressingA || pressingD;

		// Preserve Y velocity (gravity) while controlling X
		glm::vec3 velocity = glm::vec3(0.0f);
		if (rb.RuntimeBody)
			velocity = rb.RuntimeBody->GetLinearVelocity();

		if (pressingA)
		{
			m_Rotation.y = glm::radians(-90.0f);
			velocity.x   = -m_Config.Movement.Speed;
		}
		else if (pressingD)
		{
			m_Rotation.y = glm::radians(90.0f);
			velocity.x   = m_Config.Movement.Speed;
		}
		else
		{
			velocity.x = 0.0f;
		}

		if (rb.RuntimeBody)
		{
			rb.RuntimeBody->SetLinearVelocity(velocity);

			if (m_Entity.HasComponent<TransformComponent>())
			{
				auto& transform    = m_Entity.GetComponent<TransformComponent>();
				transform.Rotation = m_Rotation;
				transform.Scale    = m_Scale;
			}
		}

		// State machine: Idle <-> Running
		if (isMoving && m_State == State::Idle)
		{
			m_State = State::Running;
			animComp.TargetAnimIndex = 1;
		}
		else if (!isMoving && m_State == State::Running)
		{
			m_State = State::Idle;
			animComp.TargetAnimIndex = 0;
		}
	}

	void Player::OnImGuiRender()
	{
		if (!m_Entity)
			return;

		// Transform controls
		ImGui::Text("Player Transform");
		ImGui::DragFloat3("Scale",    &m_Scale.x,    0.001f, 0.001f, 10.0f);
		ImGui::DragFloat3("Rotation", &m_Rotation.x, 0.01f, -3.14159f, 3.14159f);

		if (ImGui::Button("Reset Transform"))
		{
			m_Scale    = m_Config.Spawn.Scale;
			m_Rotation = glm::vec3(0.0f, glm::radians(90.0f), 0.0f);
		}

		ImGui::Separator();

		// Mesh info
		if (m_Entity.HasComponent<SkinnedMeshComponent>())
		{
			auto& meshComp = m_Entity.GetComponent<SkinnedMeshComponent>();
			if (meshComp.Mesh)
			{
				ImGui::Text("Mesh Info:");
				ImGui::Text("  VAO: %u",       meshComp.Mesh->GetVAO());
				ImGui::Text("  Vertices: %u",  meshComp.Mesh->GetVertexCount());
				ImGui::Text("  Indices: %u",   meshComp.Mesh->GetIndexCount());
				ImGui::Text("  Submeshes: %u", meshComp.Mesh->GetSubmeshCount());
				ImGui::Text("  Has Skeleton: %s", meshComp.Mesh->HasSkeleton() ? "Yes" : "No");
				ImGui::Text("  Bones: %d",     meshComp.Mesh->GetBoneCount());
			}
		}

		ImGui::Separator();

		// Animation controls
		if (m_Entity.HasComponent<AnimationComponent>())
		{
			auto& animComp = m_Entity.GetComponent<AnimationComponent>();
			ImGui::Checkbox("Play Animation",    &animComp.Playing);
			ImGui::DragFloat("Animation Speed",  &animComp.AnimationSpeed, 0.1f, 0.0f, 5.0f);
			ImGui::Text("Animation Time: %.2f",   animComp.AnimationTime);

			ImGui::Separator();
			ImGui::Text("Blended Animation System:");
			ImGui::Checkbox("Use Blending", &animComp.UseBlending);
			ImGui::Text("Current State: %s", m_State == State::Idle ? "Idle" : "Running");
			ImGui::Text("Start Anim Index: %d",  animComp.StartAnimIndex);
			ImGui::Text("End Anim Index: %d",    animComp.EndAnimIndex);
			ImGui::Text("Target Anim Index: %d", animComp.TargetAnimIndex);
			ImGui::SliderFloat("Blend Factor",   &animComp.BlendFactor, 0.0f, 1.0f);
			ImGui::DragFloat("Blend Speed",      &animComp.BlendSpeed, 0.1f, 0.1f, 10.0f);
			ImGui::Text("Press A or D to switch to Running");
			ImGui::Text("Release keys to return to Idle");
		}
	}

	void Player::ReloadShader()
	{
		if (!m_Entity || !m_Entity.HasComponent<SkinnedMeshComponent>())
			return;

		auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
		if (!skinningShader) return;

		auto& meshComp = m_Entity.GetComponent<SkinnedMeshComponent>();
		if (meshComp.Mesh)
		{
			meshComp.Mesh->SetShader(skinningShader);
		}
	}

	glm::vec3 Player::GetPosition() const
	{
		if (m_Entity && m_Entity.HasComponent<TransformComponent>())
			return m_Entity.GetComponent<TransformComponent>().Translation;
		return glm::vec3(0.0f);
	}

}
