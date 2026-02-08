#include "hmnpch.h"
#include "JSON.h"
#include "GameConfig.h"
#include "Hominem/Renderer/CinematicCameraController.h"
#include <fstream>
#include <glm/gtc/quaternion.hpp>

using json = nlohmann::json;

namespace Hominem::JSON {

	bool LoadFromFile(json& outJson, const std::string& filepath)
	{
		try
		{
			std::ifstream file(filepath);
			if (!file.is_open())
			{
				HMN_CORE_ERROR("Failed to open JSON file: {}", filepath);
				return false;
			}

			file >> outJson;
			return true;
		}
		catch (const json::parse_error& e)
		{
			HMN_CORE_ERROR("JSON parse error in {}: {}", filepath, e.what());
			return false;
		}
		catch (const std::exception& e)
		{
			HMN_CORE_ERROR("Failed to load JSON from {}: {}", filepath, e.what());
			return false;
		}
	}

	void SaveToFile(const json& json, const std::string& filepath)
	{
		try
		{
			std::ofstream file(filepath);
			if (!file.is_open())
			{
				HMN_CORE_ERROR("Failed to create JSON file: {}", filepath);
				return;
			}

			file << json.dump(4); // Pretty print with 4-space indent
			HMN_CORE_INFO("Saved JSON to: {}", filepath);
		}
		catch (const std::exception& e)
		{
			HMN_CORE_ERROR("Failed to save JSON to {}: {}", filepath, e.what());
		}
	}

}

namespace nlohmann {

	void to_json(json& j, const glm::vec3& v)
	{
		j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
	}

	void from_json(const json& j, glm::vec3& v)
	{
		j.at("x").get_to(v.x);
		j.at("y").get_to(v.y);
		j.at("z").get_to(v.z);
	}

	void to_json(json& j, const glm::vec2& v)
	{
		j = json{ {"x", v.x}, {"y", v.y} };
	}

	void from_json(const json& j, glm::vec2& v)
	{
		j.at("x").get_to(v.x);
		j.at("y").get_to(v.y);
	}

	void to_json(json& j, const glm::mat4& m)
	{
		for (int i = 0; i < 4; i++)
			for (int k = 0; k < 4; k++)
				j[i][k] = m[i][k];
	}

	void from_json(const json& j, glm::mat4& m)
	{
		for (int i = 0; i < 4; i++)
			for (int k = 0; k < 4; k++)
				m[i][k] = j[i][k];
	}

	void to_json(json& j, const glm::quat& q)
	{
		j = json{ {"w", q.w}, {"x", q.x}, {"y", q.y}, {"z", q.z} };
	}

	void from_json(const json& j, glm::quat& q)
	{
		j.at("w").get_to(q.w);
		j.at("x").get_to(q.x);
		j.at("y").get_to(q.y);
		j.at("z").get_to(q.z);
	}

	// Camera data serialization - defined here after GLM serialization
	void to_json(json& j, const Hominem::CameraPoint& p)
	{
		j = json{
			{"PlayerX", p.PlayerX},
			{"CameraOffset", p.CameraOffset},
			{"CameraZoom", p.CameraZoom}
		};
	}

	void from_json(const json& j, Hominem::CameraPoint& p)
	{
		j.at("PlayerX").get_to(p.PlayerX);
		j.at("CameraOffset").get_to(p.CameraOffset);
		j.at("CameraZoom").get_to(p.CameraZoom);
	}

	void to_json(json& j, const Hominem::CinematicSequence& s)
	{
		j = json{
			{"Name", s.Name},
			{"StartX", s.StartX},
			{"EndX", s.EndX},
			{"Duration", s.Duration},
			{"AutoMovePlayer", s.AutoMovePlayer},
			{"PlayerSpeed", s.PlayerSpeed}
		};
	}

	void from_json(const json& j, Hominem::CinematicSequence& s)
	{
		j.at("Name").get_to(s.Name);
		j.at("StartX").get_to(s.StartX);
		j.at("EndX").get_to(s.EndX);
		j.at("Duration").get_to(s.Duration);
		j.at("AutoMovePlayer").get_to(s.AutoMovePlayer);
		j.at("PlayerSpeed").get_to(s.PlayerSpeed);
	}

	void to_json(json& j, const Hominem::CameraSequenceData& d)
	{
		j = json{
			{"Points", d.Points},
			{"Cinematics", d.Cinematics},
			{"Smoothing", d.Smoothing}
		};
	}

	void from_json(const json& j, Hominem::CameraSequenceData& d)
	{
		j.at("Points").get_to(d.Points);
		j.at("Cinematics").get_to(d.Cinematics);
		j.at("Smoothing").get_to(d.Smoothing);
	}

	// Game config serialization
	void from_json(const json& j, Hominem::PlayerMovementConfig& c)
	{
		j.at("speed").get_to(c.Speed);
		j.at("mass").get_to(c.Mass);
	}

	void from_json(const json& j, Hominem::PlayerSpawnConfig& c)
	{
		j.at("position").get_to(c.Position);
		j.at("scale").get_to(c.Scale);
	}

	void from_json(const json& j, Hominem::PlayerColliderConfig& c)
	{
		j.at("extents").get_to(c.Extents);
		j.at("offset").get_to(c.Offset);
		j.at("static_friction").get_to(c.StaticFriction);
		j.at("dynamic_friction").get_to(c.DynamicFriction);
	}

	void from_json(const json& j, Hominem::PlayerConfig& c)
	{
		j.at("movement").get_to(c.Movement);
		j.at("spawn").get_to(c.Spawn);
		j.at("collider").get_to(c.Collider);
	}

	void from_json(const json& j, Hominem::FloorConfig& c)
	{
		j.at("position").get_to(c.Position);
		j.at("scale").get_to(c.Scale);
		j.at("static_friction").get_to(c.StaticFriction);
		j.at("dynamic_friction").get_to(c.DynamicFriction);
	}

	void from_json(const json& j, Hominem::PhysicsConfig& c)
	{
		j.at("gravity").get_to(c.Gravity);
		j.at("floor").get_to(c.Floor);
	}

	void from_json(const json& j, Hominem::CameraControllerConfig& c)
	{
		j.at("controller_speed").get_to(c.ControllerSpeed);
		j.at("smoothing").get_to(c.Smoothing);
		j.at("ortho_size").get_to(c.OrthoSize);
		j.at("ortho_near").get_to(c.OrthoNear);
		j.at("ortho_far").get_to(c.OrthoFar);
	}

	void from_json(const json& j, Hominem::GameConfig& c)
	{
		j.at("player").get_to(c.Player);
		j.at("physics").get_to(c.Physics);
		j.at("camera").get_to(c.Camera);
	}

}
