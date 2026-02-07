#pragma once

#include "Hominem/Core/Core.h"
#include "PhysicsTypes.h"
#include "PhysicsMaterial.h"

namespace physx {

	class PxShape;
	class PxRigidActor;
}

namespace Hominem {

	// Base collider class wrapping PxShape
	class Collider
	{
	public:
		virtual ~Collider();

		ColliderType GetType() const { return m_Type; }

		void SetMaterial(Ref<PhysicsMaterial> material);
		Ref<PhysicsMaterial> GetMaterial() const { return m_Material; }

		void SetLocalPose(const glm::vec3& offset, const glm::quat& rotation = glm::identity<glm::quat>());
		glm::vec3 GetLocalPosition() const;
		glm::quat GetLocalRotation() const;

		void SetTrigger(bool isTrigger);
		bool IsTrigger() const;

		// Internal PhysX access
		physx::PxShape* GetNativeShape() const { return m_Shape; }

	public:
		// Note: Constructor should only be called by PhysicsWorld
		Collider(physx::PxShape* shape, ColliderType type);

	protected:
		friend class PhysicsWorld;

		physx::PxShape* m_Shape = nullptr;
		ColliderType m_Type;
		Ref<PhysicsMaterial> m_Material;
	};

	// Box collider
	class BoxCollider : public Collider
	{
	public:
		void SetHalfExtents(const glm::vec3& halfExtents);
		glm::vec3 GetHalfExtents() const;

	public:
		// Note: Constructor should only be called by PhysicsWorld
		BoxCollider(physx::PxShape* shape);

	private:
		friend class PhysicsWorld;
	};

	// Sphere collider
	class SphereCollider : public Collider
	{
	public:
		void SetRadius(float radius);
		float GetRadius() const;

	public:
		// Note: Constructor should only be called by PhysicsWorld
		SphereCollider(physx::PxShape* shape);

	private:
		friend class PhysicsWorld;
	};

	// Capsule collider (oriented along Y-axis by default)
	class CapsuleCollider : public Collider
	{
	public:
		void SetRadius(float radius);
		float GetRadius() const;

		void SetHalfHeight(float halfHeight);
		float GetHalfHeight() const;

	public:
		// Note: Constructor should only be called by PhysicsWorld
		CapsuleCollider(physx::PxShape* shape);

	private:
		friend class PhysicsWorld;
	};

}
