#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>

namespace Hominem {

/// Assimp row-major matrix to GLM column-major (transposes).
inline glm::mat4 AiToGlm(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,  // column 0
        m.a2, m.b2, m.c2, m.d2,  // column 1
        m.a3, m.b3, m.c3, m.d3,  // column 2
        m.a4, m.b4, m.c4, m.d4); // column 3
}

inline glm::vec3 AiToGlm(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }

inline glm::quat AiToGlm(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

}
