#pragma once

#include "SceneRenderer.h"
#include "RenderFrame.h"
#include "SharedResources.h"
#include <array>
#include <cstdint>
#include <memory>

namespace Hominem {

class VulkanRaytracer;

class Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Init(uint32_t w, uint32_t h);
    void Shutdown();
    void ExecuteFrame(RecordedFrame& frame);

    void RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h);
    void RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity);

    SceneRenderer& GetSceneRenderer() { return m_SceneRenderer; }

private:
    void SetupInterop(uint32_t w, uint32_t h, const std::array<uint8_t, 8>& glLUID);

    SceneRenderer                     m_SceneRenderer;
    std::unique_ptr<VulkanRaytracer>  m_VulkanRaytracer;
    std::unique_ptr<SharedResources>  m_SharedResources;
    uint32_t                          m_VkFrameIdx = 0;
};

}
