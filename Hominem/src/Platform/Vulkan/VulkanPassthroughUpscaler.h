#pragma once

#include "VulkanBridgedUpscaler.h"

namespace Hominem {

/// Debug backend: a plain bilinear blit through the bridge. Tests the GL<->Vulkan handoff
/// without any vendor SDK; select with HOMINEM_UPSCALER=passthrough.
class VulkanPassthroughUpscaler final : public VulkanBridgedUpscaler
{
public:
    using VulkanBridgedUpscaler::VulkanBridgedUpscaler;

    const char*  GetName() const override     { return "Vulkan passthrough (debug)"; }
    bool         IsSupported() const override { return true; }
    UpscalerCaps GetCaps() const override     { return { .temporal = false }; }

protected:
    void RecordUpscale(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge, const UpscalerInputs& in) override;
};

}
