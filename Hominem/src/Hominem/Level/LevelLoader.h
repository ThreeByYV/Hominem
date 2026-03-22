#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Scene/Scene.h"

namespace Hominem {

	// Spawned entity types recognised from LDtk entity identifiers
	enum class LevelEntityType
	{
		Unknown = 0,
		PlayerSpawn,
		Platform,
		DeathZone
	};

	// Data returned after loading a level — useful for post-load setup
	struct LevelData
	{
		glm::vec2 PlayerSpawn = { 0.0f, 0.0f };
		uint32_t  PlatformCount = 0;
		uint32_t  Width  = 0;   // level pixel width
		uint32_t  Height = 0;   // level pixel height
	};

	class LevelLoader
	{
	public:
		// Load a .ldtk project file and spawn the given level index into the scene.
		// Collision tiles become static RigidbodyComponent + BoxColliderComponent entities.
		// Entities (PlayerSpawn, etc.) are parsed and returned in LevelData.
		static bool Load(const std::string& path, uint32_t levelIndex, Scene& scene, LevelData& outData);
	};
}
