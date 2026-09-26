#include "hmnpch.h"
#include "TemporalJitter.h"

#include <algorithm>
#include <cmath>

namespace Hominem::TemporalJitter {

namespace {

constexpr uint32_t k_BasePhases = 16;

}

float Halton(uint32_t i, uint32_t base)
{
    float f = 1.f, r = 0.f;
    while (i > 0)
    {
        f /= (float)base;
        r += f * (float)(i % base);
        i /= base;
    }
    return r;
}

uint32_t PhaseCount(uint32_t renderWidth, uint32_t outputWidth)
{
    const float ratio = (float)outputWidth / (float)renderWidth;
    return std::max(k_BasePhases, (uint32_t)std::ceil(8.f * ratio * ratio));
}

glm::vec2 Offset(uint32_t frameIndex, uint32_t phaseCount)
{
    // +1: Halton(0) is 0 in every base.
    const uint32_t n = (frameIndex % phaseCount) + 1;
    return { Halton(n, 2) - 0.5f, Halton(n, 3) - 0.5f };
}

float MipBias(uint32_t renderWidth, uint32_t outputWidth, float offset)
{
    return std::log2((float)renderWidth / (float)outputWidth) + offset;
}

}
