#pragma once

namespace Hominem {

/// Linearly interpolates between a and b by t (0=a, 1=b).
/// Works with float, glm::vec2/3/4, or any type supporting + and scalar *.
template<typename T>
inline T Lerp(const T& a, const T& b, float t)
{
    return a + (b - a) * t;
}

} // namespace Hominem
