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
	Hominem::Ref<Hominem::BoxCollider> m_Collider;

	enum class State { Idle = 0, Running };
	State m_State = State::Idle;
};
