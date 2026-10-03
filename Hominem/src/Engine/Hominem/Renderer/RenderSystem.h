#pragma once

#include "Hominem/Renderer/SceneRenderer.h"
#include "Hominem/Renderer/Frame/RenderFrame.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Hominem {

class VulkanSceneRenderer;
class GpuInterop;

class RenderSystem
{
public:
    RenderSystem();
    ~RenderSystem();

    RenderSystem(const RenderSystem&)            = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    void Init(uint32_t w, uint32_t h);
    void Shutdown();
    void ExecuteFrame(RecordedFrame& frame);

    void RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h);
    void RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity);

    SceneRenderer& GetSceneRenderer() { return m_SceneRenderer; }

private:
    SceneRenderer                        m_SceneRenderer;
    std::unique_ptr<VulkanSceneRenderer> m_VulkanRenderer;
    std::unique_ptr<GpuInterop>          m_Interop;
};

}
