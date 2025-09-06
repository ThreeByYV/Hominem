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
			//m_DripTexture = Texture2D::Create("src/Hominem/Resources/Textures/drip.jpg");
			m_ActiveScene = CreateRef<Scene>();

			// Entity 
			auto square = m_ActiveScene->CreateEntity("Green Square");
			square.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f });

			m_SquareEntity = square;

			m_CameraEntity = m_ActiveScene->CreateEntity("Camera Entity");
			m_CameraEntity.AddComponent<CameraComponent>();
	

			m_SecondCamera = m_ActiveScene->CreateEntity("Clip-Space Entity");
			auto& cameraComponent = m_SecondCamera.AddComponent<CameraComponent>();
			
			cameraComponent.Primary = false;
		}

		void OnUpdate(Timestep ts) override
		{	
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnUpdate(ts);	

			// Render
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();
			
			// Update Scene
			m_ActiveScene->OnUpdate(ts);

			Renderer2D::EndScene();	
		} 

		void OnImGuiRender() override
		{	
			ImGui::Begin("Settings");

			if (m_SquareEntity)
			{
				auto& squareColor = m_SquareEntity.GetComponent<SpriteRendererComponent>().Color;
				ImGui::ColorEdit3("Square Color", glm::value_ptr(squareColor));
			}

			ImGui::DragFloat3("Camera A Transform", 
				glm::value_ptr(m_CameraEntity.GetComponent<TransformComponent>().Transform[3]));

			if (ImGui::Checkbox("Camera A", &m_PrimaryCamera))
			{
				m_CameraEntity.GetComponent<CameraComponent>().Primary = m_PrimaryCamera;
				m_SecondCamera.GetComponent<CameraComponent>().Primary = !m_PrimaryCamera;
			}

			{
				auto& camera = m_SecondCamera.GetComponent<CameraComponent>().Camera;
				float orthoSize = camera.GetOrthographicSize();
			
				if (ImGui::DragFloat("Second Camera Ortho Size", &orthoSize))
				{
					camera.SetOrthographicSize(orthoSize);
				};

			}

			ImGui::End();
		}

		void OnEvent(Event& e) override
		{
			m_CameraController.OnEvent(e);
		}

	private:
		OrthographicCameraController m_CameraController;
		Ref<Texture2D> m_DripTexture;
		Ref<Scene> m_ActiveScene;
		Entity m_SquareEntity;
		Entity m_CameraEntity;
		Entity m_SecondCamera;
		glm::vec2 m_ViewportSize;
		bool m_PrimaryCamera = true;
	};

}