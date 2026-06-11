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

	/// Instantly position the camera at the player with no smoothing lag.
	/// Call after OnEnter to avoid the camera sliding in from the initial config position.
	virtual void SnapCameraToPlayer() {}

	/// Returns the world-space position of the named bone on the player mesh.
	/// Falls back to `fallback` if the player has no mesh or the bone is not found.
	virtual glm::vec3 ResolveEyeTarget(const glm::vec3& fallback) const { return fallback; }

	virtual PlayerDebugInfo GetPlayerDebugInfo() const { return {}; }
};
