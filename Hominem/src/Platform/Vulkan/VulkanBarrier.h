#pragma once

#include "VulkanCore.h"

namespace Hominem {

/// @brief Build a global memory barrier from src/dst stage + access masks.
inline VkMemoryBarrier2 MakeMemoryBarrier(VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                                          VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
{
    return VkMemoryBarrier2
    {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask  = dstStage,
        .dstAccessMask = dstAccess,
    };
}

/// @brief Record one global memory barrier via vkCmdPipelineBarrier2.
inline void PipelineBarrier(VkCommandBuffer cmd, const VkMemoryBarrier2& barrier)
{
    const VkDependencyInfo depInfo
    {
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &depInfo);
}

}
