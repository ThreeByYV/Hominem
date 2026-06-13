#pragma once

#include <string>
#include <functional>

#include "RenderGraph.h"
#include "AutoExposure.h"
#include "RenderFrame.h"

namespace Hominem {

class SceneRenderer
{
public:
    void Init();

    void Shutdown();

    void SetImGuiCallbacks(std::function<void()> waitFn, std::function<void()> notifyFn);

    void RenderScene(const RenderFrame& frame);

    Ref<Framebuffer> GetFBO(const std::string& name) { return m_RenderGraph.GetFBO(name); }

private:
    void SetupPasses();

    void GeometryPass      (const RenderFrame& frame);
    void ImGuiPass         (const RenderFrame& frame);
    void AutoExposurePass  (const RenderFrame& frame);
    void BloomThresholdPass(const RenderFrame& frame);
    void BloomBlurHPass    (const RenderFrame& frame);
    void BloomBlurVPass    (const RenderFrame& frame);
    void CompositePass     (const RenderFrame& frame);

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
};

}
