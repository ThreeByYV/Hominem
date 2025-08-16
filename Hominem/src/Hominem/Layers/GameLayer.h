#pragma once

#include "Hominem/Core/Hominem.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Hominem {

	class GameLayer : public Layer
	{
	public:
		GameLayer()			        
			: Layer("MainGame"),
			  m_Camera(-2.0f, 2.0f, -2.0f, 2.0f), //   -x     +x    -y    +y
			  m_CameraPosition(0.0f),
		      m_SquarePosition(0.0f)
		{
			m_VertexArray.reset(Hominem::VertexArray::Create());

			float vertices[4 * 3] = {
				-0.5f, -0.5f, 0.0f,  // 0: Bottom-left
				 0.5f, -0.5f, 0.0f,  // 1: Bottom-right  
				 0.5f,  0.5f, 0.0f,  // 2: Top-right
				-0.5f,  0.5f, 0.0f   // 3: Top-left
			};

			m_VertexBuffer.reset(Hominem::VertexBuffer::Create(vertices, sizeof(vertices)));

			BufferLayout layout = {
				{ ShaderDataType::Float3, "a_Pos" }
			};

			m_VertexBuffer->SetLayout(layout);
			m_VertexArray->AddVertexBuffer(m_VertexBuffer); //todo may need to leverage Unbind more, since there could be bad associations to the bind VAO


			unsigned int indices[6] = {
				0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
				2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
			};

			m_IndexBuffer.reset(Hominem::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
			m_VertexArray->SetIndexBuffer(m_IndexBuffer);

			std::string vertexSrc = R"(
			#version 330 core
		
			layout(location = 0) in vec3 a_Pos;
			uniform mat4 u_ViewProjection;	
			uniform mat4 u_Transform;
    
			void main()
			{
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Pos, 1.0);
			}
			)"; 

			std::string fragmentSrc = R"(
			#version 330 core
		
			layout(location = 0) out vec4 color;
			uniform vec3 u_Color;
    
			void main()
			{
				color = vec4(u_Color, 1.0f);
			}
			)";

			m_Shader.reset(Hominem::Shader::Create(vertexSrc, fragmentSrc));
		}

		void OnUpdate(Hominem::Timestep ts) override
		{			
			if (Hominem::Input::IsKeyPressed(HMN_KEY_LEFT))
			{
				m_CameraPosition.x -= m_CameraSpeed * ts;
			}

			else if (Hominem::Input::IsKeyPressed(HMN_KEY_RIGHT))
			{
				m_CameraPosition.x += m_CameraSpeed * ts;
			}

		    if (Hominem::Input::IsKeyPressed(HMN_KEY_UP))
			{
				m_CameraPosition.y += m_CameraSpeed * ts;
			}

			else if (Hominem::Input::IsKeyPressed(HMN_KEY_DOWN))
			{
				m_CameraPosition.y -= m_CameraSpeed * ts;
			}

			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();

			m_Camera.SetPosition(m_CameraPosition);
			m_Camera.SetRotation(0.0f);

			Renderer::BeginScene(m_Camera);

			glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));


			std::dynamic_pointer_cast<Hominem::OpenGLShader>(m_Shader)->Bind();
			std::dynamic_pointer_cast<OpenGLShader>(m_Shader)->UploadUniformFloat3("u_Color", m_SquareColor);

			for (int y = 0; y < 20; y++)
			{
				for (int x = 0; x < 20; x++)
				{
					glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
					glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;	
					Hominem::Renderer::Submit(m_VertexArray, m_Shader, transform);
				}
			}
			
			Renderer::EndScene();	
		}

		void OnImGuiRender() override
		{
			ImGui::Begin("Settings");
			ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SquareColor));
			ImGui::End();
		}

		void OnEvent(Hominem::Event& event) override
		{
		
		}

	private:
		std::shared_ptr<VertexArray> m_VertexArray;
		std::shared_ptr<VertexBuffer> m_VertexBuffer;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;
		std::shared_ptr<Shader> m_Shader;
		float m_CameraSpeed = 5.0f;
		float m_CameraRotationSpeed = 180.0f;

		Hominem::OrthographicCamera m_Camera;
		glm::vec3 m_CameraPosition;

		glm::vec3 m_SquarePosition;
		glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };

	};
}