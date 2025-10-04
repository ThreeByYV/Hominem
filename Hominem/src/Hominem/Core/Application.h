#pragma once

#include "Core.h"
#include "Window.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Core/LayerStack.h"
#include "Hominem/ImGui/ImGuiLayer.h"
#include "Hominem/Core/Timestep.h"


namespace Hominem {

    class Application
    {
    public:
        Application();

        void Initialize();
        void Update(float deltaTime);
        void Render();
      
        void Run();

        void OnEvent(Event& e);

        void PushLayer(std::unique_ptr<Layer> layer);
        void PushOverlay(std::unique_ptr<Layer> layer);

        void QueueLayerTransition(const std::string& oldLayerName, std::unique_ptr<Layer> newLayer);


        inline Window& GetWindow() { return *m_Window; }

        inline static Application& Get() { return *s_Instance; }

        ~Application();

    public:
        LayerStack m_LayerStack;

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);

        void ProcessPendingTransitions();
    private:
        std::unique_ptr<Window> m_Window;
        ImGuiLayer* m_ImGuiLayer; //by default always will have a ImGui layer existing
       
        bool m_Running = true;
        bool m_Minimized = false;

     
        Timestep m_Timestep;
        float m_LastFrameTime = 0.0f;

        struct PendingTransition {
            std::string oldLayerName;
            std::unique_ptr<Layer> newLayer;
        };
        std::vector<PendingTransition> m_PendingTransitions;
       
    private:
        static Application* s_Instance; //Application is a singleton

    };
}
