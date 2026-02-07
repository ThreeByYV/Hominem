#include "hmnpch.h"
#include "PhysicsWorld.h"

#include <physx/PxPhysicsAPI.h>

using namespace physx;

namespace Hominem {

	// Helper conversions
	static glm::vec3 PxVec3ToGlm(const PxVec3& v)
	{
		return glm::vec3(v.x, v.y, v.z);
	}

	static PxVec3 GlmToPxVec3(const glm::vec3& v)
	{
		return PxVec3(v.x, v.y, v.z);
	}

	static PxQuat GlmToPxQuat(const glm::quat& q)
	{
		return PxQuat(q.x, q.y, q.z, q.w);
	}

	PhysicsWorld::PhysicsWorld(const glm::vec3& gravity, bool enablePVD)
	{
		InitializePhysX(gravity, enablePVD);

		// Create default material
		m_DefaultMaterial = CreateMaterial(0.5f, 0.5f, 0.0f);

		HMN_CORE_INFO("PhysicsWorld initialized with PhysX 5.5");
	}

	PhysicsWorld::~PhysicsWorld()
	{
		ShutdownPhysX();
		HMN_CORE_INFO("PhysicsWorld shut down");
	}

	void PhysicsWorld::InitializePhysX(const glm::vec3& gravity, bool enablePVD)
	{
		// Create allocator and error callback
		m_Allocator = new PxDefaultAllocator();
		m_ErrorCallback = new PxDefaultErrorCallback();

		// Create foundation
		m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, *m_Allocator, *m_ErrorCallback);
		HMN_CORE_ASSERT(m_Foundation, "PxCreateFoundation failed!");

		// Create PVD (PhysX Visual Debugger) if enabled
		if (enablePVD)
		{
			m_Pvd = PxCreatePvd(*m_Foundation);
			if (m_Pvd)
			{
				PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
				if (transport)
				{
					m_Pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
					HMN_CORE_INFO("PhysX Visual Debugger connected on localhost:5425");
				}
			}
		}

		// Create physics SDK
		PxTolerancesScale toleranceScale;
		toleranceScale.length = 1.0f;   // 1 unit = 1 meter
		toleranceScale.speed = 10.0f;   // Typical object speeds

		m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, toleranceScale, true, m_Pvd);
		HMN_CORE_ASSERT(m_Physics, "PxCreatePhysics failed!");

		// Create CPU dispatcher (2 worker threads)
		m_Dispatcher = PxDefaultCpuDispatcherCreate(2);
		HMN_CORE_ASSERT(m_Dispatcher, "PxDefaultCpuDispatcherCreate failed!");

		// Create scene
		PxSceneDesc sceneDesc(m_Physics->getTolerancesScale());
		sceneDesc.gravity = GlmToPxVec3(gravity);
		sceneDesc.cpuDispatcher = m_Dispatcher;
		sceneDesc.filterShader = PxDefaultSimulationFilterShader;

		m_Scene = m_Physics->createScene(sceneDesc);
		HMN_CORE_ASSERT(m_Scene, "createScene failed!");

		// Enable debug visualization (collision shapes, contacts)
		m_Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0f);
		m_Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
	}

	void PhysicsWorld::ShutdownPhysX()
	{
		// Release in reverse order of creation
		if (m_Scene)
		{
			m_Scene->release();
			m_Scene = nullptr;
		}

		if (m_Dispatcher)
		{
			m_Dispatcher->release();
			m_Dispatcher = nullptr;
		}

		if (m_Physics)
		{
			m_Physics->release();
			m_Physics = nullptr;
		}

		if (m_Pvd)
		{
			PxPvdTransport* transport = m_Pvd->getTransport();
			if (transport)
				transport->release();
			m_Pvd->release();
			m_Pvd = nullptr;
		}

		if (m_Foundation)
		{
			m_Foundation->release();
			m_Foundation = nullptr;
		}

		// Delete allocator and error callback
		delete m_Allocator;
		delete m_ErrorCallback;
		m_Allocator = nullptr;
		m_ErrorCallback = nullptr;
	}

	// ========== Simulation ==========

	void PhysicsWorld::Step(float deltaTime, uint32_t substeps)
	{
		if (!m_Scene)
			return;

		// PhysX recommends fixed timesteps for stability
		// Clamp to max 60fps to avoid spiral of death
		float clampedDt = glm::min(deltaTime, 1.0f / 60.0f);

		if (substeps > 1)
		{
			float subDt = clampedDt / static_cast<float>(substeps);
			for (uint32_t i = 0; i < substeps; i++)
			{
				m_Scene->simulate(subDt);
				m_Scene->fetchResults(true);
			}
		}
		else
		{
			m_Scene->simulate(clampedDt);
			m_Scene->fetchResults(true);
		}
	}

	// ========== Rigidbody Creation ==========

	Ref<Rigidbody> PhysicsWorld::CreateRigidbody(const RigidbodySpec& spec)
	{
		if (!m_Physics || !m_Scene)
			return nullptr;

		PxTransform transform(GlmToPxVec3(spec.Position), GlmToPxQuat(spec.Rotation));
		PxRigidActor* actor = nullptr;

		switch (spec.Type)
		{
		case RigidbodyType::Static:
		{
			actor = m_Physics->createRigidStatic(transform);
			break;
		}
		case RigidbodyType::Dynamic:
		{
			PxRigidDynamic* dynamic = m_Physics->createRigidDynamic(transform);
			if (dynamic)
			{
				dynamic->setMass(spec.Mass);
				dynamic->setLinearDamping(spec.LinearDamping);
				dynamic->setAngularDamping(spec.AngularDamping);

				// Apply 2.5D constraints if specified
				if (spec.LockRotationX || spec.LockRotationY || spec.LockRotationZ || spec.LockTranslationZ)
				{
					PxRigidDynamicLockFlags lockFlags(0);
					if (spec.LockRotationX) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
					if (spec.LockRotationY) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
					if (spec.LockRotationZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
					if (spec.LockTranslationZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
					dynamic->setRigidDynamicLockFlags(lockFlags);
				}

				actor = dynamic;
			}
			break;
		}
		case RigidbodyType::Kinematic:
		{
			PxRigidDynamic* kinematic = m_Physics->createRigidDynamic(transform);
			if (kinematic)
			{
				kinematic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
				kinematic->setMass(spec.Mass);

				// Apply 2.5D constraints if specified
				if (spec.LockRotationX || spec.LockRotationY || spec.LockRotationZ || spec.LockTranslationZ)
				{
					PxRigidDynamicLockFlags lockFlags(0);
					if (spec.LockRotationX) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
					if (spec.LockRotationY) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
					if (spec.LockRotationZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
					if (spec.LockTranslationZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
					kinematic->setRigidDynamicLockFlags(lockFlags);
				}

				actor = kinematic;
			}
			break;
		}
		}

		if (!actor)
		{
			HMN_CORE_ERROR("Failed to create rigidbody");
			return nullptr;
		}

		// Add to scene
		m_Scene->addActor(*actor);

		// Create wrapper
		return CreateRef<Rigidbody>(actor, spec.Type);
	}

	void PhysicsWorld::DestroyRigidbody(Ref<Rigidbody> body)
	{
		if (!body || !m_Scene)
			return;

		PxRigidActor* actor = body->GetNativeActor();
		if (actor)
		{
			m_Scene->removeActor(*actor);
			actor->release();
		}
	}

	// ========== Material Creation ==========

	Ref<PhysicsMaterial> PhysicsWorld::CreateMaterial(float staticFriction, float dynamicFriction, float restitution)
	{
		if (!m_Physics)
			return nullptr;

		PxMaterial* material = m_Physics->createMaterial(staticFriction, dynamicFriction, restitution);
		if (!material)
		{
			HMN_CORE_ERROR("Failed to create physics material");
			return nullptr;
		}

		return CreateRef<PhysicsMaterial>(material);
	}

	// ========== Collider Creation ==========

	Ref<BoxCollider> PhysicsWorld::CreateBoxCollider(const glm::vec3& halfExtents, Ref<PhysicsMaterial> material, const glm::vec3& offset)
	{
		if (!m_Physics)
			return nullptr;

		if (!material)
			material = m_DefaultMaterial;

		PxBoxGeometry geometry(GlmToPxVec3(halfExtents));
		PxShape* shape = m_Physics->createShape(geometry, *material->GetNativeMaterial());

		if (!shape)
		{
			HMN_CORE_ERROR("Failed to create box collider");
			return nullptr;
		}

		// Set local pose (offset)
		if (glm::length(offset) > 0.0001f)
		{
			PxTransform localPose(GlmToPxVec3(offset));
			shape->setLocalPose(localPose);
		}

		Ref<BoxCollider> collider = CreateRef<BoxCollider>(shape);
		collider->SetMaterial(material);
		return collider;
	}

	Ref<SphereCollider> PhysicsWorld::CreateSphereCollider(float radius, Ref<PhysicsMaterial> material, const glm::vec3& offset)
	{
		if (!m_Physics)
			return nullptr;

		if (!material)
			material = m_DefaultMaterial;

		PxSphereGeometry geometry(radius);
		PxShape* shape = m_Physics->createShape(geometry, *material->GetNativeMaterial());

		if (!shape)
		{
			HMN_CORE_ERROR("Failed to create sphere collider");
			return nullptr;
		}

		// Set local pose (offset)
		if (glm::length(offset) > 0.0001f)
		{
			PxTransform localPose(GlmToPxVec3(offset));
			shape->setLocalPose(localPose);
		}

		Ref<SphereCollider> collider = CreateRef<SphereCollider>(shape);
		collider->SetMaterial(material);
		return collider;
	}

	Ref<CapsuleCollider> PhysicsWorld::CreateCapsuleCollider(float radius, float halfHeight, Ref<PhysicsMaterial> material, const glm::vec3& offset)
	{
		if (!m_Physics)
			return nullptr;

		if (!material)
			material = m_DefaultMaterial;

		PxCapsuleGeometry geometry(radius, halfHeight);
		PxShape* shape = m_Physics->createShape(geometry, *material->GetNativeMaterial());

		if (!shape)
		{
			HMN_CORE_ERROR("Failed to create capsule collider");
			return nullptr;
		}

		// Set local pose (offset)
		if (glm::length(offset) > 0.0001f)
		{
			PxTransform localPose(GlmToPxVec3(offset));
			shape->setLocalPose(localPose);
		}

		Ref<CapsuleCollider> collider = CreateRef<CapsuleCollider>(shape);
		collider->SetMaterial(material);
		return collider;
	}

	// ========== Collider Creation with Specs ==========

	Ref<BoxCollider> PhysicsWorld::CreateBoxCollider(const BoxColliderSpec& spec, Ref<PhysicsMaterial> material)
	{
		if (!m_Physics)
			return nullptr;

		if (!material)
			material = m_DefaultMaterial;

		PxBoxGeometry geometry(GlmToPxVec3(spec.HalfExtents));
		PxShape* shape = m_Physics->createShape(geometry, *material->GetNativeMaterial());

		if (!shape)
		{
			HMN_CORE_ERROR("Failed to create box collider");
			return nullptr;
		}

		// Set local pose (offset + rotation)
		PxTransform localPose(GlmToPxVec3(spec.Offset), GlmToPxQuat(spec.Rotation));
		shape->setLocalPose(localPose);

		// Set trigger flag
		if (spec.IsTrigger)
		{
			shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
		}

		Ref<BoxCollider> collider = CreateRef<BoxCollider>(shape);
		collider->SetMaterial(material);
		return collider;
	}

	Ref<SphereCollider> PhysicsWorld::CreateSphereCollider(const SphereColliderSpec& spec, Ref<PhysicsMaterial> material)
	{
		if (!m_Physics)
			return nullptr;

		if (!material)
			material = m_DefaultMaterial;

		PxSphereGeometry geometry(spec.Radius);
		PxShape* shape = m_Physics->createShape(geometry, *material->GetNativeMaterial());

		if (!shape)
		{
			HMN_CORE_ERROR("Failed to create sphere collider");
			return nullptr;
		}

		// Set local pose (offset)
		PxTransform localPose(GlmToPxVec3(spec.Offset));
		shape->setLocalPose(localPose);

		// Set trigger flag
		if (spec.IsTrigger)
		{
			shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
		}

		Ref<SphereCollider> collider = CreateRef<SphereCollider>(shape);
		collider->SetMaterial(material);
		return collider;
	}

	Ref<CapsuleCollider> PhysicsWorld::CreateCapsuleCollider(const CapsuleColliderSpec& spec, Ref<PhysicsMaterial> material)
	{
		if (!m_Physics)
			return nullptr;

		if (!material)
			material = m_DefaultMaterial;

		PxCapsuleGeometry geometry(spec.Radius, spec.HalfHeight);
		PxShape* shape = m_Physics->createShape(geometry, *material->GetNativeMaterial());

		if (!shape)
		{
			HMN_CORE_ERROR("Failed to create capsule collider");
			return nullptr;
		}

		// Set local pose (offset + rotation)
		PxTransform localPose(GlmToPxVec3(spec.Offset), GlmToPxQuat(spec.Rotation));
		shape->setLocalPose(localPose);

		// Set trigger flag
		if (spec.IsTrigger)
		{
			shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
		}

		Ref<CapsuleCollider> collider = CreateRef<CapsuleCollider>(shape);
		collider->SetMaterial(material);
		return collider;
	}

	// ========== Gravity ==========

	void PhysicsWorld::SetGravity(const glm::vec3& gravity)
	{
		if (m_Scene)
			m_Scene->setGravity(GlmToPxVec3(gravity));
	}

	glm::vec3 PhysicsWorld::GetGravity() const
	{
		if (m_Scene)
			return PxVec3ToGlm(m_Scene->getGravity());
		return glm::vec3(0.0f);
	}

	// ========== Debug Visualization ==========

	void PhysicsWorld::SetVisualizationEnabled(bool enabled)
	{
		if (m_Scene)
		{
			float scale = enabled ? 1.0f : 0.0f;
			m_Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, scale);
		}
	}

	bool PhysicsWorld::IsVisualizationEnabled() const
	{
		if (m_Scene)
		{
			float scale = m_Scene->getVisualizationParameter(PxVisualizationParameter::eSCALE);
			return scale > 0.0f;
		}
		return false;
	}

}
