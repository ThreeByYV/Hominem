#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Hominem::TemporalJitter {

float Halton(uint32_t i, uint32_t base);

/// Grows with the upscale ratio; DLSS needs >= 18 phases at Quality, 32 at Performance.
uint32_t PhaseCount(uint32_t renderWidth, uint32_t outputWidth);

/// In render pixels, within [-0.5, 0.5).
glm::vec2 Offset(uint32_t frameIndex, uint32_t phaseCount);

/// `offset` is the upscaler's extra bias (UpscalerCaps::mipBiasOffset).
float MipBias(uint32_t renderWidth, uint32_t outputWidth, float offset);

}
