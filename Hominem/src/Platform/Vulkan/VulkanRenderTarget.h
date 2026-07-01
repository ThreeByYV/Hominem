#pragma once

#include "VulkanTexture.h"

namespace Hominem {

class VulkanRenderTarget
{
public:
    static VulkanRenderTarget Create(VkDevice device, VmaAllocator allocator,
                                     uint32_t width, uint32_t height, VkFormat format);
    void Destroy(VkDevice device, VmaAllocator allocator);

    VkImage     GetImage()     const { return m_Texture.GetImage(); }
    VkImageView GetImageView() const { return m_Texture.GetImageView(); }
    VkSampler   GetSampler()   const { return m_Texture.GetSampler(); }
    VkFormat    GetFormat()    const { return m_Texture.GetFormat(); }
    VkExtent2D  GetExtent()    const { return m_Texture.GetExtent(); }

private:
    VulkanTexture m_Texture;
};

}
