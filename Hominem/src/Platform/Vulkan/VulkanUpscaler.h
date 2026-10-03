#pragma once

#include "IUpscalerStrategy.h"
#include "Platform/Interop/InteropRoundTrip.h"

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/RHI/Shader.h"

namespace Hominem {

class GpuInterop;

/// Upscales in Vulkan while the rest of the frame is GL: copies the scene into a round trip,
/// runs the strategy mid-frame, and copies the result into OutputTarget. With no strategy
/// (passthrough) or the strategy switched off, it blits.
class VulkanUpscaler final : public Upscaler
{
public:
    /// `strategy` may be null: a plain bilinear blit, for testing the handoff without an SDK.
    VulkanUpscaler(GpuInterop& interop, Scope<IUpscalerStrategy> strategy)
        : m_Interop(interop), m_Strategy(std::move(strategy)) {}
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

    // Evaluate's steps. ConvertDepth records; the rest run when the commands execute.
    void ConvertDepth(CommandList& cmd, const Ref<Framebuffer>& depth, glm::uvec2 renderSize);
    void Upscale(const UpscalerInputs& in, const Ref<Framebuffer>& depth, const Ref<Framebuffer>& output, bool active);
    void ResizeImages(glm::uvec2 renderSize, glm::uvec2 outputSize);
    void UploadInputs(const UpscalerInputs& in, const Ref<Framebuffer>& depth);
    void RunStrategy(const UpscalerInputs& in, glm::uvec2 renderSize, glm::uvec2 outputSize, bool active);

    GpuInterop&              m_Interop;
    Scope<IUpscalerStrategy> m_Strategy;
    Scope<InteropRoundTrip>  m_RoundTrip;
    Ref<Shader>              m_DepthCopyShader;
};

}
