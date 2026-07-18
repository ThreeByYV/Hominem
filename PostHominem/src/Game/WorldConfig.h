#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/RenderFrame.h"

#include <glm/glm.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// tweak defaults here, recompile to apply

struct PlayerMovementConfig
{
	float Speed = 4.0f;
	float Mass  = 10.0f;
};

struct PlayerSpawnConfig
{
	glm::vec3 Position = { 0.0f, 1.0f, 3.0f };
	glm::vec3 Scale    = { 1.0f, 1.0f, 1.0f };
};

struct PlayerColliderConfig
{
	glm::vec2 Extents         = { 0.5f, 0.5f };
	glm::vec2 Offset          = { 0.0f, -0.75f };
	float     StaticFriction  = 0.5f;
	float     DynamicFriction = 0.5f;
};

struct PlayerConfig
{
	PlayerMovementConfig Movement;
	PlayerSpawnConfig    Spawn;
	PlayerColliderConfig Collider;
	glm::vec3            Scale = { 1.0f, 1.0f, 1.0f };
	float                RestY = 0.06f;
};

struct FloorConfig
{
	glm::vec3 Position        = {     0.f,  -6.f,     0.f };
	glm::vec3 Scale           = { 10000.f,  0.5f, 10000.f };
	float     StaticFriction  = 0.8f;
	float     DynamicFriction = 0.6f;
};

struct PhysicsConfig
{
	glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
};

struct CameraConfig
{
	float Smoothing     = 0.15f;
	float OrthoSize     = 10.0f;
	float OrthoNear     = -10.0f;
	float OrthoFar      = 10.0f;
	float VisibleHeight = 2.0f;
	float PlayerScreenY = 0.25f;
	float YBias         = 0.28f;
	float FOVDeg        = 30.f;
	float XSpeed        = 0.12f;
	float YSpeed        = 0.05f;
	float LeadStrength  = 0.45f;
	float YDeadZone     = 0.15f;
};

struct SceneConfig
{
	std::string MeshPath = "";
	glm::vec3   Position = { 0.f, 0.f, 0.f };
	glm::vec3   Rotation = { 0.f, 0.f, 0.f };
	glm::vec3   Scale    = { 1.f, 1.f, 1.f };

	void ApplyTo(Hominem::Actor& actor) const { actor.SetTransformDeg(Position, Rotation, Scale); }
};

// --- JSON-persisted: only what varies per scene and is saved via ImGui ---

struct WorldConfig
{
	SceneConfig                  Scene;
	std::vector<Hominem::Light>  Lights;
	FloorConfig                  Floor;
	std::optional<float>         CameraX;
	std::optional<float>         CameraZ;
	glm::vec3                    PlayerSpawnPos = { 0.f, 1.f, 0.f };
	float                        RenderScale    = 0.f; // 0 = let renderer decide via RecommendedRenderScale

	static constexpr const char* k_Path = "Resources/Config/game_config.json";

	static bool LoadFromFile(const std::string& path, WorldConfig& out);
	static bool SaveToFile(const std::string& path, const WorldConfig& in);
	static bool ModifyAndSave(const std::string& path, const std::function<void(WorldConfig&)>& mutate);
};
