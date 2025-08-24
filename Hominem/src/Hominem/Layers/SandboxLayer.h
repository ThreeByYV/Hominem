#pragma once

#include "Hominem/Core/Hominem.h"
#include "Platform/OpenGL/OpenGLShader.h"

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
			m_VertexArray.reset(Hominem::VertexArray::Create());

			float vertices[4 * 5] = {
				-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // 0: Bottom-left
				 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // 1: Bottom-right  
				 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,  // 2: Top-right
				-0.5f,  0.5f, 0.0f, 0.0f, 1.0f   // 3: Top-left
			};

			m_VertexBuffer.reset(Hominem::VertexBuffer::Create(vertices, sizeof(vertices)));

			BufferLayout layout = {
				{ ShaderDataType::Float3, "a_Pos" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};

			m_VertexBuffer->SetLayout(layout);
			m_VertexArray->AddVertexBuffer(m_VertexBuffer); //todo may need to leverage Unbind more, since there could be bad associations to the bind VAO


			unsigned int indices[6] = {
				0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
				2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
			};

			m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
			m_VertexArray->SetIndexBuffer(m_IndexBuffer);

			auto squareShader = m_ShaderLibrary.Load("src/Hominem/Resources/Shaders/frag.glsl");
			auto textureShader = m_ShaderLibrary.Load("src/Hominem/Resources/Shaders/texture.glsl");
			
			m_Texture = Texture2D::Create("src/Hominem/Resources/Textures/drip.jpg");
			std::dynamic_pointer_cast<OpenGLShader>(textureShader)->Bind();
			std::dynamic_pointer_cast<OpenGLShader>(textureShader)->UploadUniformInt("u_Texture", 0); //the bound texture slot is 0
		}

		void OnUpdate(Timestep ts) override
		{	
			// Update
			m_CameraController.OnUpdate(ts);

			// Render
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();

			Renderer::BeginScene(m_CameraController.GetCamera());

			glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

			auto squareShader = m_ShaderLibrary.Get("frag");
			std::dynamic_pointer_cast<OpenGLShader>(squareShader)->Bind();
			std::dynamic_pointer_cast<OpenGLShader>(squareShader)->UploadUniformFloat3("u_Color", m_SquareColor);

			for (int y = 0; y < 20; y++)
			{
				for (int x = 0; x < 20; x++)
				{
					glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;	
				    Renderer::Submit(m_VertexArray, squareShader, transform);
				}
			}
			
			auto textureShader = m_ShaderLibrary.Get("texture");
			m_Texture->Bind();
			Renderer::Submit(m_VertexArray, textureShader, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
			
			Renderer::EndScene();	
		} 

		void OnImGuiRender() override
		{
			ImGui::Begin("Settings");
			ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SquareColor));
			ImGui::End();
		}

		void OnEvent(Event& e) override
		{
			m_CameraController.OnEvent(e);
		}

	private:
		ShaderLibrary m_ShaderLibrary;
		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		Ref<Texture2D> m_Texture;
		OrthographicCameraController m_CameraController;

		glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
	};

	//tell opengl the rendering area when the window has resized, using GLViewport
}