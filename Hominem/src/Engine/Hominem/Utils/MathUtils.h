#pragma once

#include <cstdint>

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

} // namespace Hominem
