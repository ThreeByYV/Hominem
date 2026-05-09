#include "hmnpch.h"
#include "Game/WorldConfig.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace nlohmann {

	static json vec2_to_json(const glm::vec2& v) { return { {"x", v.x}, {"y", v.y} }; }
	static json vec3_to_json(const glm::vec3& v) { return { {"x", v.x}, {"y", v.y}, {"z", v.z} }; }

	static void from_json(const json& j, glm::vec2& v) { v.x = j.at("x"); v.y = j.at("y"); }
	static void from_json(const json& j, glm::vec3& v) { v.x = j.at("x"); v.y = j.at("y"); v.z = j.at("z"); }

	static void from_json(const json& j, PlayerMovementConfig& c)
	{
		j.at("speed").get_to(c.Speed);
		j.at("mass").get_to(c.Mass);
	}

	static void from_json(const json& j, PlayerSpawnConfig& c)
	{
		j.at("position").get_to(c.Position);
		j.at("scale").get_to(c.Scale);
	}

	static void from_json(const json& j, PlayerColliderConfig& c)
	{
		j.at("extents").get_to(c.Extents);
		j.at("offset").get_to(c.Offset);
		j.at("static_friction").get_to(c.StaticFriction);
		j.at("dynamic_friction").get_to(c.DynamicFriction);
	}

	static void from_json(const json& j, PlayerConfig& c)
	{
		j.at("movement").get_to(c.Movement);
		j.at("spawn").get_to(c.Spawn);
		j.at("collider").get_to(c.Collider);
	}

	static void from_json(const json& j, FloorConfig& c)
	{
		j.at("position").get_to(c.Position);
		j.at("scale").get_to(c.Scale);
		j.at("static_friction").get_to(c.StaticFriction);
		j.at("dynamic_friction").get_to(c.DynamicFriction);
	}

	static void from_json(const json& j, PhysicsConfig& c)
	{
		j.at("gravity").get_to(c.Gravity);
		j.at("floor").get_to(c.Floor);
	}

	static void from_json(const json& j, CameraConfig& c)
	{
		j.at("smoothing").get_to(c.Smoothing);
		j.at("ortho_size").get_to(c.OrthoSize);
		j.at("ortho_near").get_to(c.OrthoNear);
		j.at("ortho_far").get_to(c.OrthoFar);
	}

	static void from_json(const json& j, SceneConfig& c)
	{
		if (j.contains("mesh_path")) j.at("mesh_path").get_to(c.MeshPath);
		if (j.contains("position"))  j.at("position").get_to(c.Position);
		if (j.contains("rotation"))  j.at("rotation").get_to(c.Rotation);
		if (j.contains("scale"))     j.at("scale").get_to(c.Scale);
	}

	static void from_json(const json& j, WorldConfig& c)
	{
		j.at("player").get_to(c.Player);
		j.at("physics").get_to(c.Physics);
		j.at("camera").get_to(c.Camera);
		if (j.contains("scene")) j.at("scene").get_to(c.Scene);
	}

}

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

bool WorldConfig::SaveToFile(const std::string& path, const WorldConfig& cfg)
{
	using namespace nlohmann;

	try
	{
		// Read existing file first to preserve any keys we don't own
		json root;
		{
			std::ifstream in(path);
			if (in.is_open()) root = json::parse(in);
		}

		root["scene"] = {
			{ "mesh_path", cfg.Scene.MeshPath },
			{ "position",  vec3_to_json(cfg.Scene.Position) },
			{ "rotation",  vec3_to_json(cfg.Scene.Rotation) },
			{ "scale",     vec3_to_json(cfg.Scene.Scale)    }
		};

		std::ofstream out(path);
		if (!out.is_open())
		{
			HMN_CORE_ERROR("WorldConfig: cannot write '{}'", path);
			return false;
		}
		out << root.dump(2);
		HMN_CORE_INFO("WorldConfig: saved scene config to '{}'", path);
		return true;
	}
	catch (const json::exception& e)
	{
		HMN_CORE_ERROR("WorldConfig: save error '{}': {}", path, e.what());
		return false;
	}
}
