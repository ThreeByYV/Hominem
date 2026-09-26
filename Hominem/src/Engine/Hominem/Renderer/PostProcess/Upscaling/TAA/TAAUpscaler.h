#pragma once

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/RHI/Shader.h"
#include "Hominem/Renderer/PostProcess/Upscaling/Upscaler.h"

namespace Hominem {

/// Built-in TAA (taa_resolve.glsl). Resolves at render resolution; does not upscale.
class TAAUpscaler final : public Upscaler
{
public:
    const char*  GetName() const override     { return "TAA"; }
    bool         IsSupported() const override { return true; }
    UpscalerCaps GetCaps() const override     { return {}; }

    void Init() override;
    void DeclareResources(RenderGraph& graph) override;
    void BeginFrame(RenderGraph& graph) override;
    void Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd) override;

private:
    Ref<Shader> m_ResolveShader;
    uint32_t    m_HistoryIdx = 0;
};

}
