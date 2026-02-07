#include "hmnpch.h"
#include "Rigidbody.h"

#include <physx/PxPhysicsAPI.h>

using namespace physx;

namespace Hominem {

	// Helper conversions
	static glm::vec3 PxVec3ToGlm(const PxVec3& v)
	{
		return glm::vec3(v.x, v.y, v.z);
	}

	static glm::quat PxQuatToGlm(const PxQuat& q)
	{
		return glm::quat(q.w, q.x, q.y, q.z);
	}

	static PxVec3 GlmToPxVec3(const glm::vec3& v)
	{
		return PxVec3(v.x, v.y, v.z);
	}

	static PxQuat GlmToPxQuat(const glm::quat& q)
	{
		return PxQuat(q.x, q.y, q.z, q.w);
	}

	Rigidbody::Rigidbody(PxRigidActor* actor, RigidbodyType type)
		: m_Actor(actor), m_Type(type)
	{
		HMN_CORE_ASSERT(m_Actor, "Rigidbody created with null PxRigidActor!");
	}

	Rigidbody::~Rigidbody()
	{
		// Actor is released by PhysicsWorld, not here
	}

	// ========== Transform ==========

	void Rigidbody::SetPosition(const glm::vec3& position)
	{
		if (m_Actor)
		{
			PxTransform transform = m_Actor->getGlobalPose();
			transform.p = GlmToPxVec3(position);
			m_Actor->setGlobalPose(transform);
		}
	}

	glm::vec3 Rigidbody::GetPosition() const
	{
		if (m_Actor)
			return PxVec3ToGlm(m_Actor->getGlobalPose().p);
		return glm::vec3(0.0f);
	}

	void Rigidbody::SetRotation(const glm::quat& rotation)
	{
		if (m_Actor)
		{
			PxTransform transform = m_Actor->getGlobalPose();
			transform.q = GlmToPxQuat(rotation);
			m_Actor->setGlobalPose(transform);
		}
	}

	glm::quat Rigidbody::GetRotation() const
	{
		if (m_Actor)
			return PxQuatToGlm(m_Actor->getGlobalPose().q);
		return glm::identity<glm::quat>();
	}

	void Rigidbody::SetTransform(const glm::vec3& position, const glm::quat& rotation)
	{
		if (m_Actor)
		{
			PxTransform transform(GlmToPxVec3(position), GlmToPxQuat(rotation));
			m_Actor->setGlobalPose(transform);
		}
	}

	// ========== Physics Properties ==========

	void Rigidbody::SetMass(float mass)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->setMass(mass);
		}
	}

	float Rigidbody::GetMass() const
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				return dynamic->getMass();
		}
		return 0.0f;
	}

	void Rigidbody::SetLinearDamping(float damping)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->setLinearDamping(damping);
		}
	}

	float Rigidbody::GetLinearDamping() const
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				return dynamic->getLinearDamping();
		}
		return 0.0f;
	}

	void Rigidbody::SetAngularDamping(float damping)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->setAngularDamping(damping);
		}
	}

	float Rigidbody::GetAngularDamping() const
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				return dynamic->getAngularDamping();
		}
		return 0.0f;
	}

	void Rigidbody::SetLinearVelocity(const glm::vec3& velocity)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->setLinearVelocity(GlmToPxVec3(velocity));
		}
	}

	glm::vec3 Rigidbody::GetLinearVelocity() const
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				return PxVec3ToGlm(dynamic->getLinearVelocity());
		}
		return glm::vec3(0.0f);
	}

	void Rigidbody::SetAngularVelocity(const glm::vec3& velocity)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->setAngularVelocity(GlmToPxVec3(velocity));
		}
	}

	glm::vec3 Rigidbody::GetAngularVelocity() const
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				return PxVec3ToGlm(dynamic->getAngularVelocity());
		}
		return glm::vec3(0.0f);
	}

	// ========== Forces ==========

	void Rigidbody::AddForce(const glm::vec3& force)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->addForce(GlmToPxVec3(force));
		}
	}

	void Rigidbody::AddTorque(const glm::vec3& torque)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->addTorque(GlmToPxVec3(torque));
		}
	}

	void Rigidbody::AddImpulse(const glm::vec3& impulse)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->addForce(GlmToPxVec3(impulse), PxForceMode::eIMPULSE);
		}
	}

	// ========== Kinematic Control ==========

	void Rigidbody::SetKinematicTarget(const glm::vec3& position, const glm::quat& rotation)
	{
		if (m_Type == RigidbodyType::Kinematic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
			{
				PxTransform target(GlmToPxVec3(position), GlmToPxQuat(rotation));
				dynamic->setKinematicTarget(target);
			}
		}
	}

	// ========== Constraints ==========

	void Rigidbody::SetLockFlags(bool lockRotX, bool lockRotY, bool lockRotZ, bool lockTransZ)
	{
		if (m_Type == RigidbodyType::Dynamic || m_Type == RigidbodyType::Kinematic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
			{
				PxRigidDynamicLockFlags lockFlags(0);
				if (lockRotX) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
				if (lockRotY) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
				if (lockRotZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
				if (lockTransZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;

				dynamic->setRigidDynamicLockFlags(lockFlags);
			}
		}
	}

	// ========== Colliders ==========

	void Rigidbody::AttachCollider(Ref<Collider> collider)
	{
		if (m_Actor && collider)
		{
			m_Actor->attachShape(*collider->GetNativeShape());
			m_Colliders.push_back(collider);
		}
	}

	void Rigidbody::DetachCollider(Ref<Collider> collider)
	{
		if (m_Actor && collider)
		{
			m_Actor->detachShape(*collider->GetNativeShape());

			// Remove from list
			auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
			if (it != m_Colliders.end())
				m_Colliders.erase(it);
		}
	}

	// ========== Gravity ==========

	void Rigidbody::SetGravityEnabled(bool enabled)
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				dynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !enabled);
		}
	}

	bool Rigidbody::IsGravityEnabled() const
	{
		if (m_Type == RigidbodyType::Dynamic)
		{
			PxRigidDynamic* dynamic = m_Actor->is<PxRigidDynamic>();
			if (dynamic)
				return !(dynamic->getActorFlags() & PxActorFlag::eDISABLE_GRAVITY);
		}
		return false;
	}

}
