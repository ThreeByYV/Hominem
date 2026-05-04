#pragma once

#include "Hominem/Core/Timestep.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Scene/Scene.h"

namespace Hominem {

	class GameMode
	{
	public:
		virtual ~GameMode() = default;

		virtual void OnEnter(Scene& scene) = 0;
		virtual void OnUpdate(Timestep ts) = 0;
		virtual void OnExit() = 0;

		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& e) {}
	};

}
