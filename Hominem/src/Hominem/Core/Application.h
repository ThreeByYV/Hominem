#pragma once

#include "Core.h"
#include "Window.h"
#include "Hominem/Events/Event.h"
#include "Hominem/Events/ApplicationEvent.h"
#include "Hominem/Core/LayerStack.h"


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

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        inline Window& GetWindow() { return *m_Window; }

        inline static Application& Get() { return *s_Instance; }

        ~Application();

    private:
        bool OnWindowClose(WindowCloseEvent& e);
       
        std::unique_ptr<Window> m_Window;
        bool m_Running;
        LayerStack m_LayerStack;
    private:
        static Application* s_Instance; //Application is a singleton
    };

  
}
