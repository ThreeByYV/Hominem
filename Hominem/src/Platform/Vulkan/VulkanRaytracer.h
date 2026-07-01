#pragma once

#include "Hominem/Renderer/RenderFrame.h"
#include "VulkanRenderer.h"
#include "VulkanComputeShader.h"
#include "VulkanStorageBuffer.h"
#include "VulkanRenderTarget.h"

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

    void RunPasses(const std::vector<VulkanComputePass>& passes);

    HANDLE       GetDrawImageWin32Handle()                        { return m_Renderer->GetDrawImageWin32Handle(); }
    HANDLE       GetComputeDoneSemaphoreWin32Handle(uint32_t i)  { return m_Renderer->GetComputeDoneSemaphoreWin32Handle(i); }
    VkDeviceSize GetDrawImageMemorySize()                  const { return m_Renderer->GetDrawImageMemorySize(); }
    VkExtent2D   GetDrawExtent()                           const { return m_Renderer->GetDrawExtent(); }
    uint32_t     GetCurrentFrameIndex()                    const { return m_Renderer->GetCurrentFrameIndex(); }
    std::array<uint8_t, 8> GetDeviceLUID()                 const { return m_Renderer->GetDeviceLUID(); }

private:
    struct RTSlot { VulkanRenderTarget rt; bool needsTransition = true; bool valid = false; };
    struct BufSlot { VulkanStorageBuffer buf; bool valid = false; };

    std::unique_ptr<VulkanRenderer>                      m_Renderer;
    std::unordered_map<std::string, VulkanComputeShader> m_Shaders;
    std::vector<RTSlot>                                  m_RenderTargets;
    std::vector<BufSlot>                                 m_Buffers;
};

}
