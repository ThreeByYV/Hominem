#pragma once

#include "Hominem/Scene/Scene.h"
#include "Hominem/Scene/Entity.h"
#include "Hominem/Core/Timestep.h"
#include "Game/GameConfig.h"

#include <glm/glm.hpp>

namespace Hominem {

	class Player
	{
	public:
		Player(Scene& scene, const PlayerConfig& config);
		~Player() = default;

		void OnUpdate(Timestep ts);
		void OnImGuiRender();
		void ReloadShader();

		Entity GetEntity() const { return m_Entity; }
		glm::vec3 GetPosition() const;

	private:
		Scene& m_Scene;
		const PlayerConfig& m_Config;
		Entity m_Entity;

		// Transform state (position comes from physics, scale/rotation managed here)
		glm::vec3 m_Scale;
		glm::vec3 m_Rotation;

		enum class State { Idle = 0, Running = 1 };
		State m_State = State::Idle;
	};

}
