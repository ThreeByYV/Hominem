#include "hmnpch.h"
#include <glad/glad.h>

#include "MenuLayer.h"
#include "SandboxLayer.h"
#include "Hominem/Renderer/Font.h"

namespace Hominem {

	static Ref<Font> s_Font;

	MenuLayer::MenuLayer()
		: Layer("MenuLayer")
	{
		HMN_CORE_INFO("Created new MenuLayer!");
	}

	void MenuLayer::OnAttach()
	{
		//s_Font = Font::GetDefaultFont();
	}

	void MenuLayer::OnUpdate(Timestep ts)
	{

		if (Input::IsKeyPressed(HMN_KEY_2))
		{
			TransitionTo<SandboxLayer>();
			return;
		}

		Renderer2D::DrawString("Hominem", Font::GetDefaultFont(), glm::mat4(1.0f), glm::vec4(1.0f));
	}

	void MenuLayer::OnImGuiRender()
	{
		
	}
}