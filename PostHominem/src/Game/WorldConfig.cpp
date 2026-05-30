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
		if (j.contains("scale"))
		{
			const auto& s = j.at("scale");
			if (s.is_object()) s.get_to(c.Scale);
			else { float f = s.get<float>(); c.Scale = { f, f, f }; }
		}
		if (j.contains("rest_y")) c.RestY = j.at("rest_y").get<float>();
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
		if (j.contains("smoothing"))       c.Smoothing     = j.at("smoothing").get<float>();
		if (j.contains("ortho_size"))      c.OrthoSize     = j.at("ortho_size").get<float>();
		if (j.contains("ortho_near"))      c.OrthoNear     = j.at("ortho_near").get<float>();
		if (j.contains("ortho_far"))       c.OrthoFar      = j.at("ortho_far").get<float>();
		if (j.contains("visible_height"))  c.VisibleHeight = j.at("visible_height").get<float>();
		if (j.contains("player_screen_y")) c.PlayerScreenY = j.at("player_screen_y").get<float>();
		if (j.contains("y_bias"))          c.YBias         = j.at("y_bias").get<float>();
		if (j.contains("fov"))             c.FOVDeg        = j.at("fov").get<float>();
		if (j.contains("x_speed"))         c.XSpeed        = j.at("x_speed").get<float>();
		if (j.contains("y_speed"))         c.YSpeed        = j.at("y_speed").get<float>();
		if (j.contains("lead_strength"))   c.LeadStrength  = j.at("lead_strength").get<float>();
		if (j.contains("y_dead_zone"))     c.YDeadZone     = j.at("y_dead_zone").get<float>();
		if (j.contains("camera_x"))        c.CameraX       = j.at("camera_x").get<float>();
		if (j.contains("camera_z"))        c.CameraZ       = j.at("camera_z").get<float>();
	}

	static void from_json(const json& j, SceneConfig& c)
	{
		if (j.contains("mesh_path")) j.at("mesh_path").get_to(c.MeshPath);
		if (j.contains("position"))  j.at("position").get_to(c.Position);
		if (j.contains("rotation"))  j.at("rotation").get_to(c.Rotation);
		if (j.contains("scale"))     j.at("scale").get_to(c.Scale);
	}

	static void from_json(const json& j, LightConfig& c)
	{
		if (j.contains("position"))    j.at("position").get_to(c.Position);
		if (j.contains("color"))       j.at("color").get_to(c.Color);
		if (j.contains("direction"))   j.at("direction").get_to(c.Direction);
		if (j.contains("intensity"))   c.Intensity  = j.at("intensity").get<float>();
		if (j.contains("radius"))      c.Radius     = j.at("radius").get<float>();
		if (j.contains("inner_angle")) c.InnerAngle = j.at("inner_angle").get<float>();
		if (j.contains("outer_angle")) c.OuterAngle = j.at("outer_angle").get<float>();
		if (j.contains("type"))        c.Type       = j.at("type").get<uint32_t>();
	}

	static void from_json(const json& j, WorldConfig& c)
	{
		j.at("player").get_to(c.Player);
		j.at("physics").get_to(c.Physics);
		j.at("camera").get_to(c.Camera);
		if (j.contains("scene")) j.at("scene").get_to(c.Scene);

		// Support both old "point_lights" key and new "lights" key
		const char* key = j.contains("lights") ? "lights" : "point_lights";
		if (j.contains(key))
		{
			c.Lights.clear();
			for (const auto& pl : j.at(key))
			{
				LightConfig cfg;
				pl.get_to(cfg);
				c.Lights.push_back(cfg);
			}
		}
	}

}

static std::string ResolvePath(const std::string& path)
{
#ifdef HMN_SOURCE_RESOURCES_PATH
	// In Debug, always read/write from source so saves persist across rebuilds.
	auto it = path.find("Resources/");
	if (it != std::string::npos)
		return std::string(HMN_SOURCE_RESOURCES_PATH) + "/" + path.substr(it + 10);
#endif
	return path;
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

		root["player"] = {
			{ "movement", { { "speed", cfg.Player.Movement.Speed }, { "mass", cfg.Player.Movement.Mass } } },
			{ "spawn",    { { "position", vec3_to_json(cfg.Player.Spawn.Position) }, { "scale", vec3_to_json(cfg.Player.Spawn.Scale) } } },
			{ "collider", {
				{ "extents",          vec2_to_json(cfg.Player.Collider.Extents) },
				{ "offset",           vec2_to_json(cfg.Player.Collider.Offset)  },
				{ "static_friction",  cfg.Player.Collider.StaticFriction        },
				{ "dynamic_friction", cfg.Player.Collider.DynamicFriction       }
			}},
			{ "scale",  vec3_to_json(cfg.Player.Scale) },
			{ "rest_y", cfg.Player.RestY }
		};

		root["physics"] = {
			{ "gravity", vec3_to_json(cfg.Physics.Gravity) },
			{ "floor",   {
				{ "position",         vec3_to_json(cfg.Physics.Floor.Position) },
				{ "scale",            vec3_to_json(cfg.Physics.Floor.Scale)    },
				{ "static_friction",  cfg.Physics.Floor.StaticFriction         },
				{ "dynamic_friction", cfg.Physics.Floor.DynamicFriction        }
			}}
		};

		root["camera"] = {
			{ "smoothing",       cfg.Camera.Smoothing     },
			{ "visible_height",  cfg.Camera.VisibleHeight },
			{ "player_screen_y", cfg.Camera.PlayerScreenY },
			{ "y_bias",          cfg.Camera.YBias         },
			{ "fov",             cfg.Camera.FOVDeg        },
			{ "x_speed",         cfg.Camera.XSpeed        },
			{ "y_speed",         cfg.Camera.YSpeed        },
			{ "lead_strength",   cfg.Camera.LeadStrength  },
			{ "y_dead_zone",     cfg.Camera.YDeadZone     },
			{ "camera_x",        cfg.Camera.CameraX       },
			{ "camera_z",        cfg.Camera.CameraZ       }
		};

		root["scene"] = {
			{ "mesh_path", cfg.Scene.MeshPath },
			{ "position",  vec3_to_json(cfg.Scene.Position) },
			{ "rotation",  vec3_to_json(cfg.Scene.Rotation) },
			{ "scale",     vec3_to_json(cfg.Scene.Scale)    }
		};

		json lightsArr = json::array();
		for (const auto& l : cfg.Lights)
		{
			lightsArr.push_back({
				{ "position",    vec3_to_json(l.Position)  },
				{ "color",       vec3_to_json(l.Color)     },
				{ "direction",   vec3_to_json(l.Direction) },
				{ "intensity",   l.Intensity  },
				{ "radius",      l.Radius     },
				{ "inner_angle", l.InnerAngle },
				{ "outer_angle", l.OuterAngle },
				{ "type",        l.Type       }
			});
		}
		root["lights"] = lightsArr;

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
