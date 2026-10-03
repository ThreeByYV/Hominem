#pragma once

#include "Platform/Vulkan/IUpscalerStrategy.h"

namespace Hominem {

/// todo impl this
class FSR final : public IUpscalerStrategy
{
public:
    const char*  GetName() const override                 { return "FSR"; }
    bool         IsAvailable() const override             { return false; }
    UpscalerCaps GetCaps() const override                 { return {}; }
    float        GetRenderScale(glm::uvec2) const override { return 1.f; }

    bool Evaluate(VkCommandBuffer, const VulkanUpscalerBridge&, const UpscalerInputs&) override { return false; }
};

}
