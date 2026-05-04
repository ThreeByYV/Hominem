#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Hominem {

	struct PlayerMovementConfig
	{
		float Speed = 5.0f;
		float Mass = 10.0f;
	};

	struct PlayerSpawnConfig
	{
		glm::vec3 Position = { 0.0f, 1.0f, 3.0f };
		glm::vec3 Scale = { 0.01f, 0.01f, 0.01f };
	};

	struct PlayerColliderConfig
	{
		glm::vec2 Extents = { 0.5f, 0.5f };
		glm::vec2 Offset  = { 0.0f, -0.75f };
		float StaticFriction  = 0.5f;
		float DynamicFriction = 0.5f;
	};

	struct PlayerConfig
	{
		PlayerMovementConfig Movement;
		PlayerSpawnConfig Spawn;
		PlayerColliderConfig Collider;
	};

	struct FloorConfig
	{
		glm::vec3 Position = { 0.0f, -6.0f, 0.0f };
		glm::vec3 Scale = { 20.0f, 0.5f, 20.0f };
		float StaticFriction = 0.8f;
		float DynamicFriction = 0.6f;
	};

	struct PhysicsConfig
	{
		glm::vec3 Gravity = { 0.0f, -9.8f, 0.0f };
		FloorConfig Floor;
	};

	struct CameraControllerConfig
	{
		float ControllerSpeed = 5.0f;
		float Smoothing = 0.15f;
		float OrthoSize = 10.0f;
		float OrthoNear = -10.0f;
		float OrthoFar = 10.0f;
	};

	struct GameConfig
	{
		PlayerConfig Player;
		PhysicsConfig Physics;
		CameraControllerConfig Camera;

		static bool LoadFromFile(const std::string& path, GameConfig& out);
	};

}
