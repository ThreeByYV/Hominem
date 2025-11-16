#pragma once

#include "Hominem/Renderer/Shader.h"
#include <glad/glad.h>

using namespace Hominem;

inline Ref<Shader> SelectShader(const Ref<Shader>& meshShader,
    const Ref<Shader>& overrideShader,
    const Ref<Shader>& defaultShader)
{
    if (overrideShader) return overrideShader;
    if (meshShader)     return meshShader;
    return defaultShader;
}


