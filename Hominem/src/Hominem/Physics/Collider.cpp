#include "hmnpch.h"
#include "Collider.h"

#include <physx/PxPhysicsAPI.h>

using namespace physx;

namespace Hominem {

	// Helper to convert PhysX types to GLM
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

	// ========== Collider Base ==========

	Collider::Collider(PxShape* shape, ColliderType type)
		: m_Shape(shape), m_Type(type)
	{
		HMN_CORE_ASSERT(m_Shape, "Collider created with null PxShape!");
	}

	Collider::~Collider()
	{
		// Shape is released by PhysicsWorld/Rigidbody, not here
	}

	void Collider::SetMaterial(Ref<PhysicsMaterial> material)
	{
		if (m_Shape && material)
		{
			m_Material = material;
			PxMaterial* materials[] = { material->GetNativeMaterial() };
			m_Shape->setMaterials(materials, 1);
		}
	}

	void Collider::SetLocalPose(const glm::vec3& offset, const glm::quat& rotation)
	{
		if (m_Shape)
		{
			PxTransform localPose(GlmToPxVec3(offset), GlmToPxQuat(rotation));
			m_Shape->setLocalPose(localPose);
		}
	}

	glm::vec3 Collider::GetLocalPosition() const
	{
		if (m_Shape)
			return PxVec3ToGlm(m_Shape->getLocalPose().p);
		return glm::vec3(0.0f);
	}

	glm::quat Collider::GetLocalRotation() const
	{
		if (m_Shape)
			return PxQuatToGlm(m_Shape->getLocalPose().q);
		return glm::identity<glm::quat>();
	}

	void Collider::SetTrigger(bool isTrigger)
	{
		if (m_Shape)
		{
			if (isTrigger)
				m_Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			else
				m_Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);

			m_Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		}
	}

	bool Collider::IsTrigger() const
	{
		if (m_Shape)
			return m_Shape->getFlags() & PxShapeFlag::eTRIGGER_SHAPE;
		return false;
	}

	// ========== BoxCollider ==========

	BoxCollider::BoxCollider(PxShape* shape)
		: Collider(shape, ColliderType::Box)
	{
	}

	void BoxCollider::SetHalfExtents(const glm::vec3& halfExtents)
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			PxBoxGeometry box = static_cast<const PxBoxGeometry&>(geom);
			box.halfExtents = GlmToPxVec3(halfExtents);
			m_Shape->setGeometry(box);
		}
	}

	glm::vec3 BoxCollider::GetHalfExtents() const
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			const PxBoxGeometry& box = static_cast<const PxBoxGeometry&>(geom);
			return PxVec3ToGlm(box.halfExtents);
		}
		return glm::vec3(0.5f);
	}

	// ========== SphereCollider ==========

	SphereCollider::SphereCollider(PxShape* shape)
		: Collider(shape, ColliderType::Sphere)
	{
	}

	void SphereCollider::SetRadius(float radius)
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			PxSphereGeometry sphere = static_cast<const PxSphereGeometry&>(geom);
			sphere.radius = radius;
			m_Shape->setGeometry(sphere);
		}
	}

	float SphereCollider::GetRadius() const
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			const PxSphereGeometry& sphere = static_cast<const PxSphereGeometry&>(geom);
			return sphere.radius;
		}
		return 0.5f;
	}

	// ========== CapsuleCollider ==========

	CapsuleCollider::CapsuleCollider(PxShape* shape)
		: Collider(shape, ColliderType::Capsule)
	{
	}

	void CapsuleCollider::SetRadius(float radius)
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			PxCapsuleGeometry capsule = static_cast<const PxCapsuleGeometry&>(geom);
			capsule.radius = radius;
			m_Shape->setGeometry(capsule);
		}
	}

	float CapsuleCollider::GetRadius() const
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			const PxCapsuleGeometry& capsule = static_cast<const PxCapsuleGeometry&>(geom);
			return capsule.radius;
		}
		return 0.5f;
	}

	void CapsuleCollider::SetHalfHeight(float halfHeight)
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			PxCapsuleGeometry capsule = static_cast<const PxCapsuleGeometry&>(geom);
			capsule.halfHeight = halfHeight;
			m_Shape->setGeometry(capsule);
		}
	}

	float CapsuleCollider::GetHalfHeight() const
	{
		if (m_Shape)
		{
			const PxGeometry& geom = m_Shape->getGeometry();
			const PxCapsuleGeometry& capsule = static_cast<const PxCapsuleGeometry&>(geom);
			return capsule.halfHeight;
		}
		return 1.0f;
	}

}
