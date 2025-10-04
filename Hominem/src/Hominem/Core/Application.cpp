#include "hmnpch.h"
#include "Application.h"
#include "Core.h"
#include "Hominem/Renderer/Buffer.h"
#include "Input.h"
#include "glm/glm.hpp"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer.h"
#include <GLFW/glfw3.h>

#include "Hominem/Renderer/Camera.h"


namespace Hominem {

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		HMN_CORE_ASSERT(!s_Instance, "Application already exists!");
		
		s_Instance = this;
		m_Window = std::unique_ptr<Window>(Window::Create()); 	//we don't have to manually delete the window when the application terminates
		m_Window->SetEventCallback(HMN_BIND_EVENT_FN(Application::OnEvent));
		
		Renderer::Init();

		auto imGuiLayer = std::make_unique<ImGuiLayer>();
		m_ImGuiLayer = imGuiLayer.get();

		PushOverlay(std::move(imGuiLayer));
	}

	//this function leverages the EventDispatcher to pass an event to the correct method
	void Application::OnEvent(Event& e)
	{
		//HMN_CORE_TRACE("Event received: {}", e.ToString());
		
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(HMN_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(Application::OnWindowResize));


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
			if (Input::IsKeyPressed(HMN_KEY_ESCAPE))
				m_Running = false;

			float time = (float)glfwGetTime();

			Timestep timestep = time - m_LastFrameTime; //impliclit cast via constructor
			m_LastFrameTime = time;

			//layers shouldn't be updated when the window is minimized away
			if (!m_Minimized)
			{
				for (auto& layer : m_LayerStack)
				{
					layer->OnUpdate(timestep);
				}
			}

			m_ImGuiLayer->Begin();

			for (auto& layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}

			m_ImGuiLayer->End();
	
			m_Window->OnUpdate();

			// Process pending transitions AFTER all updates complete
			ProcessPendingTransitions(); //now it's safe to delete it all and replace 
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;

		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
			
		return false; //all layers will know about this event
	}

	
	void Application::PushLayer(std::unique_ptr<Layer> layer)
	{
		layer->OnAttach();  
		m_LayerStack.PushLayer(std::move(layer));
	}

	void Application::PushOverlay(std::unique_ptr<Layer> layer)
	{
		layer->OnAttach(); 
		m_LayerStack.PushOverlay(std::move(layer));
	}

	void Application::QueueLayerTransition(const std::string& oldLayerName, std::unique_ptr<Layer> newLayer)
	{
		m_PendingTransitions.push_back({ oldLayerName, std::move(newLayer) });
	}

	void Application::ProcessPendingTransitions()
	{
		for (auto& transition : m_PendingTransitions)
		{
			for (auto& layer : m_LayerStack)
			{
				if (layer->GetName() == transition.oldLayerName) //Each layer will have to have a unique name
				{
					layer->OnDetach();  // Clean up old layer
					layer = std::move(transition.newLayer);  // Replace with new layer
					layer->OnAttach();  // Initialize new layer
					break;
				}
			}
		}

		m_PendingTransitions.clear();
	}

	Application::~Application()
	{
	}

}

