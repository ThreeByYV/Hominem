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
    VkDeviceAddress GetIndexBufferAddress()  const { return m_IndexAddress; }
    VkBuffer        GetIndexBuffer()         const { return m_IndexBuffer; }
    uint32_t        GetIndexCount()          const { return m_IndexCount; }
    uint32_t        GetVertexCount()         const { return m_VertexCount; }

private:
    VkBuffer        m_VertexBuffer     = VK_NULL_HANDLE;
    VmaAllocation   m_VertexAllocation = VK_NULL_HANDLE;
    VkDeviceAddress m_VertexAddress    = 0;
    uint32_t        m_VertexCount      = 0;
    VkBuffer        m_IndexBuffer      = VK_NULL_HANDLE;
    VmaAllocation   m_IndexAllocation  = VK_NULL_HANDLE;
    VkDeviceAddress m_IndexAddress     = 0;
    uint32_t        m_IndexCount       = 0;
};

}
