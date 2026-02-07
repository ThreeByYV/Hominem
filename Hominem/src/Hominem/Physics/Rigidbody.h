#pragma once

#include "Hominem/Core/Core.h"
#include "PhysicsTypes.h"
#include "Collider.h"

namespace physx {

	class PxRigidActor;
	class PxRigidStatic;
	class PxRigidDynamic;
}

namespace Hominem {

	class Rigidbody
	{
	public:
		~Rigidbody();

		RigidbodyType GetType() const { return m_Type; }

		void SetPosition(const glm::vec3& position);
		glm::vec3 GetPosition() const;

		void SetRotation(const glm::quat& rotation);
		glm::quat GetRotation() const;

		void SetTransform(const glm::vec3& position, const glm::quat& rotation);

		// Physics properties for dynamic bodies only
		void SetMass(float mass);
		float GetMass() const;

		void SetLinearDamping(float damping);
		float GetLinearDamping() const;

		void SetAngularDamping(float damping);
		float GetAngularDamping() const;

		void SetLinearVelocity(const glm::vec3& velocity);
		glm::vec3 GetLinearVelocity() const;

		void SetAngularVelocity(const glm::vec3& velocity);
		glm::vec3 GetAngularVelocity() const;

		// Forces for dynamic bodies only
		void AddForce(const glm::vec3& force);
		void AddTorque(const glm::vec3& torque);
		void AddImpulse(const glm::vec3& impulse);

		// Kinematic control
		void SetKinematicTarget(const glm::vec3& position, const glm::quat& rotation);

		void SetLockFlags(bool lockRotX, bool lockRotY, bool lockRotZ, bool lockTransZ);

		void AttachCollider(Ref<Collider> collider);
		void DetachCollider(Ref<Collider> collider);
		const std::vector<Ref<Collider>>& GetColliders() const { return m_Colliders; }

		void SetGravityEnabled(bool enabled);
		bool IsGravityEnabled() const;

		physx::PxRigidActor* GetNativeActor() const { return m_Actor; }

	public:
		// note: Constructor should only be called by PhysicsWorld
		Rigidbody(physx::PxRigidActor* actor, RigidbodyType type);

	private:
		friend class PhysicsWorld;

		physx::PxRigidActor* m_Actor = nullptr;
		RigidbodyType m_Type;
		std::vector<Ref<Collider>> m_Colliders;
	};

}
