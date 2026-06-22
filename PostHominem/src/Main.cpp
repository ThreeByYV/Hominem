#include "hmnpch.h"
#include "Hominem.h"
#include "Layers/SandboxLayer.h"
#include "Layers/MenuLayer.h"

#ifdef HMN_PLATFORM_WINDOWS

int main(int argc, char** argv)
{
    Hominem::Log::Init();

    HMN_CORE_TRACE("Initialized Log");

    Hominem::Application app;
    app.PushLayer(std::make_unique<MenuLayer>());
    app.Run();
}

#endif