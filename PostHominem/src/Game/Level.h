#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Scene/Scene.h"

#include <glm/glm.hpp>

struct PlayerDebugInfo
{
	bool      valid    = false;
	glm::vec3 position = {};
	glm::vec3 velocity = {};
	float     speed    = 0.f;
	bool      isMoving = false;
};

/// Base class for all game levels / play modes.
class Level
{
public:
	virtual ~Level() = default;

	virtual void OnEnter(Hominem::Scene& scene) = 0;
	virtual void OnUpdate(Hominem::Timestep ts)  = 0;
	virtual void OnExit()                        = 0;

	virtual void OnImGuiRender()            {}
	virtual void OnEvent(Hominem::Event& e) {}

	virtual PlayerDebugInfo GetPlayerDebugInfo() const { return {}; }
};
