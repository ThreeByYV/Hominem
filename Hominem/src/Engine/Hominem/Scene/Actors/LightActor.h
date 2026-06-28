#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/RenderFrame.h"

namespace Hominem {

	/// Emits a point or spot light from its Position each frame.
	class LightActor : public Actor
	{
	public:
		Light light;

		void OnBuildRenderFrame(RenderFrame& frame) override
		{
			light.Position = Position;
			frame.lights.push_back(light);
		}
	};

}
