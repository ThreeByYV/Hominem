#pragma once

#include "glm/glm.hpp"
#include "Shader.h"
#include "Texture.h"

namespace Hominem {

    struct Quad
    {
        glm::vec3 Position{ 0.0f };
        glm::vec2 Size{ 1.0f, 1.0f };
        glm::vec4 Color{ 1.0f };
        Ref<Texture2D> Texture;     // optional
        Ref<Shader>    Shader;      // optional (per-Quad override)
        glm::mat4      Transform{ 1.0f }; // optional prebuilt transform
    };
}
