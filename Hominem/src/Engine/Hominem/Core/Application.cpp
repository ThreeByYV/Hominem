#include "hmnpch.h"
#include "Application.h"
#include "VFS.h"
#include "Core.h"
#include "Hominem/Renderer/Buffer.h"
#include "Input.h"
#include "InputMap.h"
#include "glm/glm.hpp"
#include "Hominem/Events/KeyEvent.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Renderer/RenderCommand.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Renderer3D.h"
#include "Hominem/Core/Task.h"
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

#ifdef HMN_ENGINE_RESOURCES_PATH
		VFS::Mount("engine", HMN_ENGINE_RESOURCES_PATH "/");
		HMN_CORE_INFO("VFS: engine path from source define");
#else
		VFS::Mount("engine", "EngineResources/");
		HMN_CORE_WARN("VFS: HMN_ENGINE_RESOURCES_PATH not set — relying on post-build copy");
#endif
		VFS::Mount("game", "Resources/");

#ifdef HMN_PLATFORM_WINDOWS
		// Default Windows timer resolution is 15.6ms — sleep_for snaps to that quantum.
		// 1ms resolution makes the frame limiter sleep accurate to ~1ms.
		timeBeginPeriod(1);
#endif
		m_Window = std::unique_ptr<Window>(Window::Create()); 	//we don't have to manually delete the window when the application terminates
		m_Window->SetEventCallback(HMN_BIND_EVENT_FN(Application::OnEvent));

		RenderCommand::Init();
		Renderer2D::Init();
		Renderer3D::Init();
		AssetManager::Init();

		// Default gameplay action bindings. Games can rebind via InputMap::Bind.
		InputMap::Bind("MoveLeft",  HMN_KEY_A);
		InputMap::Bind("MoveRight", HMN_KEY_D);
		InputMap::Bind("OpenMenu",  HMN_KEY_ESCAPE);
		InputMap::Bind("SkipIntro", HMN_KEY_1);

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
		dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ke) {
			if (ke.GetRepeatCount() != 0) return false;
			if (ke.GetKeyCode() == HMN_KEY_F)
			{
				m_Window->ToggleFullscreen();
				bool vsync = m_Window->IsVSync();
				RenderThread::QueueUpload([vsync] {
					glfwSwapInterval(vsync ? 1 : 0);
				});
				return true;
			}
			return false;
		});


		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled) break;
		}

		// If no layer consumed ESC, quit.
		if (!e.Handled)
		{
			dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ke) {
				if (ke.GetKeyCode() == HMN_KEY_ESCAPE && ke.GetRepeatCount() == 0)
				{
					m_Running = false;
					return true;
				}
				return false;
			});
		}
	}

	void Application::Run()
	{
		// Hand the GL context to the render thread — all GL calls happen there from now on.
		auto* nativeWindow = static_cast<GLFWwindow*>(m_Window->GetNativeWindow());
		glfwMakeContextCurrent(nullptr); // release from main thread
		m_RenderThread.Start(nativeWindow, m_Window->GetWidth(), m_Window->GetHeight());

		while (m_Running)
		{
			HMN_PROFILE_FRAME("MainThread");

			if (m_Minimized)
			{
				// Window is minimized — nothing to render or update.
				// Block until a window event wakes us so we use ~0% CPU.
				glfwWaitEvents();
				continue;
			}

			float time      = (float)glfwGetTime();
			// Clamp: a stall (asset load, window drag, breakpoint, OS scheduling) must not
			// hand layers a huge dt — that skips animations and can blow up physics contacts.
			constexpr float kMaxFrameTime = 1.0f / 20.0f;
			Timestep timestep = std::min(time - m_LastFrameTime, kMaxFrameTime);
			m_LastFrameTime = time;

			AssetManager::PumpCallbacks();
			CoroutineScheduler::Update(timestep);

			// Debug-only for any layer
			const bool muteKeyDown = Input::IsKeyPressed(HMN_KEY_M);
			if (muteKeyDown && !m_MuteKeyHeld)
			{
				m_Muted = !m_Muted;
				if (m_Muted)
				{
					m_VolumeBeforeMute = AudioSystem::Get().GetMasterVolume();
					AudioSystem::Get().SetMasterVolume(0.f);
					HMN_CORE_INFO("Audio: muted");
				}
				else
				{
					AudioSystem::Get().SetMasterVolume(m_VolumeBeforeMute);
					HMN_CORE_INFO("Audio: unmuted (volume {0})", m_VolumeBeforeMute);
				}
			}
			m_MuteKeyHeld = muteKeyDown;

			for (auto& layer : m_LayerStack)
			{
				layer->OnUpdate(timestep);
				if (UIRoot* ui = layer->GetUIRoot())
					ui->OnUpdate(timestep);
			}

			m_RenderThread.WaitImGuiConsumed();

			m_ImGuiLayer->Begin();
			for (auto& layer : m_LayerStack)
				layer->OnImGuiRender();
			m_ImGuiLayer->End();       // ImGui::Render() — writes draw data
			m_RenderThread.SignalImGuiReady();

			// Collect draw commands
			// Wire the write arena so actors can bump-allocate bone matrices.
			RenderFrame frame;
			frame.arena = &m_RenderThread.GetWriteArena();
			for (auto& layer : m_LayerStack)
			{
				if (Scene* scene = layer->GetScene())
					scene->BuildRenderFrame(frame);
				if (UIRoot* ui = layer->GetUIRoot())
					ui->BuildRenderFrame(frame);
			}

			// Record every pass into CommandLists; the render thread only replays them.
			RecordedFrame recorded;
			recorded.passCmds       = m_RenderThread.GetSceneRenderer().Record(frame);
			recorded.vulkanPasses      = std::move(frame.vulkanPasses);
			recorded.vulkanMeshUploads = std::move(frame.vulkanMeshUploads);
			recorded.vulkanMeshDraws   = std::move(frame.vulkanMeshDraws);
			recorded.vulkanDebugSpheres = std::move(frame.vulkanDebugSpheres);

			recorded.vulkanView.view      = frame.view3D;
			recorded.vulkanView.proj      = frame.proj3D;
			recorded.vulkanView.cameraPos = frame.cameraWorldPos;
			recorded.vulkanView.ambient   = frame.light.AmbientColor * frame.light.AmbientIntensity;
			for (uint32_t i = 0; i < kVulkanSceneLightCount && i < frame.lights.size(); ++i)
				recorded.vulkanView.lights[i] = frame.lights[i];
			recorded.viewportWidth  = frame.viewportWidth;
			recorded.viewportHeight = frame.viewportHeight;
			recorded.renderScale    = frame.renderScale;
			frame.arena->Reset();

			m_RenderThread.Submit(std::move(recorded));

			m_Window->OnUpdate(); // glfwPollEvents — SwapBuffers moved to render thread

			ProcessPendingTransitions();
		}

		m_Window->Hide(); // hide before teardown to avoid black-flash on close
		m_RenderThread.Stop();

		// Re-acquire the GL context — render thread released it, but we need it
		// for all GL cleanup below (OnDetach, Renderer statics, layer resources).
		glfwMakeContextCurrent(nativeWindow);

		// Detach and destroy all layers now while the context is valid.
		for (auto& layer : m_LayerStack)
			layer->OnDetach();
		m_LayerStack.Clear();

		Renderer3D::Shutdown();
		Renderer2D::Shutdown();
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

		RenderThread::QueueUpload([w = e.GetWidth(), h = e.GetHeight()] {
			RenderCommand::SetViewport(0, 0, w, h);
		});

		for (auto& layer : m_LayerStack)
			layer->SetViewportSize(e.GetWidth(), e.GetHeight());

		return false; //all layers will know about this event
	}

	
	void Application::PushLayer(std::unique_ptr<Layer> layer)
	{
		layer->SetViewportSize(m_Window->GetWidth(), m_Window->GetHeight());
		layer->OnAttach();
		m_LayerStack.PushLayer(std::move(layer));
	}

	void Application::PushOverlay(std::unique_ptr<Layer> layer)
	{
		layer->SetViewportSize(m_Window->GetWidth(), m_Window->GetHeight());
		layer->OnAttach();
		m_LayerStack.PushOverlay(std::move(layer));
	}

	void Application::QueueLayerTransition(const std::string& oldLayerName, std::unique_ptr<Layer> newLayer)
	{
		m_PendingTransitions.push_back({ oldLayerName, std::move(newLayer) });
	}

	void Application::ProcessPendingTransitions()
	{
		if (m_PendingTransitions.empty()) return;

		// Wait for the render thread to finish the last submitted frame to avoid GL_INVALID_OPERATION
		m_RenderThread.WaitIdle();

		// OnAttach can itself queue a transition (e.g. a loading screen whose assets are
		// already cached transitions synchronously). Drain into a local so those re-entrant
		// queues survive to the next frame instead of being cleared out from under us.
		auto transitions = std::move(m_PendingTransitions);
		m_PendingTransitions.clear();

		for (auto& transition : transitions)
		{
			for (auto& layer : m_LayerStack)
			{
				if (layer->GetName() == transition.oldLayerName) //Each layer will have to have a unique name
				{
					layer->OnDetach();  // Clean up old layer
					layer = std::move(transition.newLayer);  // Replace with new layer
					layer->SetViewportSize(m_Window->GetWidth(), m_Window->GetHeight());
					layer->OnAttach();  // Initialize new layer
					break;
				}
			}
		}
	}

	Application::~Application()
	{
		AssetManager::Shutdown();
		// Shutdown audio system before other cleanup
		m_AudioSystem.Shutdown();
		HMN_CORE_INFO("AudioSystem shutdown complete");

#ifdef HMN_PLATFORM_WINDOWS
		timeEndPeriod(1);
#endif
	}

}

