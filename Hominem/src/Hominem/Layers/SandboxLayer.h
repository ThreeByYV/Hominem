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
			Renderer3D::Init();
			m_Mesh = CreateRef<SkinnedMesh>();

			if (!m_Mesh->LoadFromFile("src/Hominem/Resources/Textures/test.fbx"))
			{
				HMN_CORE_ERROR("Failed to load mesh");
				return;
			}

			HMN_CORE_INFO("=== MESH DEBUG ===");
			HMN_CORE_INFO("VAO: {}", m_Mesh->GetVAO());
			HMN_CORE_INFO("Bone Count: {}", m_Mesh->GetBoneCount());

			auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
			if (!skinningShader)
			{
				HMN_CORE_ERROR("Skinning shader not found!");
				return;
			}

			m_Mesh->InitBoneUniforms(skinningShader);
			m_StartTimeMillis = GetCurrentTimeMillis();
		}

		void OnDetach() override
		{
		}

		void OnUpdate(Timestep ts) override
		{
			// Handle input
			if (Input::IsKeyPressed(HMN_KEY_R))
			{
				Renderer2D::GetShaderLibrary()->ReloadAll();
				Renderer3D::GetShaderLibrary()->ReloadAll();

				// Re-initialize bone uniforms after shader reload
				auto skinningShader = Renderer3D::GetShaderLibrary()->Get("skinning");
				m_Mesh->InitBoneUniforms(skinningShader);
			}

			if (Input::IsKeyPressed(HMN_KEY_1))
			{
				Renderer3D::EndScene();
				TransitionTo<MenuLayer>();
				return;
			}

			// Update camera
			m_CameraController.OnUpdate(ts);

			// Clear screen
			RenderCommand::Clear();

			// Calculate bone transforms
			long long currentTimeMillis = GetCurrentTimeMillis();
			float animationTimeSec = ((float)(currentTimeMillis - m_StartTimeMillis)) / 1000.0f;
			std::vector<glm::mat4> boneTransforms;
			m_Mesh->GetBoneTransforms(animationTimeSec, boneTransforms);

			// Begin 3D rendering
			Renderer3D::BeginScene(
				m_CameraController.GetCamera().GetViewProjectionMatrix(),
				m_CameraController.GetPosition()
			);

			// Setup mesh renderer component
			MeshRendererComponent rc{};
			rc.Mesh = m_Mesh;
			rc.Shader = Renderer3D::GetShaderLibrary()->Get("skinning");

			// Bind shader and set uniforms
			rc.Shader->Bind();

			// Upload bone transforms
			for (uint32_t i = 0; i < boneTransforms.size(); i++)
			{
				m_Mesh->UploadBoneTransform(i, boneTransforms[i]);
			}

			// Set shader uniforms
			rc.Shader->SetInt("gDisplayBoneIndex", -1);
			rc.Shader->SetFloat4("u_Color", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			rc.Shader->SetInt("u_Texture", 0);
			rc.Shader->SetFloat("gFogStart", 10.0f);
			rc.Shader->SetFloat("gFogEnd", 50.0f);
			rc.Shader->SetFloat3("gFogColor", glm::vec3(0.7f, 0.75f, 0.8f));

			// Model transform
			glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));

			// Draw the mesh
			Renderer3D::Draw(rc, modelMatrix);

			// End 3D rendering
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
		static long long GetCurrentTimeMillis()
		{
			const auto now = std::chrono::system_clock::now();
			return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		}

	private:
		PerspectiveCameraController m_CameraController;
		Ref<SkinnedMesh> m_Mesh = nullptr;
		long long m_StartTimeMillis = 0;
	};
}
