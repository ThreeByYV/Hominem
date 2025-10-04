#include "hmnpch.h"
#include <glad/glad.h>

#include "MenuLayer.h"
#include "SandboxLayer.h"


namespace Hominem {

	MenuLayer::MenuLayer()
		: Layer("MenuLayer")
	{
		HMN_CORE_INFO("Created new MenuLayer!");
	}

	void MenuLayer::OnUpdate(Timestep ts)
	{

		if (Input::IsKeyPressed(HMN_KEY_2))
		{
			TransitionTo<SandboxLayer>();
			return;
		}

		glClearColor(0.6f, 0.1f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void MenuLayer::OnImGuiRender()
	{
		
	}
}