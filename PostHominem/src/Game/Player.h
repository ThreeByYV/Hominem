#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Game/WorldConfig.h"


#include <glm/glm.hpp>

class Player : public Hominem::Actor
{
public:
	explicit Player(const PlayerConfig& config);
	~Player() override = default;

	void OnCreate()  override;
	void OnUpdate(Hominem::Timestep ts) override;
	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override;
	void OnDestroy() override;

	void OnImGuiRender();
	void ReloadShader();

	// Re-applies all hot-reloadable config values without restarting the level.
	// Physics body (mass, collider) and spawn position still need a level restart.
	void Reload(const PlayerConfig& cfg)
	{
		m_Config.Movement = cfg.Movement;
		m_Config.Collider.StaticFriction  = cfg.Collider.StaticFriction;
		m_Config.Collider.DynamicFriction = cfg.Collider.DynamicFriction;
		Scale = glm::vec3(cfg.Scale);
	}

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

	Hominem::Ref<Hominem::StaticMesh>  m_Mesh;

	Hominem::Ref<Hominem::Rigidbody>   m_Body;
	Hominem::Ref<Hominem::BoxCollider> m_Collider;

	float m_DebugLogTimer = 0.f;
	int   m_DebugLogCount = -1;
};
