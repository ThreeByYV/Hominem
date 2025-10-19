#pragma once

#include "Hominem/Core/Hominem.h"
#include "Hominem/Layers/MenuLayer.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace Hominem {

	class GameLayer : public Layer
	{
	public:
		GameLayer()
			: Layer("Game"), m_CameraController(800.0f / 600.0f)
		{
			HMN_CORE_INFO("Created new GameLayer!");
		}

		void OnAttach() override
		{
			m_BackgroundTexture = Texture2D::Create("src/Hominem/Resources/Textures/gamebg.png");
			m_Mujun = Texture2D::Create("src/Hominem/Resources/Textures/mujun.png");
			m_SpriteSheet = Texture2D::Create("src/Hominem/Resources/Textures/spritesheet.png");

		}

		void OnDetach() override
		{
	
		}

		void OnUpdate(Timestep ts) override
		{
			if (Input::IsKeyPressed(HMN_KEY_1))
			{
				TransitionTo<MenuLayer>();
				return;
			}

			m_CameraController.OnUpdate(ts);

			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
			RenderCommand::Clear();

			Renderer2D::BeginScene(m_CameraController.GetCamera());

			Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.5f }, { 2.7f, 2.0f }, m_BackgroundTexture);
			Renderer2D::DrawQuad({ 0.0f, -0.5f, 0.0f }, { 0.6f, 0.6f }, m_Mujun);
			Renderer2D::DrawQuad({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, m_SpriteSheet);


			Renderer2D::EndScene();
		}

		void OnImGuiRender() override
		{
			
		}

		void OnEvent(Event& e) override
		{
			m_CameraController.OnEvent(e);
		}

	private:
		OrthographicCameraController m_CameraController;
		Ref<Texture2D> m_BackgroundTexture;
		Ref<Texture2D> m_SpriteSheet;
		Ref<Texture2D> m_Mujun;
	};
}