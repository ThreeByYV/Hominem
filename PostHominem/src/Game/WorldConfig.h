#pragma once

#include "Hominem/Scene/Actor.h"

#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <vector>

namespace Hominem { struct Light; enum class LightType : uint32_t; class Scene; }

struct PlayerMovementConfig
{
	float Speed = 5.0f;
	float Mass  = 10.0f;
};

struct PlayerSpawnConfig
{
	glm::vec3 Position = { 0.0f, 1.0f, 3.0f };
	glm::vec3 Scale    = { 1.0f, 1.0f, 1.0f }; // meshes import at metres (aiProcess_GlobalScale)
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
	glm::vec3            Scale  = { 1.0f, 1.0f, 1.0f };
	float                RestY  = 0.06f;   // world-space Y the player body rests at
};

struct FloorConfig
{
	glm::vec3 Position        = { 0.0f, -6.0f, 0.0f };
	glm::vec3 Scale           = { 20.0f, 0.5f, 20.0f };
	float     StaticFriction  = 0.8f;
	float     DynamicFriction = 0.6f;
};

struct PhysicsConfig
{
	glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f }; // m/s² — matches 1 unit = 1 metre
	FloorConfig Floor;
};

struct CameraConfig
{
	float Smoothing     = 0.15f;
	// Orthographic (SandboxLayer / cinematic camera)
	float OrthoSize     = 10.0f;
	float OrthoNear     = -10.0f;
	float OrthoFar      = 10.0f;
	// SideScrollerCamera (GameLayer)
	float VisibleHeight = 2.0f;
	float PlayerScreenY = 0.25f;
	float YBias         = 0.28f;
	float FOVDeg        = 30.f;
	float XSpeed        = 0.12f;
	float YSpeed        = 0.05f;
	float LeadStrength  = 0.45f;
	float YDeadZone     = 0.15f;
	// Saved camera world position; 0 = auto-compute from scene AABB on first run
	float CameraX       = 0.0f;
	float CameraZ       = 0.0f;
};

struct SceneConfig
{
	std::string MeshPath = "Resources/Textures/source/factory.fbx";
	glm::vec3   Position = {  0.f,  0.f,  -8.f };
	glm::vec3   Rotation = {  0.f,  0.f,   0.f }; // degrees, applied as XYZ Euler
	glm::vec3   Scale    = {  1.f,  1.f,   1.f };

	/// Pushes Position/Rotation(->radians)/Scale onto an actor in one call.
	void ApplyTo(Hominem::Actor& actor) const
	{
		actor.SetTransformDeg(Position, Rotation, Scale);
	}
};

struct LightConfig
{
	glm::vec3 Position   = { 0.f, 1.f, 0.f };
	glm::vec3 Color      = { 1.f, 1.f, 1.f };
	glm::vec3 Direction  = { 0.f,-1.f, 0.f };
	float     Intensity  = 5.0f;
	float     Radius     = 3.0f;
	float     InnerAngle = 25.f;
	float     OuterAngle = 35.f;
	float     SourceRadius = 0.15f;
	uint32_t  Type       = 0; // 0=point, 1=spot
};

struct WorldConfig
{
	PlayerConfig                Player;
	PhysicsConfig               Physics;
	CameraConfig                Camera;
	SceneConfig                 Scene;
	std::vector<LightConfig> Lights;

	static constexpr const char* k_Path = "Resources/Config/game_config.json";

	static bool LoadFromFile(const std::string& path, WorldConfig& out);
	static bool SaveToFile(const std::string& path, const WorldConfig& in);

	/// Load -> mutate -> Save in one call. Returns false (mutate/save skipped) if the load failed.
	static bool ModifyAndSave(const std::string& path, const std::function<void(WorldConfig&)>& mutate);

	/// Copy LightConfig entries from this config into a runtime light vector.
	static void ApplyLights(const WorldConfig& cfg, std::vector<Hominem::Light>& out);
	/// Snapshot a runtime light vector back into this config ready for SaveToFile.
	static void CaptureLights(WorldConfig& cfg, const std::vector<Hominem::Light>& in);
	/// Apply Physics (gravity) and Lights to a scene in one call.
	static void ApplyToScene(const WorldConfig& cfg, Hominem::Scene& scene);
};
