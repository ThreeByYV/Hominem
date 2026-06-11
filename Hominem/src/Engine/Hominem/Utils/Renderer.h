#pragma once

#include "Hominem/Renderer/Shader.h"
#include <glm/glm.hpp>

using namespace Hominem;

inline Ref<Shader> SelectShader(const Ref<Shader>& meshShader,
    const Ref<Shader>& overrideShader,
    const Ref<Shader>& defaultShader)
{
    if (overrideShader) return overrideShader;
    if (meshShader)     return meshShader;
    return defaultShader;
}

// AiToGlm conversions moved to Hominem/Utils/AssimpGlm.h
