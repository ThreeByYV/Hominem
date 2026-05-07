#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include "Hominem/Physics/PhysicsTypes.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Game/WorldConfig.h"

#include <vector>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

/// Infinite tiling floor: keeps a sliding window of physics segments
/// centred on the camera. Old segments outside the window are destroyed
/// so physics bodies never accumulate unboundedly.
class InfiniteFloorActor : public Hominem::Actor
{
public:
	static constexpr int k_HalfWindow = 3;

	explicit InfiniteFloorActor(const FloorConfig& cfg)
		: m_Cfg(cfg)
		, m_SegW(cfg.Scale.x)
		, m_SegH(cfg.Scale.y)
		, m_FloorY(cfg.Position.y)
	{}

	void OnCreate() override
	{
		for (int i = -k_HalfWindow; i <= k_HalfWindow; i++)
			SpawnSegment(i * m_SegW);
	}

	void OnUpdate(Hominem::Timestep) override
	{
		if (!m_Scene) return;

		float camX = m_Scene->GetCameraPosition().x;

		while (!m_Segments.empty() && m_Segments.back().CenterX < camX + k_HalfWindow * m_SegW)
			SpawnSegment(m_Segments.back().CenterX + m_SegW);

		while (!m_Segments.empty() && m_Segments.front().CenterX > camX - k_HalfWindow * m_SegW)
			SpawnSegment(m_Segments.front().CenterX - m_SegW);

		float cullDist = (k_HalfWindow + 1) * m_SegW;
		for (auto& s : m_Segments)
		{
			if (std::abs(s.CenterX - camX) > cullDist && s.Body)
			{
				auto world = m_Scene->GetPhysicsWorld();
				if (world) world->DestroyRigidbody(s.Body);
				s.Body = nullptr;
			}
		}

		m_Segments.erase(
			std::ranges::remove_if(m_Segments, [](const Segment& s) { return s.Body == nullptr; }).begin(),
			m_Segments.end());
	}

	void OnBuildRenderFrame(Hominem::RenderFrame&) override {}

private:
	struct Segment
	{
		float                            CenterX;
		Hominem::Ref<Hominem::Rigidbody> Body;
	};

	FloorConfig          m_Cfg;
	float                m_SegW, m_SegH, m_FloorY;
	std::vector<Segment> m_Segments;

	void SpawnSegment(float centerX)
	{
		auto world = m_Scene ? m_Scene->GetPhysicsWorld() : nullptr;
		if (!world) return;

		Hominem::RigidbodySpec spec;
		spec.Type     = Hominem::RigidbodyType::Static;
		spec.Position = glm::vec3(centerX, m_FloorY, 0.f);
		auto body = world->CreateRigidbody(spec);
		body->SetGravityEnabled(false);

		auto mat = world->CreateMaterial(m_Cfg.StaticFriction, m_Cfg.DynamicFriction, 0.f);
		Hominem::BoxColliderSpec col;
		col.HalfExtents = { m_SegW * 0.5f, m_SegH * 0.5f, 0.f };
		body->AttachCollider(world->CreateBoxCollider(col, mat));

		Segment seg{ centerX, body };
		const auto it = std::ranges::lower_bound(m_Segments, seg,
			[](const Segment& a, const Segment& b) { return a.CenterX < b.CenterX; });
		m_Segments.insert(it, std::move(seg));
	}
};
