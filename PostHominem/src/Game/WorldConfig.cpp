#include "hmnpch.h"
#include "Game/WorldConfig.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Physics/PhysicsWorld.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace nlohmann {

	static json vec3_to_json(const glm::vec3& v) { return { {"x", v.x}, {"y", v.y}, {"z", v.z} }; }

	static void from_json(const json& j, glm::vec3& v) { v.x = j.at("x"); v.y = j.at("y"); v.z = j.at("z"); }

	static void from_json(const json& j, SceneConfig& c)
	{
		if (j.contains("mesh_path")) j.at("mesh_path").get_to(c.MeshPath);
		if (j.contains("position"))  j.at("position").get_to(c.Position);
		if (j.contains("rotation"))  j.at("rotation").get_to(c.Rotation);
		if (j.contains("scale"))     j.at("scale").get_to(c.Scale);
	}

	static void from_json(const json& j, Hominem::Light& l)
	{
		if (j.contains("position"))      j.at("position").get_to(l.Position);
		if (j.contains("color"))         j.at("color").get_to(l.Color);
		if (j.contains("direction"))     j.at("direction").get_to(l.Direction);
		if (j.contains("intensity"))     l.Intensity    = j.at("intensity").get<float>();
		if (j.contains("radius"))        l.Radius       = j.at("radius").get<float>();
		if (j.contains("inner_angle"))   l.InnerAngle   = j.at("inner_angle").get<float>();
		if (j.contains("outer_angle"))   l.OuterAngle   = j.at("outer_angle").get<float>();
		if (j.contains("source_radius")) l.SourceRadius = j.at("source_radius").get<float>();
		if (j.contains("type"))          l.Type = static_cast<Hominem::LightType>(j.at("type").get<uint32_t>());
	}

	static void from_json(const json& j, WorldConfig& c)
	{
		if (j.contains("scene")) j.at("scene").get_to(c.Scene);

		// Lights
		const char* lightsKey = j.contains("lights") ? "lights" : "point_lights";
		if (j.contains(lightsKey))
		{
			c.Lights.clear();
			for (const auto& entry : j.at(lightsKey))
			{
				Hominem::Light l;
				entry.get_to(l);
				c.Lights.push_back(l);
			}
		}

		// Floor position only — friction and scale are constants in FloorConfig
		if (j.contains("floor") && j.at("floor").contains("position"))
			j.at("floor").at("position").get_to(c.Floor.Position);
		else if (j.contains("physics") && j.at("physics").contains("floor"))
			j.at("physics").at("floor").at("position").get_to(c.Floor.Position);

		// Saved camera world position
		if (j.contains("camera"))
		{
			const auto& cam = j.at("camera");
			if (cam.contains("camera_x")) c.CameraX = cam.at("camera_x").get<float>();
			if (cam.contains("camera_z")) c.CameraZ = cam.at("camera_z").get<float>();
		}

		// Player spawn position (bootstrapped from scene AABB on first load)
		if (j.contains("player") && j.at("player").contains("spawn"))
			j.at("player").at("spawn").at("position").get_to(c.PlayerSpawnPos);
	}

}

static std::string ResolvePath(const std::string& path)
{
#ifdef HMN_SOURCE_RESOURCES_PATH
	auto it = path.find("Resources/");
	if (it != std::string::npos)
		return std::string(HMN_SOURCE_RESOURCES_PATH) + "/" + path.substr(it + 10);
#endif
	return path;
}

void WorldConfig::ApplyToScene(const WorldConfig& cfg, Hominem::Scene& scene)
{
	scene.SetPhysicsWorld(Hominem::CreateRef<Hominem::PhysicsWorld>(PhysicsConfig{}.Gravity));
	scene.GetLights() = cfg.Lights;
}

bool WorldConfig::LoadFromFile(const std::string& path, WorldConfig& out)
{
	const std::string resolved = ResolvePath(path);
	try
	{
		std::ifstream file(resolved);
		if (!file.is_open())
		{
			HMN_CORE_ERROR("WorldConfig: cannot open '{}'", resolved);
			return false;
		}
		json::parse(file).get_to(out);
		return true;
	}
	catch (const json::exception& e)
	{
		HMN_CORE_ERROR("WorldConfig: parse error in '{}': {}", resolved, e.what());
		return false;
	}
}

bool WorldConfig::SaveToFile(const std::string& path, const WorldConfig& cfg)
{
	using namespace nlohmann;

	const std::string resolved = ResolvePath(path);
	try
	{
		json root;
		{
			std::ifstream in(resolved);
			if (in.is_open()) root = json::parse(in);
		}

		root["scene"] = {
			{ "mesh_path", cfg.Scene.MeshPath              },
			{ "position",  vec3_to_json(cfg.Scene.Position) },
			{ "rotation",  vec3_to_json(cfg.Scene.Rotation) },
			{ "scale",     vec3_to_json(cfg.Scene.Scale)    }
		};

		root["floor"] = { { "position", vec3_to_json(cfg.Floor.Position) } };

		root["camera"] = {
			{ "camera_x", cfg.CameraX },
			{ "camera_z", cfg.CameraZ }
		};

		root["player"] = {
			{ "spawn", { { "position", vec3_to_json(cfg.PlayerSpawnPos) } } }
		};

		json lightsArr = json::array();
		for (const auto& l : cfg.Lights)
		{
			lightsArr.push_back({
				{ "position",     vec3_to_json(l.Position)  },
				{ "color",        vec3_to_json(l.Color)     },
				{ "direction",    vec3_to_json(l.Direction) },
				{ "intensity",    l.Intensity               },
				{ "radius",       l.Radius                  },
				{ "inner_angle",  l.InnerAngle              },
				{ "outer_angle",  l.OuterAngle              },
				{ "source_radius",l.SourceRadius            },
				{ "type",         static_cast<uint32_t>(l.Type) }
			});
		}
		root["lights"] = lightsArr;

		// Remove stale keys from old format
		root.erase("physics");
		root.erase("player_old");

		std::ofstream out(resolved);
		if (!out.is_open())
		{
			HMN_CORE_ERROR("WorldConfig: cannot write '{}'", resolved);
			return false;
		}
		out << root.dump(2);
		HMN_CORE_INFO("WorldConfig: saved to '{}'", resolved);
		return true;
	}
	catch (const json::exception& e)
	{
		HMN_CORE_ERROR("WorldConfig: save error '{}': {}", path, e.what());
		return false;
	}
}

bool WorldConfig::ModifyAndSave(const std::string& path, const std::function<void(WorldConfig&)>& mutate)
{
	WorldConfig cfg;
	if (!LoadFromFile(path, cfg))
		return false;
	if (mutate) mutate(cfg);
	return SaveToFile(path, cfg);
}
