#pragma once

#include "Hominem/Renderer/RenderFrame.h"
#include "RaytracingContext.h"
#include "VulkanRenderer.h"
#include "VulkanStorageBuffer.h"
#include "VulkanMeshBuffer.h"
#include "VulkanShaderLibrary.h"
#include "VulkanAccelerationStructure.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Hominem {

class VulkanRaytracer
{
public:
    void Shutdown(DeletionQueue& mainQueue, VkDevice device, VmaAllocator allocator);

    // Build/replace the BLAS for a mesh after it is uploaded; invalidates the TLAS.
    void OnMeshUploaded(VulkanRenderer& renderer, VkCommandBuffer cmd,
                        VulkanHandle handle, const VulkanMeshBuffer& mesh);

    // Build the TLAS + per-instance geometry buffer from the frame's draws (cached until
    // a mesh changes). meshes is the scene renderer's mesh table, looked up by handle.
    void EnsureScene(
        VulkanRenderer& renderer, VkCommandBuffer cmd,
        const std::vector<VulkanMeshDraw>& draws,
        const std::unordered_map<VulkanHandle, VulkanMeshBuffer>& meshes
    );

    bool HasScene() const { return m_TlasBuilt; }

    RaytracingContext MakeContext(
        VulkanRenderer& renderer,
        VkCommandBuffer cmd, VkDeviceAddress scene,
        VulkanShaderLibrary& shaders) const;

private:
    VulkanAccelerationStructure                                  m_Accel;
    std::unordered_map<VulkanHandle, VkAccelerationStructureKHR> m_MeshBlas;
    bool                                                         m_TlasBuilt = false;

    VulkanStorageBuffer m_InstanceGeomBuffer;
    bool                m_InstanceGeomCreated  = false;
    uint32_t            m_InstanceGeomCapacity = 0;
    VkDeviceAddress     m_InstanceGeomAddress  = 0;
};

}
