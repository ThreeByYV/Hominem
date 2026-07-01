#include "hmnpch.h"
#include "VulkanRenderTarget.h"

namespace Hominem {

VulkanRenderTarget VulkanRenderTarget::Create(VkDevice device, VmaAllocator allocator,
                                               uint32_t width, uint32_t height, VkFormat format)
{
    constexpr VkImageUsageFlags usage =
        VK_IMAGE_USAGE_STORAGE_BIT     |
        VK_IMAGE_USAGE_SAMPLED_BIT     |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VulkanRenderTarget rt;
    rt.m_Texture = VulkanTexture::Create(device, allocator, width, height, format, usage, true);
    return rt;
}

void VulkanRenderTarget::Destroy(VkDevice device, VmaAllocator allocator)
{
    m_Texture.Destroy(device, allocator);
}

}
