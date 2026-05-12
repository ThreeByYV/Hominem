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

using namespace Hominem;

Player::Player(const PlayerConfig& config)
	: m_Config(config)
{
	Position = m_Config.Spawn.Position;
	Scale    = m_Config.Spawn.Scale;
	Rotation = glm::vec3(0.f, glm::radians(90.f), 0.f);
}

void Player::OnCreate()
{
	m_Mesh = SkinnedMesh::Create();

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

	// Build one blend sample per loaded animation, all starting at weight 0 except the first.
	uint32_t animCount = m_Mesh->GetAnimationCount();
	m_AnimSamples.resize(animCount);
	for (uint32_t i = 0; i < animCount; i++)
		m_AnimSamples[i] = { i, 0.f, (i == 0) ? 1.f : 0.f };

	HMN_CORE_INFO("Player: {} animation(s) registered for blending", animCount);

	auto world = m_Scene ? m_Scene->GetPhysicsWorld() : nullptr;
	if (!world)
	{
		HMN_CORE_WARN("Player: No PhysicsWorld on scene — skipping physics setup");
		return;
	}

	HMN_CORE_INFO("Player::OnCreate  position({:.2f},{:.2f},{:.2f})", Position.x, Position.y, Position.z);

	RigidbodySpec rbSpec;
	rbSpec.Type             = RigidbodyType::Dynamic;
	rbSpec.Position         = Position;
	rbSpec.LockRotationX    = true;
	rbSpec.LockRotationY    = true;
	rbSpec.LockRotationZ    = true;
	rbSpec.LockTranslationZ = true;
	rbSpec.Mass             = m_Config.Movement.Mass;
	m_Body = world->CreateRigidbody(rbSpec);
	m_Body->SetGravityEnabled(true);

	auto material = world->CreateMaterial(
		m_Config.Collider.StaticFriction,
		m_Config.Collider.DynamicFriction,
		0.f);

	BoxColliderSpec colSpec;
	colSpec.HalfExtents = { m_Config.Collider.Extents.x, m_Config.Collider.Extents.y, 10.f };
	colSpec.Offset      = { m_Config.Collider.Offset.x,  m_Config.Collider.Offset.y,  0.f };

	m_Collider = world->CreateBoxCollider(colSpec, material);
	m_Body->AttachCollider(m_Collider);
}

void Player::OnUpdate(Timestep ts)
{
	if (m_AnimPlaying)
		m_AnimTime += ts * m_AnimSpeed;

	// Blend weights toward the active animation and away from all others.
	if (!m_AnimSamples.empty())
	{
		float delta = m_BlendSpeed * ts;
		for (uint32_t i = 0; i < m_AnimSamples.size(); i++)
		{
			float target = (i == m_ActiveAnim) ? 1.f : 0.f;
			float& w = m_AnimSamples[i].weight;
			if (glm::abs(w - target) > 0.001f)
				w = glm::clamp(w + (w < target ? delta : -delta), 0.f, 1.f);
			else
				w = target;

			m_AnimSamples[i].time = m_AnimTime;
		}
	}

	if (m_Mesh && m_Mesh->HasSkeleton())
	{
		m_BoneCache.clear();
		m_Mesh->GetBoneTransformsBlendedN(m_AnimSamples, m_BoneCache, m_DisableRootMotion);
		// DispatchSkinning moved to RenderThread::ExecuteFrame.
	}

	if (!m_Body) return;

	m_DebugLogTimer += ts;
	if (m_DebugLogTimer < 3.f && static_cast<int>(m_DebugLogTimer / 0.5f) != m_DebugLogCount)
	{
		m_DebugLogCount = static_cast<int>(m_DebugLogTimer / 0.5f);
		HMN_CORE_INFO("Player pos ({:.2f},{:.2f},{:.2f})  vel ({:.2f},{:.2f},{:.2f})",
			Position.x, Position.y, Position.z,
			m_Body->GetLinearVelocity().x, m_Body->GetLinearVelocity().y, m_Body->GetLinearVelocity().z);
	}

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
	Position = m_Body->GetPosition();

	if (isMoving && m_State == State::Idle)
	{
		m_State       = State::Running;
		m_ActiveAnim  = 1; // Running animation
	}
	else if (!isMoving && m_State == State::Running)
	{
		m_State       = State::Idle;
		m_ActiveAnim  = 0; // Idle animation
	}
}

void Player::OnBuildRenderFrame(RenderFrame& frame)
{
	if (!m_Mesh) { HMN_CORE_WARN("Player: m_Mesh is null, not rendering"); return; }
	MeshDraw draw;
	draw.mesh      = m_Mesh;
	draw.transform = GetTransform();
	draw.bones     = m_BoneCache;
	frame.meshes.push_back(std::move(draw));
}

void Player::OnDestroy() {}

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
		ImGui::Text("Mesh: %u verts  %u idx  %u submesh  %d bones",
			m_Mesh->GetVertexCount(), m_Mesh->GetIndexCount(),
			m_Mesh->GetSubmeshCount(), m_Mesh->GetBoneCount());
	}

	ImGui::Separator();
	ImGui::Checkbox("Play Animation",   &m_AnimPlaying);
	ImGui::DragFloat("Animation Speed", &m_AnimSpeed, 0.1f, 0.f, 5.f);
	ImGui::DragFloat("Blend Speed",     &m_BlendSpeed, 0.1f, 0.1f, 10.f);
	ImGui::Text("Time: %.2f  State: %s  Active: %u",
		m_AnimTime, m_State == State::Idle ? "Idle" : "Running", m_ActiveAnim);

	ImGui::Text("Blend weights:");
	for (uint32_t i = 0; i < m_AnimSamples.size(); i++)
		ImGui::Text("  [%u] %.2f", i, m_AnimSamples[i].weight);
}
