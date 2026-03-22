#include "hmnpch.h"
#include "Rigidbody.h"

#include <box2d/box2d.h>
#include <glm/gtc/quaternion.hpp>

namespace Hominem {

	static b2BodyType ToBox2DBodyType(RigidbodyType type)
	{
		switch (type)
		{
			case RigidbodyType::Static:    return b2_staticBody;
			case RigidbodyType::Dynamic:   return b2_dynamicBody;
			case RigidbodyType::Kinematic: return b2_kinematicBody;
		}
		return b2_staticBody;
	}

	Rigidbody::Rigidbody(b2WorldId worldId, const RigidbodySpec& spec)
		: m_WorldId(worldId), m_Type(spec.Type), m_ZPosition(spec.Position.z)
	{
		b2BodyDef bodyDef      = b2DefaultBodyDef();
		bodyDef.type           = ToBox2DBodyType(spec.Type);
		bodyDef.position       = { spec.Position.x, spec.Position.y };
		bodyDef.motionLocks.angularZ = spec.LockRotationZ;
		bodyDef.linearDamping  = spec.LinearDamping;
		bodyDef.angularDamping = spec.AngularDamping;

		m_BodyId = b2CreateBody(worldId, &bodyDef);

		if (spec.Type == RigidbodyType::Dynamic && spec.Mass > 0.0f)
		{
			b2MassData massData = b2Body_GetMassData(m_BodyId);
			massData.mass = spec.Mass;
			b2Body_SetMassData(m_BodyId, massData);
		}
	}

	Rigidbody::~Rigidbody()
	{
		if (B2_IS_NON_NULL(m_BodyId) && b2World_IsValid(m_WorldId))
			b2DestroyBody(m_BodyId);
	}

	void Rigidbody::SetPosition(const glm::vec3& position)
	{
		m_ZPosition = position.z;
		b2Rot rot = b2Body_GetRotation(m_BodyId);
		b2Body_SetTransform(m_BodyId, { position.x, position.y }, rot);
	}

	glm::vec3 Rigidbody::GetPosition() const
	{
		b2Vec2 pos = b2Body_GetPosition(m_BodyId);
		return { pos.x, pos.y, m_ZPosition };
	}

	void Rigidbody::SetLinearVelocity(const glm::vec3& velocity)
	{
		b2Body_SetLinearVelocity(m_BodyId, { velocity.x, velocity.y });
	}

	glm::vec3 Rigidbody::GetLinearVelocity() const
	{
		b2Vec2 vel = b2Body_GetLinearVelocity(m_BodyId);
		return { vel.x, vel.y, 0.0f };
	}

	void Rigidbody::AddForce(const glm::vec3& force)
	{
		b2Body_ApplyForceToCenter(m_BodyId, { force.x, force.y }, true);
	}

	void Rigidbody::AddImpulse(const glm::vec3& impulse)
	{
		b2Body_ApplyLinearImpulseToCenter(m_BodyId, { impulse.x, impulse.y }, true);
	}

	void Rigidbody::SetGravityEnabled(bool enabled)
	{
		b2Body_SetGravityScale(m_BodyId, enabled ? 1.0f : 0.0f);
	}

	void Rigidbody::AttachCollider(Ref<Collider> collider)
	{
		if (!collider) return;

		b2ShapeDef shapeDef = b2DefaultShapeDef();
		auto mat = collider->GetMaterial();
		if (mat)
		{
			shapeDef.material.friction    = mat->DynamicFriction;
			shapeDef.material.restitution = mat->Restitution;
		}

		switch (collider->GetType())
		{
			case ColliderType::Box:
			{
				auto* bc = static_cast<BoxCollider*>(collider.get());
				const auto& spec = bc->GetSpec();
				shapeDef.isSensor = spec.IsTrigger;
				b2Polygon box = b2MakeOffsetBox(
					spec.HalfExtents.x, spec.HalfExtents.y,
					{ spec.Offset.x, spec.Offset.y },
					b2Rot_identity
				);
				collider->m_ShapeId = b2CreatePolygonShape(m_BodyId, &shapeDef, &box);
				break;
			}
			case ColliderType::Circle:
			{
				auto* cc = static_cast<CircleCollider*>(collider.get());
				const auto& spec = cc->GetSpec();
				shapeDef.isSensor = spec.IsTrigger;
				b2Circle circle = { { spec.Offset.x, spec.Offset.y }, spec.Radius };
				collider->m_ShapeId = b2CreateCircleShape(m_BodyId, &shapeDef, &circle);
				break;
			}
			case ColliderType::Capsule:
			{
				auto* cc = static_cast<CapsuleCollider*>(collider.get());
				const auto& spec = cc->GetSpec();
				shapeDef.isSensor = spec.IsTrigger;
				b2Capsule capsule = {
					{ spec.Offset.x, spec.Offset.y - spec.HalfHeight },
					{ spec.Offset.x, spec.Offset.y + spec.HalfHeight },
					spec.Radius
				};
				collider->m_ShapeId = b2CreateCapsuleShape(m_BodyId, &shapeDef, &capsule);
				break;
			}
		}

		m_Colliders.push_back(collider);
	}

	void Rigidbody::DetachCollider(Ref<Collider> collider)
	{
		if (!collider) return;
		if (B2_IS_NON_NULL(collider->m_ShapeId) && b2World_IsValid(m_WorldId))
		{
			b2DestroyShape(collider->m_ShapeId, false);
			collider->m_ShapeId = b2_nullShapeId;
		}
		m_Colliders.erase(std::remove(m_Colliders.begin(), m_Colliders.end(), collider), m_Colliders.end());
	}
}
