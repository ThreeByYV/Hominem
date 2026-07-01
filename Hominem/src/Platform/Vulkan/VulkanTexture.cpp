#include "hmnpch.h"
#include "VulkanTexture.h"

namespace Hominem {

VulkanTexture VulkanTexture::Create(VkDevice device, VmaAllocator allocator,
                                    uint32_t width, uint32_t height,
                                    VkFormat format, VkImageUsageFlags usage,
                                    bool withSampler)
{
    VulkanTexture tex;
    tex.m_Image = VulkanImage::CreateImage(device, allocator,
                                           { width, height, 1 }, format, usage);
    if (withSampler)
    {
        const VkSamplerCreateInfo samplerInfo {
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_LINEAR,
            .minFilter    = VK_FILTER_LINEAR,
            .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        };
        VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &tex.m_Sampler));
    }
    return tex;
}

void VulkanTexture::Destroy(VkDevice device, VmaAllocator allocator)
{
    if (m_Sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
    VulkanImage::DestroyImage(device, allocator, m_Image);
}

}
