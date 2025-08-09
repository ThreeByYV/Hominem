#include "hmnpch.h"

#ifdef HMN_PLATFORM_WINDOWS

#include "Hominem/Core/Hominem.h"

int main(int argc, char** argv)
{
    Hominem::Log::Init();

    HMN_CORE_TRACE("Initialized Log");

    Hominem::Application app;
    app.PushLayer(new Hominem::ExampleLayer());
    app.PushOverlay(new Hominem::ImGuiLayer());

    app.Run();
}

#endif