#pragma once

#include <atomic>
#include <string>
#include <functional>

#include "RenderGraph.h"
#include "AutoExposure.h"
#include "RenderFrame.h"
#include "SharedImages.h"

namespace Hominem {

// Blackboard output types — published by passes, consumed by downstream passes.
// todo : add ShadowOutput (ESM shadow map depth ID + light-space matrix) when shadow mapping is implemented.
// todo: add DDGIOutput (irradiance volume texture + probe grid params) when DDGI is implemented.
struct ExposureOutput { float value; };

class SceneRenderer
{
public:
    void Init();

    void Shutdown();

    void SetImGuiCallbacks(std::function<void()> waitFn, std::function<void()> notifyFn);

    void SetSharedVulkanTexture(uint32_t texID) { m_SharedVkTexture = texID; }

    /// GL textures over Vulkan's published results. Swapped by the render thread when they
    /// change, read by name while recording passes on the main thread.
    void SetSharedImages(std::shared_ptr<const SharedImageTable> table)
    {
        m_SharedImages.store(std::move(table), std::memory_order_release);
    }

    /// Applies this frame's TAA jitter. Mutates the frame's 3D camera matrices, so call it
    /// on the main thread after every layer has built the frame and before Record().
    void PrepareTemporal(RenderFrame& frame);

    /// Drops the accumulated history. Call on camera cuts — blending across one smears the
    /// old shot into the new.
    void ResetTemporalHistory() { m_TAAResetPending = true; }

    /// Records every pass into a CommandList per pass. You should call it from the main thread.
    std::vector<CommandList> Record(const RenderFrame& frame);

    Ref<Framebuffer> GetFBO(const std::string& name) { return m_RenderGraph.GetFBO(name); }
    RenderGraph&     GetRenderGraph()                { return m_RenderGraph; }

private:
    void SetupPasses();

    void GeometryPass      (const RenderFrame& frame, CommandList& cmd);
    void TAAResolvePass    (const RenderFrame& frame, CommandList& cmd);
    void ImGuiPass         (const RenderFrame& frame, CommandList& cmd);
    void AutoExposurePass  (const RenderFrame& frame, CommandList& cmd);
    void BloomThresholdPass(const RenderFrame& frame, CommandList& cmd);
    void BloomBlurHPass    (const RenderFrame& frame, CommandList& cmd);
    void BloomBlurVPass    (const RenderFrame& frame, CommandList& cmd);
    void CompositePass     (const RenderFrame& frame, CommandList& cmd);
    void VulkanBlitPass    (const RenderFrame& frame, CommandList& cmd);

    RenderGraph  m_RenderGraph;
    AutoExposure m_AutoExposure;

    std::function<void()> m_WaitImGui;
    std::function<void()> m_NotifyImGui;

    Ref<Shader> m_ThresholdShader;
    Ref<Shader> m_BlurShader;
    Ref<Shader> m_CompositeShader;
    Ref<Shader> m_SkyboxShader;
    Ref<Shader> m_FireQuadShader;
    Ref<Shader> m_SmokeQuadShader;
    Ref<Shader> m_VkBlitShader;
    Ref<Shader> m_TAAResolveShader;

    // Main thread only — PrepareTemporal and Record.
    glm::mat4 m_PrevViewProjection { 1.f };
    uint32_t  m_TAAFrameIndex   = 0;
    uint32_t  m_TAAHistoryIdx   = 0;
    uint32_t  m_TAALastRenderW  = 0;
    uint32_t  m_TAALastRenderH  = 0;
    bool      m_TAAResetPending = true;

    uint32_t m_SharedVkTexture = 0;

    std::atomic<std::shared_ptr<const SharedImageTable>> m_SharedImages;
};

}
