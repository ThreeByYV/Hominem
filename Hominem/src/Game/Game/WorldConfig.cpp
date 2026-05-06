#include "hmnpch.h"
#include "Game/WorldConfig.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace nlohmann {

	static void from_json(const json& j, glm::vec2& v) { v.x = j.at("x"); v.y = j.at("y"); }
	static void from_json(const json& j, glm::vec3& v) { v.x = j.at("x"); v.y = j.at("y"); v.z = j.at("z"); }

	static void from_json(const json& j, Hominem::PlayerMovementConfig& c)
	{
		j.at("speed").get_to(c.Speed);
		j.at("mass").get_to(c.Mass);
	}

	static void from_json(const json& j, Hominem::PlayerSpawnConfig& c)
	{
		j.at("position").get_to(c.Position);
		j.at("scale").get_to(c.Scale);
	}

	static void from_json(const json& j, Hominem::PlayerColliderConfig& c)
	{
		j.at("extents").get_to(c.Extents);
		j.at("offset").get_to(c.Offset);
		j.at("static_friction").get_to(c.StaticFriction);
		j.at("dynamic_friction").get_to(c.DynamicFriction);
	}

	static void from_json(const json& j, Hominem::PlayerConfig& c)
	{
		j.at("movement").get_to(c.Movement);
		j.at("spawn").get_to(c.Spawn);
		j.at("collider").get_to(c.Collider);
	}

	static void from_json(const json& j, Hominem::FloorConfig& c)
	{
		j.at("position").get_to(c.Position);
		j.at("scale").get_to(c.Scale);
		j.at("static_friction").get_to(c.StaticFriction);
		j.at("dynamic_friction").get_to(c.DynamicFriction);
	}

	static void from_json(const json& j, Hominem::PhysicsConfig& c)
	{
		j.at("gravity").get_to(c.Gravity);
		j.at("floor").get_to(c.Floor);
	}

	static void from_json(const json& j, Hominem::CameraConfig& c)
	{
		j.at("smoothing").get_to(c.Smoothing);
		j.at("ortho_size").get_to(c.OrthoSize);
		j.at("ortho_near").get_to(c.OrthoNear);
		j.at("ortho_far").get_to(c.OrthoFar);
	}

	static void from_json(const json& j, Hominem::WorldConfig& c)
	{
		j.at("player").get_to(c.Player);
		j.at("physics").get_to(c.Physics);
		j.at("camera").get_to(c.Camera);
	}

}

namespace Hominem {

	bool WorldConfig::LoadFromFile(const std::string& path, WorldConfig& out)
	{
		try
		{
			std::ifstream file(path);
			if (!file.is_open())
			{
				HMN_CORE_ERROR("WorldConfig: cannot open '{}'", path);
				return false;
			}
			json::parse(file).get_to(out);
			return true;
		}
		catch (const json::exception& e)
		{
			HMN_CORE_ERROR("WorldConfig: parse error in '{}': {}", path, e.what());
			return false;
		}
	}

}
