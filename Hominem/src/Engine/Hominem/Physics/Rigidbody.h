#pragma once

#include "Hominem/Core/Core.h"
#include "PhysicsTypes.h"
#include "Collider.h"

#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hominem {

	class Rigidbody : public RefCounted
	{
	public:
		Rigidbody(b2WorldId worldId, const RigidbodySpec& spec);
		~Rigidbody();

		RigidbodyType GetType() const { return m_Type; }

		void SetPosition(const glm::vec3& position);
		glm::vec3 GetPosition() const;

		void SetLinearVelocity(const glm::vec3& velocity);
		glm::vec3 GetLinearVelocity() const;

		void AddForce(const glm::vec3& force);
		void AddImpulse(const glm::vec3& impulse);

		void SetGravityEnabled(bool enabled);

		// Attaches collider and creates the underlying Box2D shape
		void AttachCollider(Ref<Collider> collider);
		void DetachCollider(Ref<Collider> collider);

		b2BodyId GetBodyId() const { return m_BodyId; }

	private:
		friend class PhysicsWorld;

		b2WorldId  m_WorldId;
		b2BodyId   m_BodyId  = b2_nullBodyId;
		RigidbodyType m_Type;
		float      m_ZPosition = 0.0f;   // keep Z fixed for 2.5D
		std::vector<Ref<Collider>> m_Colliders;
	};
}
