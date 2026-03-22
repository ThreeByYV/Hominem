#pragma once

namespace Hominem {

	struct PhysicsMaterial
	{
		float StaticFriction  = 0.5f;
		float DynamicFriction = 0.5f;
		float Restitution     = 0.0f;

		PhysicsMaterial() = default;
		PhysicsMaterial(float staticFriction, float dynamicFriction, float restitution)
			: StaticFriction(staticFriction), DynamicFriction(dynamicFriction), Restitution(restitution) {}
	};
}
