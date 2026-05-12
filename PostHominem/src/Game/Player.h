#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Game/WorldConfig.h"

#include <glm/glm.hpp>
#include <vector>

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
	bool      IsMoving()    const { return m_State == State::Running; }

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

	Hominem::Ref<Hominem::SkinnedMesh>         m_Mesh;
	float                                      m_AnimTime          = 0.f;
	float                                      m_AnimSpeed         = 1.f;
	bool                                       m_AnimPlaying       = true;
	bool                                       m_DisableRootMotion = true;
	std::vector<glm::mat4>                     m_BoneCache;

	// Each entry corresponds to a loaded animation (index matches load order).
	// Weights are managed per-frame and normalised inside GetBoneTransformsBlendedN.
	std::vector<Hominem::AnimBlendSample>      m_AnimSamples;
	float                                      m_BlendSpeed        = 5.f;
	uint32_t                                   m_ActiveAnim        = 0; // target animation index

	Hominem::Ref<Hominem::Rigidbody>   m_Body;

	float m_DebugLogTimer = 0.f;
	int   m_DebugLogCount = -1;
	Hominem::Ref<Hominem::BoxCollider> m_Collider;

	enum class State { Idle = 0, Running };
	State m_State = State::Idle;
};
