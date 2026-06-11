#include "hmnpch.h"
#include "PhysicsWorld.h"

namespace Hominem {

	PhysicsWorld::PhysicsWorld(const glm::vec3& gravity)
	{
		b2WorldDef worldDef  = b2DefaultWorldDef();
		worldDef.gravity     = { gravity.x, gravity.y };
		m_WorldId            = b2CreateWorld(&worldDef);
		HMN_CORE_INFO("PhysicsWorld: Box2D world created (gravity={0}, {1})", gravity.x, gravity.y);
	}

	PhysicsWorld::~PhysicsWorld()
	{
		if (B2_IS_NON_NULL(m_WorldId))
		{
			b2DestroyWorld(m_WorldId);
			m_WorldId = b2_nullWorldId;
		}
	}

	void PhysicsWorld::Step(float deltaTime, uint32_t substeps)
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		float clampedDt = glm::min(deltaTime, 1.0f / 30.0f);
		b2World_Step(m_WorldId, clampedDt, (int)substeps);
		auto t1 = std::chrono::high_resolution_clock::now();
		m_LastStepMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
	}

	Ref<Rigidbody> PhysicsWorld::CreateRigidbody(const RigidbodySpec& spec)
	{
		return CreateRef<Rigidbody>(m_WorldId, spec);
	}

	void PhysicsWorld::DestroyRigidbody(Ref<Rigidbody> body)
	{
		// Body destructor calls b2DestroyBody — just release the ref
		body.reset();
	}

	Ref<PhysicsMaterial> PhysicsWorld::CreateMaterial(float staticFriction, float dynamicFriction, float restitution)
	{
		return CreateRef<PhysicsMaterial>(staticFriction, dynamicFriction, restitution);
	}

	Ref<BoxCollider> PhysicsWorld::CreateBoxCollider(const BoxColliderSpec& spec, Ref<PhysicsMaterial> material)
	{
		return CreateRef<BoxCollider>(spec, material);
	}

	Ref<CircleCollider> PhysicsWorld::CreateCircleCollider(const CircleColliderSpec& spec, Ref<PhysicsMaterial> material)
	{
		return CreateRef<CircleCollider>(spec, material);
	}

	Ref<CapsuleCollider> PhysicsWorld::CreateCapsuleCollider(const CapsuleColliderSpec& spec, Ref<PhysicsMaterial> material)
	{
		return CreateRef<CapsuleCollider>(spec, material);
	}

	void PhysicsWorld::SetGravity(const glm::vec3& gravity)
	{
		b2World_SetGravity(m_WorldId, { gravity.x, gravity.y });
	}

	glm::vec3 PhysicsWorld::GetGravity() const
	{
		b2Vec2 g = b2World_GetGravity(m_WorldId);
		return { g.x, g.y, 0.0f };
	}
}
