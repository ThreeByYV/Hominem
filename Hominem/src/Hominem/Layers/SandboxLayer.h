#pragma once

#include "Hominem/Core/Hominem.h"

#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


namespace Hominem {

	class SandboxLayer : public Layer
	{
	public:
		SandboxLayer()
			: Layer("Sandbox"), m_CameraController(800.0f / 600.0f)
		{
		}

		void OnAttach()
		{
			m_DripTexture = Texture2D::Create("src/Hominem/Resources/Textures/drip.jpg");
		}

		void OnUpdate(Timestep ts) override
		{	
			m_CameraController.OnUpdate(ts);

			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();

			Renderer2D::BeginScene(m_CameraController.GetCamera());
			
			Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.7f, 1.0f }, { 0.8f, 0.2f, 0.3f, 1.0f });
			Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.4f, 0.96f }, glm::vec4(m_SquareColor, 1.0f));
			Renderer2D::DrawQuad({ 0.0f, 0.0f -0.1f }, { 10.0f, 10.0f }, m_DripTexture);


			Renderer2D::EndScene();	

		} 

		void OnImGuiRender() override
		{
			ImGui::Begin("Settings");
			ImGui::ColorEdit3("Second Square Color", glm::value_ptr(m_SquareColor));
			ImGui::End();
		}

		void OnEvent(Event& e) override
		{
			m_CameraController.OnEvent(e);
		}

	private:
		OrthographicCameraController m_CameraController;
		Ref<Texture2D> m_DripTexture;
		glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
	};

}