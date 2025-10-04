#include "hmnpch.h"
#include "VoidLayer.h"
#include <glad/glad.h>
#include "SandboxLayer.h"

namespace Hominem {

	VoidLayer::VoidLayer()
		: Layer("VoidLayer")
	{
		HMN_CORE_INFO("Created new VoidLayer!");
	}

	void VoidLayer::OnUpdate(Timestep ts)
	{

		if (Input::IsKeyPressed(HMN_KEY_3))
		{
			TransitionTo<SandboxLayer>();
			return;
		}
	}

	void VoidLayer::OnImGuiRender()
	{
		glClearColor(0.6f, 0.1f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

}

