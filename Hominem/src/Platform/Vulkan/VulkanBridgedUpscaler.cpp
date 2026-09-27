#include "hmnpch.h"
#include "VulkanBridgedUpscaler.h"

#include "Hominem/Renderer/Frame/RenderGraph.h"
#include "Hominem/Renderer/RHI/SharedResources.h"

namespace Hominem {

namespace {

constexpr const char* k_Output = "vk_upscale_out";
constexpr const char* k_Depth  = "vk_upscale_depth"; // scene depth as R32F; D24S8 doesn't share

}

VulkanBridgedUpscaler::~VulkanBridgedUpscaler()
{
    m_Bridge.Shutdown();
}

void VulkanBridgedUpscaler::Init()
{
    m_DepthCopyShader = ShaderLibrary::Engine()->Load("engine://Shaders/upscale_depth.glsl");
    m_Bridge.Init(m_Vulkan, m_GL);
}

void VulkanBridgedUpscaler::DeclareResources(RenderGraph& graph)
{
    graph.AddFBO(k_Output, FramebufferFormat::RGBA16F, 1.0f, 1, RenderGraph::Resolution::Output);
    graph.AddFBO(k_Depth,  FramebufferFormat::R32F,    1.0f, 1, RenderGraph::Resolution::Render);
    graph.SetAlias(OutputTarget, k_Output);
}

void VulkanBridgedUpscaler::Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd)
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
    cmd.Invoke([this, in, output, depth]()
    {
        using B = VulkanUpscalerBridge;
        const auto& sceneSpec = in.sceneTarget->GetSpecification();
        const auto& outSpec   = output->GetSpecification();
        const glm::uvec2 rs { sceneSpec.Width, sceneSpec.Height };
        const glm::uvec2 os { outSpec.Width,   outSpec.Height };
        m_Bridge.Resize(rs, os);

        m_GL.CopyTexture(in.sceneTarget->GetColorAttachmentRendererID(0), m_Bridge.GLTexture(B::Color),    rs.x, rs.y);
        m_GL.CopyTexture(in.sceneTarget->GetColorAttachmentRendererID(1), m_Bridge.GLTexture(B::Velocity), rs.x, rs.y);
        m_GL.CopyTexture(depth->GetColorAttachmentRendererID(0),          m_Bridge.GLTexture(B::Depth),    rs.x, rs.y);

        m_Bridge.Run([&](VkCommandBuffer vkCmd) { RecordUpscale(vkCmd, m_Bridge, in); });

        m_GL.CopyTexture(m_Bridge.GLTexture(B::Output), output->GetColorAttachmentRendererID(0), os.x, os.y);
    });
}

}
