#include "hmnpch.h"
#include "VulkanMeshBuffer.h"
#include "VulkanStorageBuffer.h"
#include "Hominem/Assets/MeshData.h"

namespace Hominem {

static constexpr VkBufferUsageFlags kAccelInput =
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

namespace {

struct DeviceBuffer
{
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

DeviceBuffer CreateDeviceLocalBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage)
{
    const VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };
    const VmaAllocationCreateInfo allocInfo {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    DeviceBuffer out;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                             &out.buffer, &out.allocation, nullptr));
    return out;
}

}

VulkanMeshBuffer VulkanMeshBuffer::Create(VkDevice device, VmaAllocator allocator,
                                          VkCommandBuffer frameCmd, DeletionQueue& frameQueue,
                                          std::span<const uint8_t> vertexData,
                                          std::span<const uint32_t> indices)
{
    const VkDeviceSize vertexSize = vertexData.size_bytes();
    const VkDeviceSize indexSize  = indices.size_bytes();

    VulkanMeshBuffer mesh;
    mesh.m_IndexCount  = (uint32_t)indices.size();
    mesh.m_VertexCount = (uint32_t)(vertexSize / sizeof(StaticVertex));

    auto [vb, vbAlloc] = CreateDeviceLocalBuffer(allocator, vertexSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | kAccelInput);
    mesh.m_VertexBuffer     = vb;
    mesh.m_VertexAllocation = vbAlloc;

    auto [ib, ibAlloc] = CreateDeviceLocalBuffer(allocator, indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | kAccelInput);
    mesh.m_IndexBuffer     = ib;
    mesh.m_IndexAllocation = ibAlloc;

    const VkBufferDeviceAddressInfo vertexAddr
    {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = mesh.m_VertexBuffer,
    };
    mesh.m_VertexAddress = vkGetBufferDeviceAddress(device, &vertexAddr);

    const VkBufferDeviceAddressInfo indexAddr
    {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = mesh.m_IndexBuffer,
    };
    mesh.m_IndexAddress = vkGetBufferDeviceAddress(device, &indexAddr);

    auto staging = VulkanStorageBuffer::Create(allocator, vertexSize + indexSize,
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    staging.Upload(vertexData.data(), vertexSize, 0);
    staging.Upload(indices.data(), indexSize, vertexSize);

    const VkBufferCopy2 vertexRegion {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .size  = vertexSize,
    };
    const VkCopyBufferInfo2 vertexCopy {
        .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer   = staging.GetBuffer(),
        .dstBuffer   = mesh.m_VertexBuffer,
        .regionCount = 1,
        .pRegions    = &vertexRegion,
    };
    vkCmdCopyBuffer2(frameCmd, &vertexCopy);

    const VkBufferCopy2 indexRegion {
        .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = vertexSize,
        .size      = indexSize,
    };
    const VkCopyBufferInfo2 indexCopy {
        .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer   = staging.GetBuffer(),
        .dstBuffer   = mesh.m_IndexBuffer,
        .regionCount = 1,
        .pRegions    = &indexRegion,
    };
    vkCmdCopyBuffer2(frameCmd, &indexCopy);

    const VkMemoryBarrier2 barrier
    {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
                         VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_INDEX_READ_BIT |
                         VK_ACCESS_2_SHADER_READ_BIT,
    };
    const VkDependencyInfo depInfo {
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(frameCmd, &depInfo);

    frameQueue.push_function([staging, allocator]() mutable { staging.Destroy(allocator); });

    return mesh;
}

void VulkanMeshBuffer::Destroy(VmaAllocator allocator)
{
    if (m_VertexBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_VertexBuffer, m_VertexAllocation);
        m_VertexBuffer = VK_NULL_HANDLE;
    }
    if (m_IndexBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_IndexBuffer, m_IndexAllocation);
        m_IndexBuffer = VK_NULL_HANDLE;
    }
}

}
