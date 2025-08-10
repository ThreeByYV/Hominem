#include "hmnpch.h"
#include "Application.h"

#include <glad/glad.h>
#include "Hominem/Renderer/Buffer.h"
#include "Input.h"
#include "glm/glm.hpp"

namespace Hominem {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)
	 
	Application* Application::s_Instance = nullptr;

	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:     return GL_FLOAT;
			case ShaderDataType::Float2:    return GL_FLOAT;
			case ShaderDataType::Float3:    return GL_FLOAT;
			case ShaderDataType::Float4:    return GL_FLOAT;
			case ShaderDataType::Mat3:      return GL_FLOAT;
			case ShaderDataType::Mat4:      return GL_FLOAT;
			case ShaderDataType::Int:       return GL_INT;
			case ShaderDataType::Int2:      return GL_INT;
			case ShaderDataType::Int3:      return GL_INT;
			case ShaderDataType::Int4:      return GL_INT;
			case ShaderDataType::Bool:      return GL_BOOL;
		}

		HMN_CORE_ASSERT(false, "Unknown ShaderDataType on convert to OpenGL Base!");
		return 0;
	}

	Application::Application()
	{
		HMN_CORE_ASSERT(!s_Instance, "Application already exists!");
		
		s_Instance = this;
		m_Window = std::unique_ptr<Window>(Window::Create()); 	//we don't have to manually delete the window when the application terminates
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

		m_ImGuiLayer = new ImGuiLayer();


		PushOverlay(m_ImGuiLayer);


		float vertices[4 * 3] = {
			-0.5f, -0.5f, 0.0f,  // 0: Bottom-left
			 0.5f, -0.5f, 0.0f,  // 1: Bottom-right  
			 0.5f,  0.5f, 0.0f,  // 2: Top-right
			-0.5f,  0.5f, 0.0f   // 3: Top-left
		};

		m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Pos" }
		};

		m_VertexBuffer->SetLayout(layout);

		uint32_t index = 0;
		for (const auto& element : m_VertexBuffer->GetLayout())
		{
			glEnableVertexAttribArray(index);

			glVertexAttribPointer(index,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type), 
				element.Normalized ? GL_TRUE : GL_FALSE, 
				m_VertexBuffer->GetLayout().GetStride(),
				(const void*)element.Offset);

			index++;
		}

		
		unsigned int indices[6] = {
			0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
			2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
		};

		m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
	}

	//this function leverages the EventDispatcher to pass an event to the correct method
	void Application::OnEvent(Event& e)
	{
		HMN_CORE_TRACE("Event received: {}", e.ToString());
		
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));


		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled) break;
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{
			glClearColor(0.1f, 0.1f, 0.1f, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindVertexArray(m_VertexArray);
			glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);

			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate();
			}

			m_ImGuiLayer->Begin();

			for (Layer* layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}

			m_ImGuiLayer->End();

			auto [x, y] = Input::GetMousePosition();
			//HMN_CORE_TRACE("{0}, {1}", x, y);
				
			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	};

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}


	Application::~Application()
	{
	}

}

