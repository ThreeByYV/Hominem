#include "hmnpch.h"
#include "Application.h"

#include <GLFW/glfw3.h>


namespace Hominem {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application::Application()
	{
		m_Window = std::unique_ptr<Window>(Window::Create()); 	//we don't have to manually delete the window when the application terminates
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
	
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
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{
			glClearColor(1, 0, 1, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			m_Window->OnUpdate();
		}

		for (Layer* layer : m_LayerStack)
			layer->OnUpdate();
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	};

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
	}


	Application::~Application()
	{
	}

}

