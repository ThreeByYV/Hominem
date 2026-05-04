#pragma once

#include "Hominem/Renderer/Shader.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>

using namespace Hominem;

inline Ref<Shader> SelectShader(const Ref<Shader>& meshShader,
    const Ref<Shader>& overrideShader,
    const Ref<Shader>& defaultShader)
{
    if (overrideShader) return overrideShader;
    if (meshShader)     return meshShader;
    return defaultShader;
}


inline glm::mat4 AiToGlm(const aiMatrix4x4& m)
{
    // Transpose from Assimp row-major to GLM column-major
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,  // column 0 (from row 0 of aiMatrix4x4)
        m.a2, m.b2, m.c2, m.d2,  // column 1
        m.a3, m.b3, m.c3, m.d3,  // column 2
        m.a4, m.b4, m.c4, m.d4   // column 3
    );
}

inline glm::vec3 AiToGlm(const aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

inline glm::quat AiToGlm(const aiQuaternion& q)
{
    // GLM quaternion constructor: (w, x, y, z)
    return glm::quat(q.w, q.x, q.y, q.z);
}
