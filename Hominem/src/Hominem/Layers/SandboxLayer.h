#pragma once

#include "Hominem/Core/Hominem.h"

#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <future>


namespace Hominem {

	static std::mutex s_MeshMutex;


	class SandboxLayer : public Layer
	{
	public:
		SandboxLayer() 
			: Layer("Sandbox"), m_CameraController(800.0f / 600.0f)
		{
		}


		static void RenderMesh(BasicMesh* mesh, OrthographicCameraController cameraController)
		{
			std::lock_guard<std::mutex> lock(s_MeshMutex);
		
			auto meshShader = Renderer2D::GetShaderLibrary()->Get("texture");
			meshShader->Bind();
			meshShader->SetMat4("u_ViewProjection", cameraController.GetCamera().GetViewProjectionMatrix());
			meshShader->SetMat4("u_Transform", glm::mat4(1.0f)); // Identity matrix
			meshShader->SetFloat4("u_Color", glm::vec4(1.0f)); // White
			meshShader->SetInt("u_Texture", 0);

			mesh->Render();
		}
		
		static void LoadMesh(BasicMesh* mesh)
		{
			if (!mesh->LoadMesh("src/Hominem/Resources/Textures/char.fbx"))
			{
				return;
			}
		}

		void OnAttach()
		{
			//m_DripTexture = Texture2D::Create("src/Hominem/Resources/Textures/drip.jpg");
			
			m_Mesh = new BasicMesh();

#define ASYNC 0
#if ASYNC 
			m_Futures.push_back(std::async(std::launch::async, LoadMesh, m_Mesh));
#else
			if (!m_Mesh->LoadMesh("src/Hominem/Resources/Textures/char.fbx"))
			{
				return;
			}
#endif

		}

		void OnUpdate(Timestep ts) override
		{	
			m_CameraController.OnUpdate(ts);

			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
			RenderCommand::Clear();

			Renderer2D::BeginScene(m_CameraController.GetCamera());

			Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.7f, 1.0f }, { 0.8f, 0.2f, 0.3f, 1.0f });
			Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.4f, 0.96f }, glm::vec4(m_SquareColor, 1.0f));
			//Renderer2D::DrawQuad({ 0.0f, 0.0f - 0.1f }, { 10.0f, 10.0f }, m_DripTexture);

			Renderer2D::EndScene();
#if ASYNC
			m_Futures.push_back(std::async(std::launch::async, RenderMesh, m_Mesh, m_CameraController));
#else
			auto meshShader = Renderer2D::GetShaderLibrary()->Get("texture");
			
			meshShader->Bind();
			//meshShader->SetMat4("u_ViewProjection", m_CameraController.GetCamera().GetViewProjectionMatrix());
			meshShader->SetMat4("u_Transform", glm::mat4(1.0f)); // Identity matrix
			meshShader->SetFloat4("u_Color", glm::vec4(1.0f)); // White
			meshShader->SetInt("u_Texture", 0);
			
			m_Mesh->Render();
#endif
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
		PerspectiveCameraController m_CameraController;
		
		BasicMesh* m_Mesh = nullptr;
		//Ref<Texture2D> m_DripTexture;
		glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
		std::vector<std::future<void>> m_Futures;
	};

}