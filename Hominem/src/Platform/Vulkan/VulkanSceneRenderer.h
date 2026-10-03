#pragma once

#include "Hominem/Renderer/Frame/RenderFrame.h"
#include "VulkanRenderer.h"
#include "VulkanComputePipeline.h"
#include "VulkanStorageBuffer.h"
#include "VulkanRenderTarget.h"
#include "VulkanMeshBuffer.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanShaderLibrary.h"
#include "VulkanRaytracer.h"
#include "VulkanRayTracingDDGI.h"
#include "VulkanSharedImages.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

class VulkanSceneRenderer
{
public:
    VulkanSceneRenderer()  = default;
    ~VulkanSceneRenderer() = default;

    VulkanRenderer& GetRenderer() { return *m_Renderer; }

    VulkanSceneRenderer(const VulkanSceneRenderer&)            = delete;
    VulkanSceneRenderer& operator=(const VulkanSceneRenderer&) = delete;

    void Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID = {}, std::string preferredName = {});
    void Shutdown();

    void RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h);
    void RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity);

    void RunFrame(const std::vector<VulkanMeshUpload>& uploads,
                  const std::vector<VulkanComputePass>& computePasses,
                  const std::vector<VulkanMeshDraw>& draws,
                  const std::vector<VulkanSphereInstance>& debugSpheres,
                  const VulkanDDGIParams& ddgi,
                  const VulkanSceneView& view);

    /// Poll this before CollectSharedImages — that one mints Win32 handles the caller owns.
    uint32_t GetSharedImageGeneration() const { return m_SharedImages.GetGeneration(); }

    std::vector<SharedImageExport> CollectSharedImages() const
    {
        return m_SharedImages.Collect(m_Renderer->GetDevice());
    }

    std::array<uint8_t, 8> GetDeviceLUID() const { return m_Renderer->GetDeviceLUID(); }

private:
    struct RenderTargetSlot { VulkanRenderTarget renderTarget; bool needsTransition = true; bool valid = false; };
    struct BufSlot { VulkanStorageBuffer buf; bool valid = false; };

    void UploadMeshes(VkCommandBuffer cmd, const std::vector<VulkanMeshUpload>& uploads);
    void RunComputePasses(VkCommandBuffer cmd, const std::vector<VulkanComputePass>& passes);
    void RunScenePass(VkCommandBuffer cmd, const std::vector<VulkanMeshDraw>& draws,
                      const std::vector<VulkanSphereInstance>& debugSpheres,
                      const VulkanDDGIParams& ddgi,
                      const VulkanSceneView& view);

    void DrawDebugSpheres(VkCommandBuffer cmd, const std::vector<VulkanSphereInstance>& spheres,
                          VkDeviceAddress sceneAddress);
    void DrawSphereInstances(VkCommandBuffer cmd, VkDeviceAddress instanceAddress,
                             uint32_t count, VkDeviceAddress sceneAddress);

    VulkanGraphicsPipeline& GetOrCreateScenePipeline();
    VulkanGraphicsPipeline& GetOrCreateDebugSpherePipeline();
    void EnsureDebugSphereMesh(VkCommandBuffer cmd);
    void EnsureMeshDescriptors(VkCommandBuffer cmd);

    std::unique_ptr<VulkanRenderer>                         m_Renderer;
    VulkanRaytracer                                         m_Raytracer;
    VulkanRayTracingDDGI                                    m_DDGI;
    VulkanSharedImages                                      m_SharedImages;

    std::unordered_map<std::string, VulkanComputePipeline>  m_ComputePipelines;
    std::vector<RenderTargetSlot>                           m_RenderTargets;
    std::vector<BufSlot>                                    m_Buffers;

    std::unordered_map<VulkanHandle, VulkanMeshBuffer>      m_Meshes;
    std::unordered_map<std::string, VulkanGraphicsPipeline> m_GraphicsPipelines;
    VulkanShaderLibrary                                     m_ShaderLibrary;

    std::array<VulkanStorageBuffer, 2>                      m_SceneBuffers;
    bool                                                    m_SceneBuffersCreated = false;

    // Single combined-image-sampler set feeding the mesh shader the DDGI atlases.
    VkDescriptorSetLayout          m_MeshSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool               m_MeshDescPool  = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> m_MeshDescSets { VK_NULL_HANDLE, VK_NULL_HANDLE };
    bool                           m_MeshDescReady = false;
    VulkanTexture                  m_DummyAtlas;
    bool                           m_DummyAtlasCreated = false;

    VulkanMeshBuffer                   m_DebugSphereMesh;
    bool                               m_DebugSphereMeshReady = false;
    std::array<VulkanStorageBuffer, 2> m_DebugSphereBuffers;
    bool                               m_DebugSphereBuffersCreated = false;
    uint32_t                           m_DebugSphereCapacity = 0;
};

}
