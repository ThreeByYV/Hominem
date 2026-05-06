#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Scene/Scene.h"

/// Base class for all game levels / play modes.
class Level
{
public:
	virtual ~Level() = default;

	virtual void OnEnter(Hominem::Scene& scene) = 0;
	virtual void OnUpdate(Hominem::Timestep ts)  = 0;
	virtual void OnExit()                        = 0;

	virtual void OnImGuiRender()         {}
	virtual void OnEvent(Hominem::Event& e) {}
};
