#include "hmnpch.h"
#include "VulkanStorageBuffer.h"

#include <cstring>

namespace Hominem {

VulkanStorageBuffer VulkanStorageBuffer::Create(VmaAllocator allocator, VkDeviceSize capacity)
{
    const VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = capacity,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };
    const VmaAllocationCreateInfo allocInfo {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
               | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo info = {};
    VulkanStorageBuffer buf;
    buf.m_Capacity = capacity;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                             &buf.m_Buffer, &buf.m_Allocation, &info));
    buf.m_MappedData = info.pMappedData;
    return buf;
}

void VulkanStorageBuffer::Upload(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    HMN_CORE_ASSERT(m_MappedData);
    HMN_CORE_ASSERT(offset + size <= m_Capacity);
    std::memcpy(static_cast<char*>(m_MappedData) + offset, data, static_cast<size_t>(size));
}

void VulkanStorageBuffer::Destroy(VmaAllocator allocator)
{
    if (m_Buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_Buffer, m_Allocation);
        m_Buffer     = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;
        m_MappedData = nullptr;
    }
}

}
