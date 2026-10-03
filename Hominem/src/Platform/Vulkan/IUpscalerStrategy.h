#pragma once

#include "VulkanCore.h"
#include "VulkanRenderTarget.h"
#include "Hominem/Renderer/PostProcess/Upscaling/Upscaler.h"

#include <glm/glm.hpp>

namespace Hominem {

struct UpscaleImages
{
    const VulkanRenderTarget& color;
    const VulkanRenderTarget& velocity;
    const VulkanRenderTarget& depth;
    const VulkanRenderTarget& output;
    glm::uvec2                renderSize;
    glm::uvec2                outputSize;
};

class IUpscalerStrategy
{
public:
    virtual ~IUpscalerStrategy() = default;

    virtual const char*  GetName() const = 0;
    virtual bool         IsAvailable() const = 0;
    /// False runs the frame at native resolution with no AA (the ImGui A/B toggle).
    virtual bool         IsEnabled() const { return true; }
    virtual UpscalerCaps GetCaps() const = 0;
    virtual float        GetRenderScale(glm::uvec2 outputSize) const = 0;

    /// Main thread, once per frame.
    virtual void PollDebugKeys() {}

    /// Records the upscale into images.output. False when it couldn't run this frame;
    /// VulkanUpscaler blits instead. Render thread.
    virtual bool Evaluate(VkCommandBuffer cmd, const UpscaleImages& images, const UpscalerInputs& in) = 0;
};

}
