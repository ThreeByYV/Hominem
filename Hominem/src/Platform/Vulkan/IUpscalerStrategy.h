#pragma once

#include "VulkanCore.h"
#include "Hominem/Renderer/PostProcess/Upscaling/Upscaler.h"

#include <glm/glm.hpp>

namespace Hominem {

class VulkanUpscalerBridge;

/// Strategy for VulkanUpscaler: one vendor SDK (DLSS, FSR). The bridge images arrive upright, in
/// VK_IMAGE_LAYOUT_GENERAL: color, velocity and depth at render size, output at output size.
class IUpscalerStrategy
{
public:
    virtual ~IUpscalerStrategy() = default;

    virtual const char*  GetName() const = 0;
    virtual bool         IsAvailable() const = 0;
    /// False runs the frame at native resolution with no AA (the ImGui A/B toggle).
    virtual bool         IsEnabled() const { return true; }
    virtual UpscalerCaps GetCaps() const = 0;
    /// Any thread.
    virtual float        GetRenderScale(glm::uvec2 outputSize) const = 0;

    /// Main thread, once per frame.
    virtual void PollDebugKeys() {}

    /// Records the upscale into the bridge output. False when it couldn't run this frame;
    /// VulkanUpscaler blits instead. Render thread.
    virtual bool Evaluate(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge, const UpscalerInputs& in) = 0;
};

}
