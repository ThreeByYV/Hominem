#pragma once

#include "Hominem/Core/Core.h"
#include "PhysicsTypes.h"
#include "PhysicsMaterial.h"
#include "Rigidbody.h"
#include "Collider.h"

namespace physx {

	class PxFoundation;
	class PxPhysics;
	class PxScene;
	class PxDefaultCpuDispatcher;
	class PxPvd;
	class PxDefaultAllocator;
	class PxDefaultErrorCallback;
}

namespace Hominem {

	class PhysicsWorld
	{
	public:
		PhysicsWorld(const glm::vec3& gravity = glm::vec3(0.0f, -9.8f, 0.0f), bool enablePVD = false);
		~PhysicsWorld();

		void Step(float deltaTime, uint32_t substeps = 1);

		Ref<Rigidbody> CreateRigidbody(const RigidbodySpec& spec);
		void DestroyRigidbody(Ref<Rigidbody> body);

		Ref<PhysicsMaterial> CreateMaterial(float staticFriction = 0.5f, float dynamicFriction = 0.5f, float restitution = 0.0f);

		Ref<BoxCollider> CreateBoxCollider(const glm::vec3& halfExtents, Ref<PhysicsMaterial> material, const glm::vec3& offset = glm::vec3(0.0f));
		Ref<SphereCollider> CreateSphereCollider(float radius, Ref<PhysicsMaterial> material, const glm::vec3& offset = glm::vec3(0.0f));
		Ref<CapsuleCollider> CreateCapsuleCollider(float radius, float halfHeight, Ref<PhysicsMaterial> material, const glm::vec3& offset = glm::vec3(0.0f));

		Ref<BoxCollider> CreateBoxCollider(const BoxColliderSpec& spec, Ref<PhysicsMaterial> material);
		Ref<SphereCollider> CreateSphereCollider(const SphereColliderSpec& spec, Ref<PhysicsMaterial> material);
		Ref<CapsuleCollider> CreateCapsuleCollider(const CapsuleColliderSpec& spec, Ref<PhysicsMaterial> material);

		void SetGravity(const glm::vec3& gravity);
		glm::vec3 GetGravity() const;

		void SetVisualizationEnabled(bool enabled);
		bool IsVisualizationEnabled() const;

		physx::PxPhysics* GetPhysics() const { return m_Physics; }
		physx::PxScene* GetScene() const { return m_Scene; }

	private:
		void InitializePhysX(const glm::vec3& gravity, bool enablePVD);
		void ShutdownPhysX();

	private:
		physx::PxFoundation* m_Foundation = nullptr;
		physx::PxPhysics* m_Physics = nullptr;
		physx::PxScene* m_Scene = nullptr;
		physx::PxDefaultCpuDispatcher* m_Dispatcher = nullptr;
		physx::PxPvd* m_Pvd = nullptr;

		// Use heap-allocated default allocator/error callback for PhysX 5.x
		physx::PxDefaultAllocator* m_Allocator = nullptr;
		physx::PxDefaultErrorCallback* m_ErrorCallback = nullptr;

		// Default material
		Ref<PhysicsMaterial> m_DefaultMaterial;
	};

}
