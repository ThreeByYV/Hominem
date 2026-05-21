#include "hmnpch.h"
#include "Player.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
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
	m_Mesh = CreateRef<StaticMesh>();

	const std::string meshPath = "Resources/Textures/beige.glb";
	if (!m_Mesh->LoadFromFile(meshPath))
	{
		HMN_CORE_ERROR("Player: Failed to load mesh from {}", meshPath);
		return;
	}
	HMN_CORE_INFO("Player: Loaded mesh from {}", meshPath);

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

}

void Player::OnBuildRenderFrame(RenderFrame& frame)
{
	if (!m_Mesh) return;
	frame.staticMeshes.push_back({ m_Mesh, GetTransform() });
}

void Player::OnDestroy() {}

void Player::ReloadShader() {}

void Player::OnImGuiRender()
{
	ImGui::Text("Player Transform");
	glm::vec3 pos = Position;
	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
		SetPosition(pos);
	ImGui::DragFloat3("Scale",    &Scale.x,    0.001f, 0.001f, 10.f);
	ImGui::DragFloat3("Rotation", &Rotation.x, 0.01f, -3.14159f, 3.14159f);

	if (ImGui::Button("Reset Transform"))
	{
		SetPosition(m_Config.Spawn.Position);
		Scale    = m_Config.Spawn.Scale;
		Rotation = glm::vec3(0.f, glm::radians(90.f), 0.f);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Spawn"))
	{
		m_Config.Spawn.Position = Position;
		m_Config.Scale = Scale;
		WorldConfig cfg;
		if (WorldConfig::LoadFromFile("Resources/Config/game_config.json", cfg))
		{
			cfg.Player.Spawn.Position = Position;
			cfg.Player.Scale          = Scale;
			WorldConfig::SaveToFile("Resources/Config/game_config.json", cfg);
		}
	}
}
