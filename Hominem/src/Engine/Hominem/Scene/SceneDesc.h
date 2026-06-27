#pragma once

#include "Hominem/Renderer/RenderFrame.h"
#include "Hominem/Renderer/PostProcessSettings.h"
#include <glm/glm.hpp>
#include <vector>

namespace Hominem {

/// Declarative description of a scene's static configuration.
/// Fill this out in SceneLayer::Describe() — the framework wires the rest.
struct SceneDesc
{
    struct Camera
    {
        float OrthoSize = 10.f;
        float Near      = -10.f;
        float Far       = 10.f;
    };

    struct Physics
    {
        bool      Enabled = true;
        glm::vec3 Gravity = { 0.f, -9.81f, 0.f };
    };

    Camera             Camera;
    Physics            Physics;
    glm::vec4          ClearColor = { 0.1f, 0.1f, 0.1f, 1.f };
    std::vector<Light> Lights;
    PostProcessSettings PostProcess;
};

} // namespace Hominem
