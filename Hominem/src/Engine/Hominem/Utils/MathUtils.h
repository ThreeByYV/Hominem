#pragma once

#include <cstdint>
#include <cfloat>
#include <glm/glm.hpp>

namespace Hominem {

/// Linearly interpolates between a and b by t (0=a, 1=b).
/// Works with float, glm::vec2/3/4, or any type supporting + and scalar *.
template<typename T>
inline T Lerp(const T& a, const T& b, float t)
{
    return a + (b - a) * t;
}

/// PCG-style integer hash. Use to derive deterministic pseudo-random values
/// (e.g. per-instance shader seeds) from an index.
static inline uint32_t PCG_Hash(int32_t input)
{
    uint32_t seed = (uint32_t)input * 135193u + 812921u;
    seed = (seed ^ (seed >> 16)) * 0x45d9f3bu;
    return (seed ^ (seed >> 16)) * 0x45d9f3bu;
}

/// Transforms a local-space AABB by a matrix and returns the enclosing world-space AABB
/// (re-fit to the 8 transformed corners -- conservative, still axis-aligned).
inline void TransformAABB(const glm::vec3& localMin, const glm::vec3& localMax,
                           const glm::mat4& transform, glm::vec3& outMin, glm::vec3& outMax)
{
    outMin = glm::vec3( FLT_MAX);
    outMax = glm::vec3(-FLT_MAX);
    for (int i = 0; i < 8; i++)
    {
        const glm::vec3 corner(
            (i & 1) ? localMax.x : localMin.x,
            (i & 2) ? localMax.y : localMin.y,
            (i & 4) ? localMax.z : localMin.z);
        const glm::vec3 worldCorner = glm::vec3(transform * glm::vec4(corner, 1.f));
        outMin = glm::min(outMin, worldCorner);
        outMax = glm::max(outMax, worldCorner);
    }
}

} // namespace Hominem
