#pragma once

#include "Hominem/Renderer/RenderFrame.h"
#include "VulkanRenderer.h"
#include "VulkanComputePipeline.h"
#include "VulkanStorageBuffer.h"
#include "VulkanRenderTarget.h"
#include "VulkanMeshBuffer.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanShaderLibrary.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

class VulkanRaytracer
{
public:
    VulkanRaytracer()  = default;
    ~VulkanRaytracer() = default;

    VulkanRaytracer(const VulkanRaytracer&)            = delete;
    VulkanRaytracer& operator=(const VulkanRaytracer&) = delete;

    void Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID = {}, std::string preferredName = {});
    void Shutdown();

    void RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h);
    void RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity);

    void RunFrame(const std::vector<VulkanMeshUpload>& uploads,
                  const std::vector<VulkanComputePass>& computePasses,
                  const std::vector<VulkanMeshDraw>& draws,
                  const std::vector<VulkanSphereInstance>& debugSpheres,
                  const VulkanSceneView& view);

    HANDLE       GetDrawImageWin32Handle()                        { return m_Renderer->GetDrawImageWin32Handle(); }
    HANDLE       GetComputeDoneSemaphoreWin32Handle(uint32_t i)  { return m_Renderer->GetComputeDoneSemaphoreWin32Handle(i); }
    HANDLE       GetGLDoneSemaphoreWin32Handle()                 { return m_Renderer->GetGLDoneSemaphoreWin32Handle(); }
    VkDeviceSize GetDrawImageMemorySize()                  const { return m_Renderer->GetDrawImageMemorySize(); }
    VkExtent2D   GetDrawExtent()                           const { return m_Renderer->GetDrawExtent(); }
    uint32_t     GetCurrentFrameIndex()                    const { return m_Renderer->GetCurrentFrameIndex(); }
    std::array<uint8_t, 8> GetDeviceLUID()                 const { return m_Renderer->GetDeviceLUID(); }

private:
    struct RTSlot { VulkanRenderTarget rt; bool needsTransition = true; bool valid = false; };
    struct BufSlot { VulkanStorageBuffer buf; bool valid = false; };

    void UploadMeshes(VkCommandBuffer cmd, const std::vector<VulkanMeshUpload>& uploads);
    void RunComputePasses(VkCommandBuffer cmd, const std::vector<VulkanComputePass>& passes);
    void RunScenePass(VkCommandBuffer cmd, const std::vector<VulkanMeshDraw>& draws,
                      const std::vector<VulkanSphereInstance>& debugSpheres,
                      const VulkanSceneView& view);

    void DrawDebugSpheres(VkCommandBuffer cmd, const std::vector<VulkanSphereInstance>& spheres,
                          VkDeviceAddress sceneAddress);

    VulkanGraphicsPipeline& GetOrCreateScenePipeline();
    VulkanGraphicsPipeline& GetOrCreateDebugSpherePipeline();
    void EnsureDebugSphereMesh(VkCommandBuffer cmd);

    std::unique_ptr<VulkanRenderer>                         m_Renderer;
    std::unordered_map<std::string, VulkanComputePipeline>  m_ComputePipelines;
    std::vector<RTSlot>                                     m_RenderTargets;
    std::vector<BufSlot>                                    m_Buffers;

    std::unordered_map<VulkanHandle, VulkanMeshBuffer>      m_Meshes;
    std::unordered_map<std::string, VulkanGraphicsPipeline> m_GraphicsPipelines;
    VulkanShaderLibrary                                     m_ShaderLibrary;
    std::array<VulkanStorageBuffer, 2>                      m_SceneBuffers;
    bool                                                    m_SceneBuffersCreated = false;

    VulkanMeshBuffer                   m_DebugSphereMesh;
    bool                               m_DebugSphereMeshReady = false;
    std::array<VulkanStorageBuffer, 2> m_DebugSphereBuffers;
    bool                               m_DebugSphereBuffersCreated = false;
    uint32_t                           m_DebugSphereCapacity = 0;
};

}
