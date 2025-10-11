#include "hmnpch.h"
#include <glad/glad.h>

#include "MenuLayer.h"
#include "SandboxLayer.h"
#include "Hominem/Renderer/Font.h"

namespace Hominem {

	static Ref<Font> s_Font;

	MenuLayer::MenuLayer()
		: Layer("MenuLayer"), m_CameraController(800.0f / 600.0f)
	{
		HMN_CORE_INFO("Created new MenuLayer!");
	}

	void MenuLayer::OnAttach()
	{
		s_Font = Font::GetDefaultFont();
		m_BackgroundTexture = Texture2D::Create("src/Hominem/Resources/Textures/menu2.png");

	}

	void MenuLayer::OnUpdate(Timestep ts)
	{
		m_CameraController.OnUpdate(ts);

		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f }); 
		RenderCommand::Clear();

		Renderer2D::BeginScene(m_CameraController.GetCamera());

		if (Input::IsKeyPressed(HMN_KEY_2))
		{
			TransitionTo<SandboxLayer>();
			return;
		}

		Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.5f }, { 2.7f, 2.0f }, m_BackgroundTexture);

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, 0.0f, 0.0f))  // Move left
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));

		Renderer2D::DrawString("Hominem", Font::GetDefaultFont(), transform, glm::vec4(1.0f));
	

	}

	void MenuLayer::OnImGuiRender()
	{
		
	}
}