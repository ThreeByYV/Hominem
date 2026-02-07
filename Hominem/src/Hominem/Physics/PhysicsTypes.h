#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hominem {

	// Rigidbody type matching PhysX actor types
	enum class RigidbodyType
	{
		Static = 0,    // Immovable (terrain, walls)
		Dynamic,       // Fully simulated (player, objects)
		Kinematic      // Animated but affects dynamic bodies (moving platforms)
	};

	// Collider type for runtime type checking
	enum class ColliderType
	{
		Box = 0,
		Sphere,
		Capsule,
		Mesh
	};

	// Specification for creating a rigidbody
	struct RigidbodySpec
	{
		RigidbodyType Type = RigidbodyType::Static;
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = glm::identity<glm::quat>();

		// 2.5D constraints 
		bool LockRotationX = false;  // Pitch
		bool LockRotationY = false;  // Yaw
		bool LockRotationZ = false;  // Roll
		bool LockTranslationZ = false; // Depth movement

		float Mass = 1.0f;
		float LinearDamping = 0.0f;
		float AngularDamping = 0.05f;
	};

	// Specifications for creating certain Colliders 
	struct BoxColliderSpec
	{
		glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = glm::identity<glm::quat>();
		bool IsTrigger = false;
	};

	struct SphereColliderSpec
	{
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
	};

	struct CapsuleColliderSpec
	{
		float Radius = 0.5f;
		float HalfHeight = 1.0f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = glm::identity<glm::quat>();
		bool IsTrigger = false;
	};

}
