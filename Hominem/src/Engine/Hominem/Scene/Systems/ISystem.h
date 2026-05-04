#pragma once

#include "Hominem/Core/Timestep.h"
#include <entt.hpp>

namespace Hominem {

	class ISystem
	{
	public:
		virtual ~ISystem() = default;

		virtual void OnRuntimeStart(entt::registry&) {}
		virtual void OnRuntimeStop(entt::registry&) {}
		virtual void OnUpdate(Timestep ts, entt::registry&) = 0;
	};

}
