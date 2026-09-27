#include "hmnpch.h"
#include "VulkanPassthroughUpscaler.h"

namespace Hominem {

void VulkanPassthroughUpscaler::RecordUpscale(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge,
                                              const UpscalerInputs&)
{
    using B = VulkanUpscalerBridge;
    const glm::uvec2 src = bridge.RenderSize();
    const glm::uvec2 dst = bridge.OutputSize();

    const VkImageBlit region
    {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets     = { { 0, 0, 0 }, { (int32_t)src.x, (int32_t)src.y, 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets     = { { 0, 0, 0 }, { (int32_t)dst.x, (int32_t)dst.y, 1 } },
    };
    vkCmdBlitImage(cmd, bridge.Target(B::Color).GetImage(),  VK_IMAGE_LAYOUT_GENERAL,
                        bridge.Target(B::Output).GetImage(), VK_IMAGE_LAYOUT_GENERAL,
                        1, &region, VK_FILTER_LINEAR);
}

}
