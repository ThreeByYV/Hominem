#include "hmnpch.h"
#include "Hominem/Core/Hominem.h"
#include "Hominem/Layers/SandboxLayer.h"
#include "Hominem/Layers/OverlayLayer.h"

#ifdef HMN_PLATFORM_WINDOWS


int main(int argc, char** argv)
{
    Hominem::Log::Init();

    HMN_CORE_TRACE("Initialized Log");

    Hominem::Application app;

    app.PushLayer(std::make_unique<Hominem::SandboxLayer>());
    app.PushLayer(std::make_unique<Hominem::OverlayLayer>());

    app.Run();
}

#endif