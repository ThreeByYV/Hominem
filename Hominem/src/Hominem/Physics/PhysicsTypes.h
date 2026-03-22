#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Hominem {

	enum class RigidbodyType
	{
		Static = 0,
		Dynamic,
		Kinematic
	};

	enum class ColliderType
	{
		Box = 0,
		Circle,
		Capsule
	};

	struct RigidbodySpec
	{
		RigidbodyType Type = RigidbodyType::Static;
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = glm::identity<glm::quat>();

		bool LockRotationX = false;
		bool LockRotationY = false;
		bool LockRotationZ = false;
		bool LockTranslationZ = false;

		float Mass = 1.0f;
		float LinearDamping = 0.0f;
		float AngularDamping = 0.05f;
	};

	struct BoxColliderSpec
	{
		glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
	};

	struct CircleColliderSpec
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
		bool IsTrigger = false;
	};
}
