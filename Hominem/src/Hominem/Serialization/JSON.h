#pragma once

#include "Hominem/Core/Core.h"
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <string>

namespace Hominem::JSON {

	/// Load JSON from file
	bool LoadFromFile(nlohmann::json& outJson, const std::string& filepath);

	/// Save JSON to file (pretty-printed)
	void SaveToFile(const nlohmann::json& json, const std::string& filepath);

	/// Load and deserialize any type from JSON file
	/// Requires T to have from_json defined in nlohmann namespace
	template<typename T>
	bool Load(const std::string& filepath, T& outData)
	{
		try
		{
			nlohmann::json j;
			if (!LoadFromFile(j, filepath))
				return false;

			outData = j.get<T>();
			return true;
		}
		catch (const nlohmann::json::exception& e)
		{
			HMN_CORE_ERROR("JSON deserialization error: {}", e.what());
			return false;
		}
	}

	/// Serialize and save any type to JSON file
	/// Requires T to have to_json defined in nlohmann namespace
	template<typename T>
	void Save(const std::string& filepath, const T& data)
	{
		nlohmann::json j = data;
		SaveToFile(j, filepath);
	}

	/// Load nested value from JSON by path (e.g., "camera_sequences.level_01_vista")
	template<typename T>
	bool LoadNested(const std::string& filepath, const std::string& jsonPath, T& outData)
	{
		try
		{
			nlohmann::json j;
			if (!LoadFromFile(j, filepath))
				return false;

			// Parse path and navigate
			auto current = j;
			std::stringstream ss(jsonPath);
			std::string token;

			while (std::getline(ss, token, '.'))
			{
				if (!current.contains(token))
				{
					HMN_CORE_ERROR("JSON path '{}' not found", jsonPath);
					return false;
				}
				current = current[token];
			}

			outData = current.get<T>();
			return true;
		}
		catch (const nlohmann::json::exception& e)
		{
			HMN_CORE_ERROR("JSON deserialization error: {}", e.what());
			return false;
		}
	}

}


namespace nlohmann {

	// GLM vec3
	void to_json(nlohmann::json& j, const glm::vec3& v);
	void from_json(const nlohmann::json& j, glm::vec3& v);

	// GLM vec2
	void to_json(nlohmann::json& j, const glm::vec2& v);
	void from_json(const nlohmann::json& j, glm::vec2& v);

	// GLM mat4
	void to_json(nlohmann::json& j, const glm::mat4& m);
	void from_json(const nlohmann::json& j, glm::mat4& m);

	// GLM quat
	void to_json(nlohmann::json& j, const glm::quat& q);
	void from_json(const nlohmann::json& j, glm::quat& q);

}

// Forward declarations for camera data serialization
namespace Hominem {
	struct CameraPoint;
	struct CinematicSequence;
	struct CameraSequenceData;
	struct GameConfig;
	struct PlayerMovementConfig;
	struct PlayerSpawnConfig;
	struct PlayerColliderConfig;
	struct PlayerConfig;
	struct FloorConfig;
	struct PhysicsConfig;
	struct CameraControllerConfig;
}

namespace nlohmann {
	void to_json(nlohmann::json& j, const Hominem::CameraPoint& p);
	void from_json(const nlohmann::json& j, Hominem::CameraPoint& p);
	void to_json(nlohmann::json& j, const Hominem::CinematicSequence& s);
	void from_json(const nlohmann::json& j, Hominem::CinematicSequence& s);
	void to_json(nlohmann::json& j, const Hominem::CameraSequenceData& d);
	void from_json(const nlohmann::json& j, Hominem::CameraSequenceData& d);
	void from_json(const nlohmann::json& j, Hominem::PlayerMovementConfig& c);
	void from_json(const nlohmann::json& j, Hominem::PlayerSpawnConfig& c);
	void from_json(const nlohmann::json& j, Hominem::PlayerColliderConfig& c);
	void from_json(const nlohmann::json& j, Hominem::PlayerConfig& c);
	void from_json(const nlohmann::json& j, Hominem::FloorConfig& c);
	void from_json(const nlohmann::json& j, Hominem::PhysicsConfig& c);
	void from_json(const nlohmann::json& j, Hominem::CameraControllerConfig& c);
	void from_json(const nlohmann::json& j, Hominem::GameConfig& c);
}
