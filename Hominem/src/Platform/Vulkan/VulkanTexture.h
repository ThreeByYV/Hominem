#pragma once

#include "VulkanCore.h"
#include "VulkanImage.h"

namespace Hominem {

class VulkanTexture
{
public:
    static VulkanTexture Create(VkDevice device, VmaAllocator allocator,
                                uint32_t width, uint32_t height,
                                VkFormat format, VkImageUsageFlags usage,
                                bool withSampler = false);
    void Destroy(VkDevice device, VmaAllocator allocator);

    VkImage     GetImage()     const { return m_Image.image; }
    VkImageView GetImageView() const { return m_Image.imageView; }
    VkSampler   GetSampler()   const { return m_Sampler; }
    VkFormat    GetFormat()    const { return m_Image.format; }
    VkExtent2D  GetExtent()    const { return { m_Image.extent.width, m_Image.extent.height }; }

private:
    VulkanAllocatedImage m_Image   = {};
    VkSampler            m_Sampler = VK_NULL_HANDLE;
};

}
