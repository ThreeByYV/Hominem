#pragma once

#include "VulkanTexture.h"

namespace Hominem {

class VulkanRenderTarget
{
public:
    static VulkanRenderTarget Create(VkDevice device, VmaAllocator allocator,
                                     uint32_t width, uint32_t height, VkFormat format);

    /// Backed by memory OpenGL can import as a texture.
    static VulkanRenderTarget CreateShared(VkDevice device, VkPhysicalDevice physical,
                                           uint32_t width, uint32_t height, VkFormat format);

    void Destroy(VkDevice device, VmaAllocator allocator);

    VkImage     GetImage()     const { return m_Texture.GetImage(); }
    VkImageView GetImageView() const { return m_Texture.GetImageView(); }
    VkSampler   GetSampler()   const { return m_Texture.GetSampler(); }
    VkFormat    GetFormat()    const { return m_Texture.GetFormat(); }
    VkExtent2D  GetExtent()    const { return m_Texture.GetExtent(); }

    bool         IsShared()      const { return m_Texture.IsShared(); }
    VkDeviceSize GetMemorySize() const { return m_Texture.GetMemorySize(); }
    HANDLE       GetWin32Handle(VkDevice device) const { return m_Texture.GetWin32Handle(device); }

private:
    VulkanTexture m_Texture;
};

}
