#include "hmnpch.h"
#include "Application.h"

#include "Hominem/Renderer/Buffer.h"
#include "Input.h"
#include "glm/glm.hpp"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer.h"


namespace Hominem {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)
	 
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		HMN_CORE_ASSERT(!s_Instance, "Application already exists!");
		
		s_Instance = this;
		m_Window = std::unique_ptr<Window>(Window::Create()); 	//we don't have to manually delete the window when the application terminates
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

		m_ImGuiLayer = new ImGuiLayer();

		PushOverlay(m_ImGuiLayer);

		m_VertexArray.reset(VertexArray::Create());

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
		m_VertexArray->AddVertexBuffer(m_VertexBuffer); //todo may need to leverage Unbind more, since there could be bad associations to the bind VAO


		unsigned int indices[6] = {
			0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
			2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
		};

		m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);
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
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();

			Renderer::BeginScene();

			Renderer::Submit(m_VertexArray);

			Renderer::EndScene();

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

