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

    // GGX importance-sampled prefilter of a baked environment cube into a
    // mip-chained specular cubemap, one roughness value per mip. Must be
    // called on the render thread, after the source cube has been baked.
    static Ref<TextureCube> PrefilterSpecular(const Ref<TextureCube>& source,
                                               uint32_t resolution = 128);

    // Pre-integrates the split-sum specular BRDF (scale, bias) over (NdotV, roughness)
    // into an RG16F 2D LUT. Independent of any scene content — bake once at startup.
    // Must be called on the render thread.
    static Ref<Texture2D> BakeBRDFLUT(uint32_t resolution = 512);
};

} // namespace Hominem
