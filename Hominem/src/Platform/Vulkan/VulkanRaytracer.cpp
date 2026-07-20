#include "hmnpch.h"
#include "VulkanRaytracer.h"

#include <glm/glm.hpp>

namespace Hominem {

namespace {

// Per-instance geometry a hit shader reads to fetch attributes + material, indexed by
// the ray query's gl_InstanceCustomIndexEXT.
struct InstanceGeomEntry
{
    VkDeviceAddress vertexAddress;
    VkDeviceAddress indexAddress;
    glm::vec4       baseColor;
};

}

void VulkanRaytracer::Shutdown(DeletionQueue& mainQueue, VkDevice device, VmaAllocator allocator)
{
    // Deferred, not called directly: the caller flushes this queue only after its own
    // vkDeviceWaitIdle, so these are guaranteed safe to destroy by the time they run.
    mainQueue.push_function([this, device, allocator]() { m_Accel.Destroy(device, allocator); });
    if (m_InstanceGeomCreated)
        mainQueue.push_function([this, allocator]() { m_InstanceGeomBuffer.Destroy(allocator); });
}

RaytracingContext VulkanRaytracer::MakeContext(VulkanRenderer& renderer, VkCommandBuffer cmd,
                                               VkDeviceAddress scene, VulkanShaderLibrary& shaders) const
{
    return { renderer, cmd, m_Accel.GetTlas(), m_InstanceGeomAddress, scene, shaders };
}

void VulkanRaytracer::OnMeshUploaded(VulkanRenderer& renderer, VkCommandBuffer cmd,
                                     VulkanHandle handle, const VulkanMeshBuffer& mesh)
{
    m_MeshBlas[handle] = m_Accel.BuildMeshBlas(renderer, cmd, renderer.GetFrameDeletionQueue(), mesh);
    m_TlasBuilt = false;
}

void VulkanRaytracer::EnsureScene(VulkanRenderer& renderer, VkCommandBuffer cmd,
                                  const std::vector<VulkanMeshDraw>& draws,
                                  const std::unordered_map<VulkanHandle, VulkanMeshBuffer>& meshes)
{
    if (m_TlasBuilt || draws.empty()) return;

    auto device    = renderer.GetDevice();
    auto allocator = renderer.GetAllocator();

    std::vector<AccelInstance>     instances;
    std::vector<InstanceGeomEntry> geom(draws.size());
    instances.reserve(draws.size());
    for (uint32_t i = 0; i < draws.size(); i++)
    {
        auto meshIt = meshes.find(draws[i].mesh);
        auto blasIt = m_MeshBlas.find(draws[i].mesh);
        if (meshIt == meshes.end() || blasIt == m_MeshBlas.end()) continue;

        instances.push_back({ draws[i].transform, blasIt->second, i });
        geom[i] = {
            meshIt->second.GetVertexBufferAddress(),
            meshIt->second.GetIndexBufferAddress(),
            draws[i].baseColor,
        };
    }
    if (instances.empty()) return;

    m_Accel.BuildTlas(renderer, cmd, renderer.GetFrameDeletionQueue(), instances);

    const uint32_t geomBytes = (uint32_t)(geom.size() * sizeof(InstanceGeomEntry));
    if (!m_InstanceGeomCreated || geomBytes > m_InstanceGeomCapacity)
    {
        if (m_InstanceGeomCreated)
            renderer.GetFrameDeletionQueue().push_function(
                [b = m_InstanceGeomBuffer, allocator]() mutable { b.Destroy(allocator); });

        m_InstanceGeomCapacity = std::max(geomBytes, 1u);
        m_InstanceGeomBuffer   = VulkanStorageBuffer::Create(allocator, m_InstanceGeomCapacity,
                                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_InstanceGeomCreated  = true;
    }
    m_InstanceGeomBuffer.Upload(geom.data(), geomBytes);
    m_InstanceGeomAddress = m_InstanceGeomBuffer.GetDeviceAddress(device);

    m_TlasBuilt = true;
}

}
