#pragma once

#include <string>
#include <functional>

#include "RenderGraph.h"
#include "AutoExposure.h"
#include "RenderFrame.h"

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

    /// Records every pass into a CommandList per pass. You should call it from the main thread.
    std::vector<CommandList> Record(const RenderFrame& frame) { return m_RenderGraph.Record(frame); }

    Ref<Framebuffer> GetFBO(const std::string& name) { return m_RenderGraph.GetFBO(name); }
    RenderGraph&     GetRenderGraph()                { return m_RenderGraph; }

private:
    void SetupPasses();

    void GeometryPass      (const RenderFrame& frame, CommandList& cmd);
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

    uint32_t m_SharedVkTexture = 0;
};

}
