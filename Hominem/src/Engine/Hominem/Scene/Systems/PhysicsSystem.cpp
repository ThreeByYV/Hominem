#include "hmnpch.h"
#include "PhysicsSystem.h"
#include "Hominem/Scene/Components.h"
#include "Hominem/Physics/Rigidbody.h"
#include "Hominem/Physics/Collider.h"
#include "Hominem/Physics/PhysicsTypes.h"

namespace Hominem {

	void PhysicsSystem::OnRuntimeStart(entt::registry& registry)
	{
		m_World = CreateRef<PhysicsWorld>(glm::vec3(0.0f, -9.8f, 0.0f));

		auto view = registry.view<TransformComponent, RigidbodyComponent>();
		for (auto e : view)
		{
			auto& transform = view.get<TransformComponent>(e);
			auto& rb        = view.get<RigidbodyComponent>(e);

			RigidbodySpec spec;
			spec.Type            = (RigidbodyType)rb.Type;
			spec.Position        = transform.Translation;
			spec.LockRotationZ   = rb.FixedRotation;
			spec.Mass            = rb.Mass;
			spec.LinearDamping   = rb.LinearDamping;
			spec.AngularDamping  = rb.AngularDamping;

			rb.RuntimeBody = m_World->CreateRigidbody(spec);
			rb.RuntimeBody->SetGravityEnabled(rb.GravityEnabled);

			if (registry.any_of<BoxColliderComponent>(e))
			{
				auto& col = registry.get<BoxColliderComponent>(e);

				auto material = m_World->CreateMaterial(col.StaticFriction, col.DynamicFriction, col.Restitution);

				BoxColliderSpec colSpec;
				colSpec.HalfExtents = { col.HalfExtents.x * transform.Scale.x, col.HalfExtents.y * transform.Scale.y, 0.0f };
				colSpec.Offset      = { col.Offset.x, col.Offset.y, 0.0f };
				colSpec.IsTrigger   = col.IsTrigger;

				col.RuntimeCollider = m_World->CreateBoxCollider(colSpec, material);
				rb.RuntimeBody->AttachCollider(col.RuntimeCollider);
			}
		}

		size_t count = 0;
		for (auto e : view) count++;
		HMN_CORE_INFO("PhysicsSystem: runtime started — {} rigidbodies", count);
	}

	void PhysicsSystem::OnRuntimeStop(entt::registry& registry)
	{
		auto view = registry.view<RigidbodyComponent>();
		for (auto e : view)
		{
			auto& rb = view.get<RigidbodyComponent>(e);
			if (rb.RuntimeBody)
			{
				m_World->DestroyRigidbody(rb.RuntimeBody);
				rb.RuntimeBody = nullptr;
			}
		}

		m_World = nullptr;
		HMN_CORE_INFO("PhysicsSystem: runtime stopped");
	}

	void PhysicsSystem::OnUpdate(Timestep ts, entt::registry& registry)
	{
		if (!m_World) return;

		m_World->Step(ts);

		auto view = registry.view<TransformComponent, RigidbodyComponent>();
		for (auto e : view)
		{
			auto& transform = view.get<TransformComponent>(e);
			auto& rb        = view.get<RigidbodyComponent>(e);

			if (rb.RuntimeBody && rb.Type == RigidbodyComponent::BodyType::Dynamic)
				transform.Translation = rb.RuntimeBody->GetPosition();
		}
	}

}
