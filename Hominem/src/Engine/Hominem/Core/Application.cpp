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
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#ifdef HMN_PLATFORM_WINDOWS
    #include <windows.h>
    #include <timeapi.h>
    #pragma comment(lib, "winmm.lib")
#endif

#include "Hominem/Renderer/Camera.h"


namespace Hominem {

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		HMN_CORE_ASSERT(!s_Instance, "Application already exists!");

		s_Instance = this;

#ifdef HMN_PLATFORM_WINDOWS
		// Default Windows timer resolution is 15.6ms — sleep_for snaps to that quantum.
		// 1ms resolution makes the frame limiter sleep accurate to ~1ms.
		timeBeginPeriod(1);
#endif
		m_Window = std::unique_ptr<Window>(Window::Create()); 	//we don't have to manually delete the window when the application terminates
		m_Window->SetEventCallback(HMN_BIND_EVENT_FN(Application::OnEvent));

		Renderer::Init();

		// Initialize audio system
		AudioConfig audioConfig;
		audioConfig.SampleRate = 44100;
		audioConfig.MaxSounds = 64;
		audioConfig.MasterVolume = 1.0f;

		if (!m_AudioSystem.Init(audioConfig))
		{
			HMN_CORE_ERROR("Failed to initialize AudioSystem!");
		}
		else
		{
			HMN_CORE_INFO("AudioSystem initialized successfully");
		}

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
		// Hand the GL context to the render thread — all GL calls happen there from now on.
		GLFWwindow* nativeWindow = static_cast<GLFWwindow*>(m_Window->GetNativeWindow());
		glfwMakeContextCurrent(nullptr); // release from main thread
		m_RenderThread.Start(nativeWindow);

		while (m_Running)
		{
			HMN_PROFILE_FRAME("MainThread");

			float time      = (float)glfwGetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			if (Input::IsKeyPressed(HMN_KEY_ESCAPE))
				m_Running = false;

			if (!m_Minimized)
			{
				for (auto& layer : m_LayerStack)
					layer->OnUpdate(timestep);
			}

			//todo: I dont think i like how application knows this much abt render thread, imgui and signals for it, anything else can be done?

			// Wait for render thread to finish reading the previous frame's draw data,
			// then build this frame's ImGui and signal the render thread it can read.
			m_RenderThread.WaitImGuiConsumed();

			m_ImGuiLayer->Begin();
			for (auto& layer : m_LayerStack)
				layer->OnImGuiRender();
			m_ImGuiLayer->End();       // ImGui::Render() — writes draw data
			m_RenderThread.SignalImGuiReady();

			// Collect draw commands — pure data, no GL.
			// Wire the write arena so actors can bump-allocate bone matrices.
			RenderFrame frame;
			frame.arena    = &m_RenderThread.GetWriteArena();
			frame.arenaIdx = m_RenderThread.GetWriteArenaIdx();
			if (!m_Minimized)
			{
				for (auto& layer : m_LayerStack)
					layer->OnBuildRenderFrame(frame);
			}

			// Hand frame to render thread. Main thread continues immediately.
			// Blocks only if render thread hasn't consumed the previous frame yet (GPU-bound).
			m_RenderThread.Submit(std::move(frame));

			m_Window->OnUpdate(); // glfwPollEvents — SwapBuffers moved to render thread

			ProcessPendingTransitions();
		}

		m_RenderThread.Stop();

		// Re-acquire the GL context — render thread released it, but we need it
		// for all GL cleanup below (OnDetach, Renderer statics, layer resources).
		glfwMakeContextCurrent(nativeWindow);

		// Detach and destroy all layers now while the context is valid.
		// LayerStack's destructor is =default so it never calls OnDetach itself,
		// and m_LayerStack outlives m_Window (declared first = destroyed last),
		// meaning its destructor would run after the context is gone.
		for (auto& layer : m_LayerStack)
			layer->OnDetach();
		m_LayerStack.Clear(); // destroys layer unique_ptrs (and their GL resources) here

		// Free all Renderer static GL objects (shaders, VAOs, white texture, etc.)
		Renderer::Shutdown();
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
		// Shutdown audio system before other cleanup
		m_AudioSystem.Shutdown();
		HMN_CORE_INFO("AudioSystem shutdown complete");

#ifdef HMN_PLATFORM_WINDOWS
		timeEndPeriod(1);
#endif
	}

}

