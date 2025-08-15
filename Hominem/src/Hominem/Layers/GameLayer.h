#pragma once

#include "Hominem/Core/Hominem.h"

namespace Hominem {

	class GameLayer : public Layer
	{
	public:
		GameLayer()			        
			: Layer("MainGame"),
			  m_Camera(-2.0f, 2.0f, -2.0f, 2.0f), //   -x     +x    -y    +y
			  m_CameraPosition(0.0f)
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
    
			void main()
			{
				gl_Position = u_ViewProjection * vec4(a_Pos, 1.0);
			}
			)";

			std::string fragmentSrc = R"(
			#version 330 core
		
			layout(location = 0) out vec4 color;
    
			void main()
			{
				color = vec4(1.0, 0.0, 0.0, 1.0);
			}
			)";

			m_Shader.reset(new Hominem::Shader(vertexSrc, fragmentSrc));
		}

		void OnUpdate(Hominem::Timestep ts) override
		{			
			if (Hominem::Input::IsKeyPressed(HMN_KEY_LEFT))
			{
				m_CameraPosition.x += m_CameraSpeed * ts;
			}

			else if (Hominem::Input::IsKeyPressed(HMN_KEY_RIGHT))
			{
				m_CameraPosition.x -= m_CameraSpeed * ts;
			}

		    if (Hominem::Input::IsKeyPressed(HMN_KEY_UP))
			{
				m_CameraPosition.y -= m_CameraSpeed * ts;
			}

			else if (Hominem::Input::IsKeyPressed(HMN_KEY_DOWN))
			{
				m_CameraPosition.y += m_CameraSpeed * ts;
			}

			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();

			m_Camera.SetPosition(m_CameraPosition);
			m_Camera.SetRotation(0.0f);

			Renderer::BeginScene(m_Camera);

			Renderer::Submit(m_VertexArray, m_Shader);

			Renderer::EndScene();	
		}

		void OnImGuiRender() override
		{

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

	};
}