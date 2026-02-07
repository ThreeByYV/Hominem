#pragma once

#include "Hominem/Core/Core.h"

namespace physx {

	class PxMaterial;
	class PxPhysics;
}

namespace Hominem {

	// Wrapper for PhysX material (friction, restitution)
	class PhysicsMaterial
	{
	public:
		PhysicsMaterial(physx::PxMaterial* material);
		~PhysicsMaterial();

		// Property accessors
		void SetStaticFriction(float friction);
		float GetStaticFriction() const;

		void SetDynamicFriction(float friction);
		float GetDynamicFriction() const;

		void SetRestitution(float restitution);
		float GetRestitution() const;

		// Internal access for PhysX
		physx::PxMaterial* GetNativeMaterial() const { return m_Material; }

	private:
		physx::PxMaterial* m_Material = nullptr;
	};

}
