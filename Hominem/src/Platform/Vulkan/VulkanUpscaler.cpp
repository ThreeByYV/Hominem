#include "hmnpch.h"
#include "VulkanUpscaler.h"

#include "Hominem/Renderer/Frame/RenderGraph.h"
#include "Hominem/Renderer/RHI/SharedResources.h"

namespace Hominem {

namespace {

constexpr const char* k_Output = "vk_upscale_out";
constexpr const char* k_Depth  = "vk_upscale_depth"; // scene depth as R32F; D24S8 doesn't share

}

VulkanUpscaler::~VulkanUpscaler()
{
    m_Strategy.reset(); // releases its SDK resources before the bridge images go
    m_Bridge.Shutdown();
}

const char* VulkanUpscaler::GetName() const
{
    return m_Strategy ? m_Strategy->GetName() : "Vulkan passthrough (debug)";
}

bool VulkanUpscaler::IsSupported() const
{
    return !m_Strategy || m_Strategy->IsAvailable();
}

UpscalerCaps VulkanUpscaler::GetCaps() const
{
    if (StrategyActive()) return m_Strategy->GetCaps();
    // A strategy switched off is native resolution with no AA.
    if (m_Strategy) return { .temporal = false, .minRenderScale = 1.f, .maxRenderScale = 1.f };
    return { .temporal = false };
}

float VulkanUpscaler::GetRenderScale(float requested, glm::uvec2 outputSize) const
{
    if (StrategyActive()) return m_Strategy->GetRenderScale(outputSize);
    return Upscaler::GetRenderScale(requested, outputSize);
}

void VulkanUpscaler::Init()
{
    m_DepthCopyShader = ShaderLibrary::Engine()->Load("engine://Shaders/upscale_depth.glsl");
    m_Bridge.Init(m_Vulkan, m_GL);
}

void VulkanUpscaler::DeclareResources(RenderGraph& graph)
{
    graph.AddFBO(k_Output, FramebufferFormat::RGBA16F, 1.0f, 1, RenderGraph::Resolution::Output);
    graph.AddFBO(k_Depth,  FramebufferFormat::R32F,    1.0f, 1, RenderGraph::Resolution::Render);
    graph.SetAlias(OutputTarget, k_Output);
}

void VulkanUpscaler::BeginFrame(RenderGraph&)
{
    if (m_Strategy) m_Strategy->PollDebugKeys();
}

void VulkanUpscaler::Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd)
{
    const auto output = graph.GetFBO(k_Output);
    const auto depth  = graph.GetFBO(k_Depth);
    if (!output || !depth || !m_DepthCopyShader || in.renderSize.x == 0 || in.renderSize.y == 0) return;

    // Depth into a color target the bridge can share; the pass bound the scene depth at Slot::Depth.
    cmd.BindFramebuffer(depth->GetRendererID());
    cmd.SetViewport(0, 0, in.renderSize.x, in.renderSize.y);
    cmd.BindShader(m_DepthCopyShader);
    cmd.SetInt(m_DepthCopyShader, "u_Depth", (int)Slot::Depth);
    cmd.DrawFullscreenTriangle();

    // Sizes and IDs are read here, when the commands execute: the graph resizes between
    // recording and execution, and a resize recreates every GL object in the target.
    const bool active = StrategyActive();
    cmd.Invoke([this, in, output, depth, active]()
    {
        using B = VulkanUpscalerBridge;
        const auto& sceneSpec = in.sceneTarget->GetSpecification();
        const auto& outSpec   = output->GetSpecification();
        const glm::uvec2 rs { sceneSpec.Width, sceneSpec.Height };
        const glm::uvec2 os { outSpec.Width,   outSpec.Height };
        m_Bridge.Resize(rs, os);

        // Flipped so the Vulkan side sees the frame upright (row 0 at the top); depth was
        // already flipped by the copy shader.
        m_GL.CopyTexture(in.sceneTarget->GetColorAttachmentRendererID(0), m_Bridge.GLTexture(B::Color),    rs.x, rs.y, true);
        m_GL.CopyTexture(in.sceneTarget->GetColorAttachmentRendererID(1), m_Bridge.GLTexture(B::Velocity), rs.x, rs.y, true);
        m_GL.CopyTexture(depth->GetColorAttachmentRendererID(0),          m_Bridge.GLTexture(B::Depth),    rs.x, rs.y);

        m_Bridge.Run([&](VkCommandBuffer vkCmd)
        {
            // Off, out of range or failed: the frame as rendered, no AA.
            if (!active || !m_Strategy->Evaluate(vkCmd, m_Bridge, in))
                m_Bridge.RecordBlit(vkCmd);
        });

        m_GL.CopyTexture(m_Bridge.GLTexture(B::Output), output->GetColorAttachmentRendererID(0), os.x, os.y, true);
    });
}

}
