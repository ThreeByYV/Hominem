#include "hmnpch.h"
#include "Player.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/PhysicsTypes.h"
#include "Game/WorldConfig.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Hominem {

	Player::Player(const PlayerConfig& config)
		: m_Config(config)
	{
		// Position / scale come from config; OnCreate() runs after m_Scene is set.
		Position = m_Config.Spawn.Position;
		Scale    = m_Config.Spawn.Scale;
		Rotation = glm::vec3(0.f, glm::radians(90.f), 0.f);
	}

	void Player::OnCreate()
	{
		m_Mesh = CreateRef<SkinnedMesh>();
		const std::string idlePath    = "Resources/Textures/Idle.fbx";
		const std::string runningPath = "Resources/Textures/Running.fbx";

		if (!m_Mesh->LoadFromFile(idlePath))
		{
			HMN_CORE_ERROR("Player: Failed to load mesh from {}", idlePath);
			return;
		}
		HMN_CORE_INFO("Player: Loaded base mesh with {} animations", m_Mesh->GetAnimationCount());

		if (!m_Mesh->LoadAdditionalAnimation(runningPath))
			HMN_CORE_ERROR("Player: Failed to load running animation from {}", runningPath);

		HMN_CORE_INFO("Player: Total animations: {}", m_Mesh->GetAnimationCount());

		auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
		if (!skinningShader)
		{
			HMN_CORE_ERROR("Player: Skinning shader not found!");
			return;
		}
		m_Mesh->SetShader(skinningShader);

		if (m_Mesh->GetAnimationCount() >= 2)
		{
			HMN_CORE_INFO("Player: Blended animations: Idle(0) <-> Running(1)");
			m_UseBlending = true;
			m_StartAnim   = 0;
			m_EndAnim     = 1;
			m_TargetAnim  = 0;
			m_BlendFactor = 0.f;
		}
		else
		{
			HMN_CORE_WARN("Player: Only {} animation(s) — blending disabled.", m_Mesh->GetAnimationCount());
		}

		auto world = m_Scene ? m_Scene->GetPhysicsWorld() : nullptr;
		if (!world)
		{
			HMN_CORE_WARN("Player: No PhysicsWorld on scene — skipping physics setup");
			return;
		}

		RigidbodySpec rbSpec;
		rbSpec.Type          = RigidbodyType::Dynamic;
		rbSpec.Position      = Position;
		rbSpec.LockRotationZ = true;
		rbSpec.Mass          = m_Config.Movement.Mass;
		m_Body = world->CreateRigidbody(rbSpec);
		m_Body->SetGravityEnabled(true);

		auto material = world->CreateMaterial(
			m_Config.Collider.StaticFriction,
			m_Config.Collider.DynamicFriction,
			0.f);

		BoxColliderSpec colSpec;
		colSpec.HalfExtents = { m_Config.Collider.Extents.x, m_Config.Collider.Extents.y, 0.f };
		colSpec.Offset      = { m_Config.Collider.Offset.x,  m_Config.Collider.Offset.y,  0.f };

		m_Collider = world->CreateBoxCollider(colSpec, material);
		m_Body->AttachCollider(m_Collider);
	}

	void Player::OnUpdate(Timestep ts)
	{
		if (m_AnimPlaying)
			m_AnimTime += ts * m_AnimSpeed;

		if (m_UseBlending)
		{
			float target = (m_TargetAnim == m_EndAnim) ? 1.0f : 0.0f;
			float delta  = m_BlendSpeed * ts;
			if (glm::abs(m_BlendFactor - target) > 0.01f)
				m_BlendFactor = glm::clamp(m_BlendFactor + (m_BlendFactor < target ? delta : -delta), 0.f, 1.f);
			else
				m_BlendFactor = target;
		}

		if (m_Mesh && m_Mesh->HasSkeleton())
		{
			m_BoneCache.clear();
			if (m_UseBlending)
				m_Mesh->GetBoneTransformsBlended(m_AnimTime, m_BoneCache,
					m_StartAnim, m_EndAnim, m_BlendFactor, m_DisableRootMotion);
			else
				m_Mesh->GetBoneTransforms(m_AnimTime, m_BoneCache, m_DisableRootMotion);

			m_Mesh->DispatchSkinning(m_BoneCache);
		}

		// --- Physics input ---------------------------------------------------
		if (!m_Body) return;

		bool pressingA = Input::IsKeyPressed(HMN_KEY_A);
		bool pressingD = Input::IsKeyPressed(HMN_KEY_D);
		bool isMoving  = pressingA || pressingD;

		glm::vec3 velocity = m_Body->GetLinearVelocity();

		if (pressingA)
		{
			Rotation.y = glm::radians(-90.f);
			velocity.x = -m_Config.Movement.Speed;
		}
		else if (pressingD)
		{
			Rotation.y = glm::radians(90.f);
			velocity.x =  m_Config.Movement.Speed;
		}
		else
		{
			velocity.x = 0.f;
		}

		m_Body->SetLinearVelocity(velocity);

		// Sync position from physics body.
		Position = m_Body->GetPosition();

		// --- State machine ---------------------------------------------------
		if (isMoving && m_State == State::Idle)
		{
			m_State      = State::Running;
			m_TargetAnim = m_EndAnim;
		}
		else if (!isMoving && m_State == State::Running)
		{
			m_State      = State::Idle;
			m_TargetAnim = m_StartAnim;
		}
	}

	void Player::OnDraw3D()
	{
		if (!m_Mesh) return;

		// DrawSkinnedMesh sets u_ViewProjection, u_Model, and gCameraWorldPos
		// using the values stored by Renderer3D::BeginScene.
		Renderer3D::DrawSkinnedMesh(*m_Mesh, GetTransform());
	}

	void Player::OnDestroy()
	{
		// Physics resources are ref-counted; nothing manual needed.
	}

	void Player::ReloadShader()
	{
		if (!m_Mesh) return;
		auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
		if (skinningShader)
			m_Mesh->SetShader(skinningShader);
	}

	void Player::OnImGuiRender()
	{
		ImGui::Text("Player Transform");
		ImGui::DragFloat3("Position", &Position.x, 0.01f);
		ImGui::DragFloat3("Scale",    &Scale.x,    0.001f, 0.001f, 10.f);
		ImGui::DragFloat3("Rotation", &Rotation.x, 0.01f, -3.14159f, 3.14159f);

		if (ImGui::Button("Reset Transform"))
		{
			Scale    = m_Config.Spawn.Scale;
			Rotation = glm::vec3(0.f, glm::radians(90.f), 0.f);
		}

		ImGui::Separator();

		if (m_Mesh)
		{
			ImGui::Text("Mesh Info:");
			ImGui::Text("  VAO: %u",       m_Mesh->GetVAO());
			ImGui::Text("  Vertices: %u",  m_Mesh->GetVertexCount());
			ImGui::Text("  Indices: %u",   m_Mesh->GetIndexCount());
			ImGui::Text("  Submeshes: %u", m_Mesh->GetSubmeshCount());
			ImGui::Text("  Has Skeleton: %s", m_Mesh->HasSkeleton() ? "Yes" : "No");
			ImGui::Text("  Bones: %d",     m_Mesh->GetBoneCount());
		}

		ImGui::Separator();

		ImGui::Checkbox("Play Animation",   &m_AnimPlaying);
		ImGui::DragFloat("Animation Speed", &m_AnimSpeed, 0.1f, 0.f, 5.f);
		ImGui::Text("Animation Time: %.2f",  m_AnimTime);

		ImGui::Separator();
		ImGui::Text("Blended Animation System:");
		ImGui::Checkbox("Use Blending",    &m_UseBlending);
		ImGui::Text("State: %s",           m_State == State::Idle ? "Idle" : "Running");
		ImGui::Text("Start Anim: %d",      m_StartAnim);
		ImGui::Text("End Anim: %d",        m_EndAnim);
		ImGui::Text("Target Anim: %d",     m_TargetAnim);
		ImGui::SliderFloat("Blend Factor", &m_BlendFactor, 0.f, 1.f);
		ImGui::DragFloat("Blend Speed",    &m_BlendSpeed,  0.1f, 0.1f, 10.f);
	}

}
