#include "hmnpch.h"
#include "LevelLoader.h"

#include "Hominem/Scene/Components.h"
#include "Hominem/Scene/Entity.h"

#include <LDtkLoader/Project.hpp>

namespace Hominem {

	// LDtk uses pixel coordinates — convert to world units
	constexpr float PPU = 16.0f;   // pixels per world unit

	// Find a layer by name safely — returns nullptr if not found
	static const ldtk::Layer* FindLayer(const ldtk::Level& level, const std::string& name)
	{
		for (const auto& layer : level.allLayers())
			if (layer.getName() == name)
				return &layer;
		return nullptr;
	}

	bool LevelLoader::Load(const std::string& path, uint32_t levelIndex, Scene& scene, LevelData& outData)
	{
		ldtk::Project project;

		try
		{
			project.loadFromFile(path);
		}
		catch (const std::exception& e)
		{
			HMN_CORE_ERROR("LevelLoader: Failed to load '{0}' — {1}", path, e.what());
			return false;
		}

		const auto& world  = project.getWorld();
		const auto& levels = world.allLevels();

		if (levelIndex >= (uint32_t)levels.size())
		{
			HMN_CORE_ERROR("LevelLoader: Level index {0} out of range (has {1})", levelIndex, levels.size());
			return false;
		}

		const ldtk::Level& level = levels[levelIndex];
		outData.Width  = (uint32_t)level.size.x;
		outData.Height = (uint32_t)level.size.y;

		// ── Collision layer → static physics tiles ───────────────────────────
		const ldtk::Layer* collisionLayer = FindLayer(level, "Collision");
		if (collisionLayer)
		{
			float cellSize = (float)collisionLayer->getCellSize();
			float half     = (cellSize * 0.5f) / PPU;

			for (const auto& gridPos : collisionLayer->getIntGridValPositions(1))
			{
				float wx =  (gridPos.x * cellSize + cellSize * 0.5f) / PPU;
				float wy = -((gridPos.y * cellSize + cellSize * 0.5f) / PPU);

				Entity e = scene.CreateEntity("Tile");
				e.GetComponent<TransformComponent>().Translation = { wx, wy, 0.0f };

				auto& rb = e.AddComponent<RigidbodyComponent>();
				rb.Type  = RigidbodyComponent::BodyType::Static;

				auto& col       = e.AddComponent<BoxColliderComponent>();
				col.HalfExtents = { half, half };
				col.StaticFriction  = 0.8f;
				col.DynamicFriction = 0.6f;
			}
		}
		else
		{
			HMN_CORE_WARN("LevelLoader: No 'Collision' IntGrid layer found in level '{0}'", level.name);
		}

		// ── Entities layer → spawns, platforms, triggers ─────────────────────
		const ldtk::Layer* entLayer = FindLayer(level, "Entities");
		if (entLayer)
		{
			for (const auto& entity : entLayer->allEntities())
			{
				float wx =  (float)entity.getPosition().x / PPU;
				float wy = -((float)entity.getPosition().y / PPU);

				if (entity.getName() == "PlayerSpawn")
				{
					outData.PlayerSpawn = { wx, wy };
				}
				else if (entity.getName() == "Platform")
				{
					float hw = (entity.getSize().x * 0.5f) / PPU;
					float hh = (entity.getSize().y * 0.5f) / PPU;

					Entity e = scene.CreateEntity("Platform");
					e.GetComponent<TransformComponent>().Translation = { wx, wy, 0.0f };

					auto& rb = e.AddComponent<RigidbodyComponent>();
					rb.Type  = RigidbodyComponent::BodyType::Static;

					auto& col       = e.AddComponent<BoxColliderComponent>();
					col.HalfExtents = { hw, hh };

					outData.PlatformCount++;
				}
			}
		}

		HMN_CORE_INFO("LevelLoader: Loaded level '{0}' — spawn ({1},{2}), {3} platforms, {4}x{5}px",
			level.name,
			outData.PlayerSpawn.x, outData.PlayerSpawn.y,
			outData.PlatformCount,
			outData.Width, outData.Height);

		return true;
	}
}
