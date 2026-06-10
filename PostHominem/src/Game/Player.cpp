#include "hmnpch.h"
#include "Player.h"

#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/AsyncLoad.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/PhysicsTypes.h"
#include "Game/WorldConfig.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Hominem;

Player::Player(const PlayerConfig& config, Ref<SkinnedMesh> preloadedMesh)
	: m_Config(config), m_Mesh(std::move(preloadedMesh))
{
	Position = m_Config.Spawn.Position;
	Scale    = m_Config.Spawn.Scale;
	Rotation = glm::vec3(0.f, glm::radians(90.f), 0.f);
}

Ref<SkinnedMesh> Player::LoadMesh()
{
	auto mesh = SkinnedMesh::Create();

	const std::string meshPath = "Resources/Textures/beige.glb";
	if (auto res = mesh->LoadFromFile(meshPath); !res)
	{
		HMN_CORE_ERROR("{}", res.error());
		return nullptr;
	}

	return mesh;
}

namespace { AsyncLoad<SkinnedMesh> s_MeshPreload; }

void Player::BeginPreload()
{
	HMN_CORE_INFO("Player: preloading mesh on a background thread");
	s_MeshPreload.Begin([]() -> Ref<SkinnedMesh>
	{
		auto mesh = LoadMesh();
		if (mesh) HMN_CORE_INFO("Player: preloaded mesh ready");
		else      HMN_CORE_ERROR("Player: preload failed");
		return mesh;
	});
}

Ref<SkinnedMesh> Player::PreloadedMesh() { return s_MeshPreload.TryGet(); }

void Player::OnCreate()
{
	// Preloaded via BeginPreload in the common case; fall back to a synchronous
	// load (stalls the main thread for a few seconds) only if it wasn't ready.
	if (!m_Mesh)
	{
		HMN_CORE_WARN("Player: mesh not preloaded, loading synchronously on the main thread");
		m_Mesh = LoadMesh();
	}

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
	m_AnimTime += ts;
}

void Player::OnBuildRenderFrame(RenderFrame& frame)
{
	if (!m_Mesh) return;

	int boneCount = m_Mesh->GetBoneCount();
	uint32_t animCount = m_Mesh->GetAnimationCount();

	if (boneCount > 0 && animCount >= 2)
	{
		m_BoneTransforms.resize(boneCount);
		float speed = m_Body ? glm::abs(m_Body->GetLinearVelocity().x) : 0.f;
		float blend = glm::clamp(speed / m_Config.Movement.Speed, 0.f, 1.f);
		m_Mesh->GetBoneTransformsBlended(m_AnimTime, m_BoneTransforms, 1, 2, blend, true);

		auto bones = frame.AllocBones(boneCount);
		std::copy(m_BoneTransforms.begin(), m_BoneTransforms.end(), bones.begin());
		frame.meshes.push_back({ m_Mesh, GetTransform(), bones });
	}
	else if (boneCount > 0 && animCount == 1)
	{
		m_BoneTransforms.resize(boneCount);
		m_Mesh->GetBoneTransformsBlended(m_AnimTime, m_BoneTransforms, 1, 1, 1.f, true);

		auto bones = frame.AllocBones(boneCount);
		std::copy(m_BoneTransforms.begin(), m_BoneTransforms.end(), bones.begin());
		frame.meshes.push_back({ m_Mesh, GetTransform(), bones });
	}
	else
	{
		frame.meshes.push_back({ m_Mesh, GetTransform(), {} });
	}
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
