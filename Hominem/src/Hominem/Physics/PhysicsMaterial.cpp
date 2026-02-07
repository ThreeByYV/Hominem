#include "hmnpch.h"
#include "PhysicsMaterial.h"

#include <physx/PxPhysicsAPI.h>

using namespace physx;

namespace Hominem {

	PhysicsMaterial::PhysicsMaterial(PxMaterial* material)
		: m_Material(material)
	{
		HMN_CORE_ASSERT(m_Material, "PhysicsMaterial created with null PxMaterial!");
	}

	PhysicsMaterial::~PhysicsMaterial()
	{
		// Material is released by PhysicsWorld, not here
		// PhysX materials are shared resources managed by PxPhysics
	}

	void PhysicsMaterial::SetStaticFriction(float friction)
	{
		if (m_Material)
			m_Material->setStaticFriction(friction);
	}

	float PhysicsMaterial::GetStaticFriction() const
	{
		return m_Material ? m_Material->getStaticFriction() : 0.0f;
	}

	void PhysicsMaterial::SetDynamicFriction(float friction)
	{
		if (m_Material)
			m_Material->setDynamicFriction(friction);
	}

	float PhysicsMaterial::GetDynamicFriction() const
	{
		return m_Material ? m_Material->getDynamicFriction() : 0.0f;
	}

	void PhysicsMaterial::SetRestitution(float restitution)
	{
		if (m_Material)
			m_Material->setRestitution(restitution);
	}

	float PhysicsMaterial::GetRestitution() const
	{
		return m_Material ? m_Material->getRestitution() : 0.0f;
	}

}
