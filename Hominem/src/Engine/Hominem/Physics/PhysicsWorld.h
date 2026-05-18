#pragma once

#include "Hominem/Core/Core.h"
#include "PhysicsTypes.h"
#include "PhysicsMaterial.h"
#include "Rigidbody.h"
#include "Collider.h"

#include <box2d/box2d.h>

namespace Hominem {

	class PhysicsWorld : public RefCounted
	{
	public:
		PhysicsWorld(const glm::vec3& gravity = glm::vec3(0.0f, -9.8f, 0.0f));
		~PhysicsWorld();

		void Step(float deltaTime, uint32_t substeps = 4);

		Ref<Rigidbody>       CreateRigidbody(const RigidbodySpec& spec);
		void                 DestroyRigidbody(Ref<Rigidbody> body);

		Ref<PhysicsMaterial> CreateMaterial(float staticFriction = 0.5f, float dynamicFriction = 0.5f, float restitution = 0.0f);

		Ref<BoxCollider>     CreateBoxCollider(const BoxColliderSpec& spec, Ref<PhysicsMaterial> material);
		Ref<CircleCollider>  CreateCircleCollider(const CircleColliderSpec& spec, Ref<PhysicsMaterial> material);
		Ref<CapsuleCollider> CreateCapsuleCollider(const CapsuleColliderSpec& spec, Ref<PhysicsMaterial> material);

		void       SetGravity(const glm::vec3& gravity);
		glm::vec3  GetGravity() const;

		b2WorldId  GetWorldId() const { return m_WorldId; }

	private:
		b2WorldId m_WorldId = b2_nullWorldId;
	};
}
