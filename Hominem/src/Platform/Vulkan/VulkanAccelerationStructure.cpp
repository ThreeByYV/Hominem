#include "hmnpch.h"
#include "VulkanAccelerationStructure.h"
#include "VulkanRenderer.h"
#include "Hominem/Assets/MeshData.h"

namespace Hominem {

namespace {

struct ScratchBuffer
{
    VkBuffer        buffer  = VK_NULL_HANDLE;
    VmaAllocation   alloc   = VK_NULL_HANDLE;
    VkDeviceAddress address = 0;
};

VkDeviceAddress BufferAddress(VkDevice device, VkBuffer buffer)
{
    const VkBufferDeviceAddressInfo info
    {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    return vkGetBufferDeviceAddress(device, &info);
}

// Device-local buffer backing an acceleration structure's stored data.
void CreateAccelStorage(VmaAllocator allocator, VkDeviceSize size, VkBuffer& outBuffer, VmaAllocation& outAlloc)
{
    const VkBufferCreateInfo bufferInfo
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    const VmaAllocationCreateInfo allocInfo
    {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &outBuffer, &outAlloc, nullptr));
}

ScratchBuffer CreateScratch(VkDevice device, VmaAllocator allocator, VkDeviceSize size)
{
    const VkBufferCreateInfo bufferInfo
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    const VmaAllocationCreateInfo allocInfo
    {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };
    ScratchBuffer scratch;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &scratch.buffer, &scratch.alloc, nullptr));
    scratch.address = BufferAddress(device, scratch.buffer);
    return scratch;
}

// Barrier so a just-built structure is visible to the next build (BLAS -> TLAS) and to
// the ray query in the compute trace pass.
void AccelBuildBarrier(VkCommandBuffer cmd)
{
    const VkMemoryBarrier2 barrier
    {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
    };
    const VkDependencyInfo depInfo
    {
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &depInfo);
}

}

VkAccelerationStructureKHR VulkanAccelerationStructure::BuildMeshBlas(
    VulkanRenderer& renderer, VkCommandBuffer cmd, DeletionQueue& frameQueue, const VulkanMeshBuffer& mesh)
{
    auto device    = renderer.GetDevice();
    auto allocator = renderer.GetAllocator();

    const uint32_t triangleCount = mesh.GetIndexCount() / 3;

    const VkAccelerationStructureGeometryKHR geometry
    {
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry     =
        {
            .triangles =
            {
                .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData    = { .deviceAddress = mesh.GetVertexBufferAddress() },
                .vertexStride  = sizeof(StaticVertex),
                .maxVertex     = mesh.GetVertexCount() - 1,
                .indexType     = VK_INDEX_TYPE_UINT32,
                .indexData     = { .deviceAddress = mesh.GetIndexBufferAddress() },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo
    {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry,
    };

    VkAccelerationStructureBuildSizesInfoKHR sizes
    {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &triangleCount, &sizes);

    AccelBuffer blas;
    CreateAccelStorage(allocator, sizes.accelerationStructureSize, blas.buffer, blas.alloc);

    const VkAccelerationStructureCreateInfoKHR createInfo
    {
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blas.buffer,
        .size   = sizes.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &blas.handle));

    const ScratchBuffer scratch = CreateScratch(device, allocator, sizes.buildScratchSize);

    buildInfo.dstAccelerationStructure  = blas.handle;
    buildInfo.scratchData.deviceAddress = scratch.address;

    const VkAccelerationStructureBuildRangeInfoKHR range
    {
        .primitiveCount = triangleCount,
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    AccelBuildBarrier(cmd);

    const VkAccelerationStructureDeviceAddressInfoKHR addrInfo
    {
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas.handle,
    };
    blas.address = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);

    frameQueue.push_function([scratch, allocator]() { vmaDestroyBuffer(allocator, scratch.buffer, scratch.alloc); });

    m_Blas.push_back(blas);
    return blas.handle;
}

void VulkanAccelerationStructure::BuildTlas(
    VulkanRenderer& renderer, VkCommandBuffer cmd, DeletionQueue& frameQueue,
    const std::vector<AccelInstance>& instances)
{
    auto device    = renderer.GetDevice();
    auto allocator = renderer.GetAllocator();

    std::vector<VkAccelerationStructureInstanceKHR> gpuInstances;
    gpuInstances.reserve(instances.size());
    for (const auto& inst : instances)
    {
        const VkAccelerationStructureDeviceAddressInfoKHR addrInfo
        {
            .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = inst.blas,
        };

        VkAccelerationStructureInstanceKHR gpu
        {
            .instanceCustomIndex                    = inst.customIndex,
            .mask                                   = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference         = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo),
        };
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 4; col++)
                gpu.transform.matrix[row][col] = inst.transform[col][row]; // glm column-major -> row-major 3x4

        gpuInstances.push_back(gpu);
    }

    const VkDeviceSize instanceBytes = gpuInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    if (m_InstanceBuffer != VK_NULL_HANDLE)
        frameQueue.push_function([b = m_InstanceBuffer, a = m_InstanceAlloc, allocator]()
                                 { vmaDestroyBuffer(allocator, b, a); });

    const VkBufferCreateInfo instanceInfo
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = instanceBytes,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    const VmaAllocationCreateInfo instanceAlloc
    {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VmaAllocationInfo mapped {};
    VK_CHECK(vmaCreateBuffer(allocator, &instanceInfo, &instanceAlloc,
                             &m_InstanceBuffer, &m_InstanceAlloc, &mapped));
    memcpy(mapped.pMappedData, gpuInstances.data(), (size_t)instanceBytes);

    const VkAccelerationStructureGeometryKHR geometry
    {
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     =
        {
            .instances =
            {
                .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data            = { .deviceAddress = BufferAddress(device, m_InstanceBuffer) },
            },
        },
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo
    {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry,
    };

    const uint32_t instanceCount = (uint32_t)gpuInstances.size();
    VkAccelerationStructureBuildSizesInfoKHR sizes
    {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &instanceCount, &sizes);

    AccelBuffer tlas;
    CreateAccelStorage(allocator, sizes.accelerationStructureSize, tlas.buffer, tlas.alloc);

    const VkAccelerationStructureCreateInfoKHR createInfo
    {
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlas.buffer,
        .size   = sizes.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &tlas.handle));

    const ScratchBuffer scratch = CreateScratch(device, allocator, sizes.buildScratchSize);

    buildInfo.dstAccelerationStructure  = tlas.handle;
    buildInfo.scratchData.deviceAddress = scratch.address;

    const VkAccelerationStructureBuildRangeInfoKHR range
    {
        .primitiveCount = instanceCount,
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    AccelBuildBarrier(cmd);

    if (m_Tlas.handle != VK_NULL_HANDLE)
    {
        AccelBuffer old = m_Tlas;
        frameQueue.push_function([old, device, allocator]()
        {
            vkDestroyAccelerationStructureKHR(device, old.handle, nullptr);
            vmaDestroyBuffer(allocator, old.buffer, old.alloc);
        });
    }
    m_Tlas = tlas;

    frameQueue.push_function([scratch, allocator]() { vmaDestroyBuffer(allocator, scratch.buffer, scratch.alloc); });
}

void VulkanAccelerationStructure::Destroy(VkDevice device, VmaAllocator allocator)
{
    for (auto& blas : m_Blas)
    {
        vkDestroyAccelerationStructureKHR(device, blas.handle, nullptr);
        vmaDestroyBuffer(allocator, blas.buffer, blas.alloc);
    }
    m_Blas.clear();

    if (m_Tlas.handle != VK_NULL_HANDLE)
    {
        vkDestroyAccelerationStructureKHR(device, m_Tlas.handle, nullptr);
        vmaDestroyBuffer(allocator, m_Tlas.buffer, m_Tlas.alloc);
        m_Tlas = {};
    }
    if (m_InstanceBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_InstanceBuffer, m_InstanceAlloc);
        m_InstanceBuffer = VK_NULL_HANDLE;
    }
}

}
