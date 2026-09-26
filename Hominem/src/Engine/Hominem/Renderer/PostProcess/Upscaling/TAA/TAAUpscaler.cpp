#include "hmnpch.h"
#include "TAAUpscaler.h"
#include "Hominem/Core/Profiler.h"

#include "Hominem/Renderer/Frame/RenderGraph.h"
#include "Hominem/Renderer/Frame/RenderSettings.h"

namespace Hominem {

namespace {

// Ping-pong: this frame's output is next frame's history.
constexpr const char* k_Targets[2] = { "taa_0", "taa_1" };
constexpr const char* k_History    = "taa_history";

}

void TAAUpscaler::Init()
{
    m_ResolveShader = ShaderLibrary::Engine()->Load("engine://Shaders/taa_resolve.glsl");
}

void TAAUpscaler::DeclareResources(RenderGraph& graph)
{
    graph.AddFBO(k_Targets[0], FramebufferFormat::RGBA16F, 1.0f);
    graph.AddFBO(k_Targets[1], FramebufferFormat::RGBA16F, 1.0f);
    graph.SetAlias(OutputTarget, k_Targets[0]);
    graph.SetAlias(k_History,    k_Targets[1]);
}

void TAAUpscaler::BeginFrame(RenderGraph& graph)
{
    m_HistoryIdx ^= 1u;
    graph.SetAlias(OutputTarget, k_Targets[m_HistoryIdx]);
    graph.SetAlias(k_History,    k_Targets[m_HistoryIdx ^ 1u]);
}

void TAAUpscaler::Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();

    const auto history = graph.GetFBO(k_History);
    if (!m_ResolveShader || !history || in.renderSize.x == 0 || in.renderSize.y == 0) return;

    cmd.BindTexture((uint32_t)Slot::Color1, history->GetColorAttachmentRendererID(0));

    cmd.BindShader(m_ResolveShader);
    cmd.SetInt(m_ResolveShader, "u_Current",  (int)Slot::Color0);
    cmd.SetInt(m_ResolveShader, "u_History",  (int)Slot::Color1);
    cmd.SetInt(m_ResolveShader, "u_Velocity", (int)Slot::Color2);
    cmd.SetInt(m_ResolveShader, "u_Depth",    (int)Slot::Depth);
    cmd.SetInt(m_ResolveShader, "u_UseVelocity", RenderSettings::TAAVelocity ? 1 : 0);

    // xy = 1/size, zw = size (CommandList has no SetFloat2).
    const glm::vec2 size = in.renderSize;
    cmd.SetFloat4(m_ResolveShader, "u_TexelSize", glm::vec4(1.f / size, size));

    cmd.SetMat4(m_ResolveShader, "u_InvViewProj",  glm::inverse(in.viewProjUnjittered));
    cmd.SetMat4(m_ResolveShader, "u_PrevViewProj", in.prevViewProj);

    const float feedbackMax = glm::clamp(RenderSettings::TAAFeedbackMax, 0.f, 0.99f);
    cmd.SetFloat(m_ResolveShader, "u_FeedbackMin",
                 glm::clamp(RenderSettings::TAAFeedbackMin, 0.f, feedbackMax));
    cmd.SetFloat(m_ResolveShader, "u_FeedbackMax", feedbackMax);
    cmd.SetInt(m_ResolveShader, "u_Reset",       in.reset ? 1 : 0);
    cmd.SetInt(m_ResolveShader, "u_UseDilation", RenderSettings::TAADilation ? 1 : 0);
    cmd.SetInt(m_ResolveShader, "u_DebugView",   RenderSettings::TAADebugView);

    cmd.DrawFullscreenTriangle();

    cmd.BindTexture((uint32_t)Slot::Color1, 0);
}

}
