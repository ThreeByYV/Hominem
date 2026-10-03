#pragma once

#include "VulkanUpscalerBridge.h"
#include "IUpscalerStrategy.h"

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/RHI/Shader.h"

namespace Hominem {

/// Upscales in Vulkan while the rest of the frame is GL: copies the scene into the bridge,
/// runs the strategy upscaler mid-frame, and copies the result into OutputTarget. With no
/// strategy (passthrough) or the strategy switched off, it blits.
class VulkanUpscaler final : public Upscaler
{
public:
    /// `strategy` may be null: a plain bilinear blit, for testing the handoff without an SDK.
    VulkanUpscaler(VulkanRenderer& vk, SharedResources& gl, Scope<IUpscalerStrategy> strategy)
        : m_Vulkan(vk), m_GL(gl), m_Strategy(std::move(strategy)) {}
    ~VulkanUpscaler() override;

    const char*  GetName() const override;
    bool         IsSupported() const override;
    UpscalerCaps GetCaps() const override;
    float        GetRenderScale(float requested, glm::uvec2 outputSize) const override;

    void Init() override;
    void DeclareResources(RenderGraph& graph) override;
    void BeginFrame(RenderGraph& graph) override;
    void Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd) override;

private:
    bool StrategyActive() const { return m_Strategy && m_Strategy->IsEnabled(); }

    VulkanRenderer&       m_Vulkan;
    SharedResources&      m_GL;
    Scope<IUpscalerStrategy> m_Strategy;
    VulkanUpscalerBridge  m_Bridge;
    Ref<Shader>           m_DepthCopyShader;
};

}
