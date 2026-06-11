#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Core/Timestep.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Game/WorldConfig.h"


#include <glm/glm.hpp>

class Player : public Hominem::Actor
{
public:
	/// @param preloadedMesh  Optional fully-built skinned mesh (from PreloadedMesh()) to
	///   skip the multi-second synchronous Assimp import in OnCreate.
	explicit Player(const PlayerConfig& config,
	                Hominem::Ref<Hominem::SkinnedMesh> preloadedMesh = nullptr);

	~Player() override = default;

	/// Builds the player's skinned mesh with animations.
	static Hominem::Ref<Hominem::SkinnedMesh> LoadMesh();

	/// Starts LoadMesh() on a background thread at launch (call once from main).
	static void BeginPreload();

	/// Non-blocking; the preloaded mesh once ready, else nullptr (falls back to a sync load).
	static Hominem::Ref<Hominem::SkinnedMesh> PreloadedMesh();

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
		Scale = cfg.Scale;
	}

	Hominem::SkinnedMesh* GetMesh() const { return m_Mesh.get(); }
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

	Hominem::Ref<Hominem::SkinnedMesh> m_Mesh;

	Hominem::Ref<Hominem::Rigidbody>   m_Body;
	Hominem::Ref<Hominem::BoxCollider> m_Collider;

	float m_AnimTime      = 0.f;
	std::vector<glm::mat4> m_BoneTransforms;
};
