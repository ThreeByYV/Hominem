#pragma once

#include "Hominem/Renderer/RenderFrame.h"
#include "VulkanCore.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

class VulkanRaytracer
{
public:
    VulkanRaytracer();
    ~VulkanRaytracer();

    VulkanRaytracer(const VulkanRaytracer&)            = delete;
    VulkanRaytracer& operator=(const VulkanRaytracer&) = delete;

    void Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID = {}, std::string preferredName = {});
    void Shutdown();

    void RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h);
    void RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity);

    void RunPasses(const std::vector<VulkanComputePass>& passes);

    HANDLE       GetDrawImageWin32Handle();
    HANDLE       GetComputeDoneSemaphoreWin32Handle(uint32_t frameIdx);
    VkDeviceSize GetDrawImageMemorySize()          const;
    VkExtent2D   GetDrawExtent()                   const;
    uint32_t     GetCurrentFrameIndex()            const;

    std::array<uint8_t, 8> GetDeviceLUID()         const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

}
