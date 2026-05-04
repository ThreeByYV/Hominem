#pragma once

#include "Hominem/Core/Core.h"
#include "PhysicsTypes.h"
#include "PhysicsMaterial.h"

#include <box2d/box2d.h>

namespace Hominem {

	// Base collider — holds the spec and material until attached to a Rigidbody
	class Collider
	{
	public:
		virtual ~Collider() = default;
		ColliderType GetType() const { return m_Type; }
		Ref<PhysicsMaterial> GetMaterial() const { return m_Material; }

	protected:
		Collider(ColliderType type, Ref<PhysicsMaterial> material)
			: m_Type(type), m_Material(material) {}

		friend class Rigidbody;
		ColliderType          m_Type;
		Ref<PhysicsMaterial>  m_Material;
		b2ShapeId             m_ShapeId = b2_nullShapeId;
	};

	class BoxCollider : public Collider
	{
	public:
		BoxCollider(const BoxColliderSpec& spec, Ref<PhysicsMaterial> material)
			: Collider(ColliderType::Box, material), m_Spec(spec) {}

		const BoxColliderSpec& GetSpec() const { return m_Spec; }

	private:
		BoxColliderSpec m_Spec;
	};

	class CircleCollider : public Collider
	{
	public:
		CircleCollider(const CircleColliderSpec& spec, Ref<PhysicsMaterial> material)
			: Collider(ColliderType::Circle, material), m_Spec(spec) {}

		const CircleColliderSpec& GetSpec() const { return m_Spec; }

	private:
		CircleColliderSpec m_Spec;
	};

	class CapsuleCollider : public Collider
	{
	public:
		CapsuleCollider(const CapsuleColliderSpec& spec, Ref<PhysicsMaterial> material)
			: Collider(ColliderType::Capsule, material), m_Spec(spec) {}

		const CapsuleColliderSpec& GetSpec() const { return m_Spec; }

	private:
		CapsuleColliderSpec m_Spec;
	};
}
