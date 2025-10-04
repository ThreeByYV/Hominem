#pragma once

#include "Hominem/Core/Hominem.h"
#include "Hominem/Layers/MenuLayer.h"
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
			HMN_CORE_INFO("Created new SandboxLayer!");
		}

		void OnAttach() override
		{
			FramebufferSpecification fbSpec;
			fbSpec.Width = 800;
			fbSpec.Height = 600;
			m_Framebuffer = Framebuffer::Create(fbSpec);

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

			m_Framebuffer->Bind(); //render the scene to the framebuffer

			// Render
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		
			RenderCommand::Clear();
			
			// Update Scene
			m_ActiveScene->OnUpdate(ts);

			Renderer2D::EndScene();	
			m_Framebuffer->Unbind();
		} 

		void OnImGuiRender() override
		{	
			ImGui::Begin("Settings");

			/*	if (m_SquareEntity)
				{
					auto& squareColor = m_SquareEntity.GetComponent<SpriteRendererComponent>().Color;
				}

				ImGui::DragFloat3("Camera A Transform", 
					glm::value_ptr(m_CameraEntity.GetComponent<TransformComponent>().Transform[3]));

				if (ImGui::Checkbox("Camera A", &m_PrimaryCamera))
		{
			m_CameraController.OnEvent(e);
			*/
		}

	private:
		PerspectiveCameraController m_CameraController;
		BasicMesh* m_Mesh = nullptr;
		Ref<Texture2D> m_DripTexture;
		Ref<Scene> m_ActiveScene;
		Entity m_SquareEntity;
		Entity m_CameraEntity;
		Entity m_SecondCamera;
		Ref<Framebuffer> m_Framebuffer;
		glm::vec2 m_ViewportSize;
		bool m_PrimaryCamera = true;
	};
	

}