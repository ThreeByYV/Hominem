#pragma once

#include "VulkanCore.h"

#include <cstdint>
#include <span>

namespace Hominem {

class VulkanMeshBuffer
{
public:
    static VulkanMeshBuffer Create(VkDevice device, VmaAllocator allocator,
                                   VkCommandBuffer frameCmd, DeletionQueue& frameQueue,
                                   std::span<const uint8_t> vertexData,
                                   std::span<const uint32_t> indices);
    void Destroy(VmaAllocator allocator);

    VkDeviceAddress GetVertexBufferAddress() const { return m_VertexAddress; }
    VkBuffer        GetIndexBuffer()         const { return m_IndexBuffer; }
    uint32_t        GetIndexCount()          const { return m_IndexCount; }

private:
    VkBuffer        m_VertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation   m_VertexAllocation = VK_NULL_HANDLE;
    VkDeviceAddress m_VertexAddress    = 0;
    VkBuffer        m_IndexBuffer      = VK_NULL_HANDLE;
    VmaAllocation   m_IndexAllocation  = VK_NULL_HANDLE;
    uint32_t        m_IndexCount       = 0;
};

}
