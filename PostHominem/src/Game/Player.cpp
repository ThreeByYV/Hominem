#include "hmnpch.h"
#include "Player.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/PhysicsHelpers.h"
#include "Game/WorldConfig.h"

#include <imgui.h>
#include "Hominem/ImGui/UI.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace Hominem;

Player::Player(const PlayerConfig& config, Ref<SkinnedMesh> preloadedMesh)
	: m_Config(config)
{
	Mesh     = std::move(preloadedMesh);
	Position = m_Config.Spawn.Position;
	Scale    = m_Config.Spawn.Scale;
	Rotation = glm::vec3(0.f, glm::radians(90.f), 0.f);
}

void Player::OnCreate()
{
	if (!Mesh)
	{
		HMN_CORE_WARN("Player: mesh not preloaded, loading synchronously");
		if (auto r = AssetManager::Load<SkinnedMesh>("game://Textures/beige.glb"))
			Mesh = r->Get();
	}

	auto world = m_Scene ? m_Scene->GetPhysicsWorld() : nullptr;
	if (!world)
	{
		HMN_CORE_WARN("Player: No PhysicsWorld on scene - skipping physics setup");
		return;
	}

	HMN_CORE_INFO("Player::OnCreate  position({:.2f},{:.2f},{:.2f})", Position.x, Position.y, Position.z);

	m_Body = Physics::CreateDynamicBody2D(world.get(), Position, m_Config.Movement.Mass);
	m_Collider = Physics::AttachBoxCollider(world.get(), m_Body,
		{ m_Config.Collider.Extents.x, m_Config.Collider.Extents.y, 10.f },
		{ m_Config.Collider.Offset.x,  m_Config.Collider.Offset.y,  0.f },
		m_Config.Collider.StaticFriction, m_Config.Collider.DynamicFriction);
}

void Player::OnUpdate(Timestep ts)
{
	SkinnedMeshActor::OnUpdate(ts);

	if (!m_Body) return;

	bool pressingA = Input::IsKeyPressed(HMN_KEY_A);
	bool pressingD = Input::IsKeyPressed(HMN_KEY_D);

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
	BlendFactor = glm::clamp(glm::abs(velocity.x) / m_Config.Movement.Speed, 0.f, 1.f);
}

void Player::OnDestroy() {}

void Player::ReloadShader() {}

void Player::OnImGuiRender()
{
	using namespace Hominem;
	ImGui::TextUnformatted("Player Transform");
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
