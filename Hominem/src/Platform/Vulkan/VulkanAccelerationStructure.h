#pragma once

#include "VulkanCore.h"
#include "VulkanMeshBuffer.h"

#include <cstdint>
#include <vector>

namespace Hominem {

class VulkanRenderer;

// One mesh instance placed into the TLAS: its transform and which BLAS it uses.
struct AccelInstance
{
    glm::mat4          transform { 1.f };
    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    uint32_t           customIndex  = 0; // surfaces as gl_InstanceCustomIndexEXT in the ray query
};

// Owns a bottom-level structure per mesh and a single top-level structure over the
// instances. Built with a one-time immediate submit; rebuilt only when geometry changes.
class VulkanAccelerationStructure
{
public:
    void Destroy(VkDevice device, VmaAllocator allocator);

    // Build/replace the BLAS for one mesh. Returns its handle for use in AccelInstance.
    VkAccelerationStructureKHR BuildMeshBlas(VulkanRenderer& renderer, VkCommandBuffer cmd,
                                             DeletionQueue& frameQueue, const VulkanMeshBuffer& mesh);

    // Build/replace the TLAS over the given instances.
    void BuildTlas(VulkanRenderer& renderer, VkCommandBuffer cmd, DeletionQueue& frameQueue,
                   const std::vector<AccelInstance>& instances);

    VkAccelerationStructureKHR GetTlas() const { return m_Tlas.handle; }
    bool                       HasTlas() const { return m_Tlas.handle != VK_NULL_HANDLE; }

private:
    struct AccelBuffer
    {
        VkAccelerationStructureKHR handle  = VK_NULL_HANDLE;
        VkBuffer                   buffer  = VK_NULL_HANDLE;
        VmaAllocation              alloc   = VK_NULL_HANDLE;
        VkDeviceAddress            address = 0;
    };

    std::vector<AccelBuffer> m_Blas;
    AccelBuffer              m_Tlas;
    VkBuffer                 m_InstanceBuffer = VK_NULL_HANDLE;
    VmaAllocation            m_InstanceAlloc  = VK_NULL_HANDLE;
};

}
