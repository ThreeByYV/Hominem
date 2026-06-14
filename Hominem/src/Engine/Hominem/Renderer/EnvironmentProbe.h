#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Renderer/Texture.h"
#include "RenderFrame.h"
#include <glm/glm.hpp>

namespace Hominem {

// Renders the scene into a cubemap from a single capture point.
// Bake() must be called on the render thread (from SceneRenderer::GeometryPass).
class EnvironmentProbe
{
public:
    static Ref<TextureCube> Bake(const glm::vec3& capturePos,
                                  const RenderFrame& sceneFrame,
                                  uint32_t resolution = 512);

    // Cosine-weighted hemisphere convolution of a baked environment cube into a
    // small diffuse irradiance cubemap. Must be called on the render thread,
    // after the source cube has been baked.
    static Ref<TextureCube> ConvolveIrradiance(const Ref<TextureCube>& source,
                                                uint32_t resolution = 32);
};

} // namespace Hominem
