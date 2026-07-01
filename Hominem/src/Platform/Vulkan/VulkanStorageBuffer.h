#pragma once

#include "VulkanCore.h"
#include <cstdint>

namespace Hominem {

class VulkanStorageBuffer
{
public:
    static VulkanStorageBuffer Create(VmaAllocator allocator, VkDeviceSize capacity);
    void Upload(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void Destroy(VmaAllocator allocator);

    VkBuffer     GetBuffer()   const { return m_Buffer; }
    VkDeviceSize GetCapacity() const { return m_Capacity; }

private:
    VkBuffer      m_Buffer     = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    void*         m_MappedData = nullptr;
    VkDeviceSize  m_Capacity   = 0;
};

}
