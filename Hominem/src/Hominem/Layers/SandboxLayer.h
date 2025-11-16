#pragma once

#include "Hominem/Core/Hominem.h"
#include "Hominem/Layers/MenuLayer.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>

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
			Renderer3D::Init();

			m_Mesh = CreateRef<BasicMesh>();

			if (!m_Mesh->LoadMesh("src/Hominem/Resources/Textures/boblampclean.md5mesh"))
			{
				HMN_CORE_ERROR("Failed to load mesh");
			}
		}

		void OnDetach() override
		{
		}

		void OnUpdate(Timestep ts) override
		{
			if (Input::IsKeyPressed(HMN_KEY_R))
			{
				Renderer2D::GetShaderLibrary()->ReloadAll();
				Renderer3D::GetShaderLibrary()->ReloadAll();
			}

			if (Input::IsKeyPressed(HMN_KEY_1))
			{
				Renderer3D::EndScene();
				TransitionTo<MenuLayer>();
				return;
			}

			m_CameraController.OnUpdate(ts);

			RenderCommand::Clear();

			// 3D Pass
			Renderer3D::BeginScene(
				m_CameraController.GetCamera().GetViewProjectionMatrix(), 
				m_CameraController.GetPosition()
			);

			MeshRendererComponent rc{};

			rc.Mesh = m_Mesh;
			rc.Shader = Renderer3D::GetShaderLibrary()->Get("fog");

			rc.Shader->Bind();

			rc.Shader->SetInt("u_Texture", 0);
			rc.Shader->SetFloat("gFogStart", 10.0f);
			
			//todo merge ecs branch and use a real model transform
			glm::mat4 modelMatrix = glm::mat4(1.0f);

			rc.Shader->SetFloat("gFogEnd", 50.0f);
		    rc.Shader->SetFloat3("gFogColor", { 0.7f, 0.75f, 0.8f });

			Renderer3D::Draw(rc, modelMatrix);
			Renderer3D::EndScene();
		}

		void OnImGuiRender() override
		{
		}

		void OnEvent(Event& e) override
		{
			m_CameraController.OnEvent(e);
		}

	private:
		PerspectiveCameraController m_CameraController;
		Ref<BasicMesh> m_Mesh = nullptr;
	};
}