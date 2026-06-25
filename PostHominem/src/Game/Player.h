#pragma once

#include "Hominem/Scene/Actors/SkinnedMeshActor.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Game/WorldConfig.h"

#include <glm/glm.hpp>

class Player : public Hominem::SkinnedMeshActor
{
public:
	/// Virtual asset path for the player mesh — published so LoadingLayer/GameLayer can preload it.
	static constexpr const char* k_MeshPath = "game://Textures/beige.glb";

	explicit Player(const PlayerConfig& config,
	                Hominem::Ref<Hominem::SkinnedMesh> preloadedMesh = nullptr);

	~Player() override = default;

	void OnCreate()  override;
	void OnUpdate(Hominem::Timestep ts) override;
	void OnDestroy() override;

	void OnImGuiRender();
	void ReloadShader();

	/// Hot-reloads movement/collider friction/scale; physics body and spawn need a level restart.
	void Reload(const PlayerConfig& cfg)
	{
		m_Config.Movement = cfg.Movement;
		m_Config.Collider.StaticFriction  = cfg.Collider.StaticFriction;
		m_Config.Collider.DynamicFriction = cfg.Collider.DynamicFriction;
		Scale = cfg.Scale;
	}

	Hominem::SkinnedMesh* GetMesh() const { return Mesh.get(); }
	glm::vec3 GetVelocity() const { return m_Body ? m_Body->GetLinearVelocity() : glm::vec3(0.f); }
	bool      IsMoving()    const { return m_Body && glm::abs(m_Body->GetLinearVelocity().x) > 0.01f; }

	void SetPosition(const glm::vec3& pos)
	{
		Position = pos;
		if (m_Body)
		{
			m_Body->SetPosition(pos);
			m_Body->SetLinearVelocity(glm::vec3(0.f));
		}
	}

private:
	PlayerConfig m_Config;

	Hominem::Ref<Hominem::Rigidbody>   m_Body;
	Hominem::Ref<Hominem::BoxCollider> m_Collider;
};
