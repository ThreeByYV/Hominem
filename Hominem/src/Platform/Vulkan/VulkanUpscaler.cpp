#include "hmnpch.h"
#include "VulkanUpscaler.h"

#include "Platform/Interop/GpuInterop.h"
#include "Hominem/Renderer/Frame/RenderGraph.h"

namespace Hominem {

namespace {

constexpr const char* k_Output = "vk_upscale_out";
constexpr const char* k_Depth  = "vk_upscale_depth"; // scene depth as R32F; D24S8 doesn't share

// The round trip's images, in order.
enum Image : size_t { Color, Velocity, Depth, Output, ImageCount };

void RecordBlit(VkCommandBuffer cmd, const UpscaleImages& images)
{
    const VkImageBlit region
    {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets     = { { 0, 0, 0 }, { (int32_t)images.renderSize.x, (int32_t)images.renderSize.y, 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets     = { { 0, 0, 0 }, { (int32_t)images.outputSize.x, (int32_t)images.outputSize.y, 1 } },
    };
    vkCmdBlitImage(cmd, images.color.GetImage(),  VK_IMAGE_LAYOUT_GENERAL,
                        images.output.GetImage(), VK_IMAGE_LAYOUT_GENERAL,
                        1, &region, VK_FILTER_LINEAR);
}

}

VulkanUpscaler::~VulkanUpscaler()
{
    m_Strategy.reset(); // releases its SDK resources before the shared images go
    m_RoundTrip.reset();
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
    m_RoundTrip = m_Interop.CreateRoundTrip();
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
    if (!output || !depth || !m_DepthCopyShader || !m_RoundTrip || in.renderSize.x == 0 || in.renderSize.y == 0) return;

    ConvertDepth(cmd, depth, in.renderSize);

    // Sizes and IDs are read when the commands execute: the graph resizes between recording
    // and execution, and a resize recreates every GL object in the target.
    const bool active = StrategyActive();
    cmd.Invoke([this, in, depth, output, active]() { Upscale(in, depth, output, active); });
}

void VulkanUpscaler::ConvertDepth(CommandList& cmd, const Ref<Framebuffer>& depth, glm::uvec2 renderSize)
{
    // Depth into a color target that can be shared; the pass bound the scene depth at Slot::Depth.
    cmd.BindFramebuffer(depth->GetRendererID());
    cmd.SetViewport(0, 0, renderSize.x, renderSize.y);
    cmd.BindShader(m_DepthCopyShader);
    cmd.SetInt(m_DepthCopyShader, "u_Depth", (int)Slot::Depth);
    cmd.DrawFullscreenTriangle();
}

void VulkanUpscaler::Upscale(const UpscalerInputs& in, const Ref<Framebuffer>& depth,
                             const Ref<Framebuffer>& output, bool active)
{
    const auto& sceneSpec = in.sceneTarget->GetSpecification();
    const auto& outSpec   = output->GetSpecification();
    const glm::uvec2 renderSize { sceneSpec.Width, sceneSpec.Height };
    const glm::uvec2 outputSize { outSpec.Width,   outSpec.Height };

    ResizeImages(renderSize, outputSize);
    UploadInputs(in, depth);
    RunStrategy(in, renderSize, outputSize, active);
    m_RoundTrip->Download(Output, output->GetColorAttachmentRendererID(0), true);
}

void VulkanUpscaler::ResizeImages(glm::uvec2 renderSize, glm::uvec2 outputSize)
{
    using Spec = InteropRoundTrip::ImageSpec;
    const Spec specs[ImageCount] =
    {
        { renderSize, VK_FORMAT_R16G16B16A16_SFLOAT }, // Color
        { renderSize, VK_FORMAT_R16G16B16A16_SFLOAT }, // Velocity (RG used)
        { renderSize, VK_FORMAT_R32_SFLOAT          }, // Depth
        { outputSize, VK_FORMAT_R16G16B16A16_SFLOAT }, // Output
    };
    if (m_RoundTrip->SetImages(specs))
        HMN_CORE_INFO("Upscaler: {0}x{1} -> {2}x{3}", renderSize.x, renderSize.y, outputSize.x, outputSize.y);
}

void VulkanUpscaler::UploadInputs(const UpscalerInputs& in, const Ref<Framebuffer>& depth)
{
    // Flipped so the Vulkan side sees the frame upright (row 0 at the top); depth was
    // already flipped by the copy shader.
    m_RoundTrip->Upload(Color,    in.sceneTarget->GetColorAttachmentRendererID(0), true);
    m_RoundTrip->Upload(Velocity, in.sceneTarget->GetColorAttachmentRendererID(1), true);
    m_RoundTrip->Upload(Depth,    depth->GetColorAttachmentRendererID(0));
}

void VulkanUpscaler::RunStrategy(const UpscalerInputs& in, glm::uvec2 renderSize, glm::uvec2 outputSize, bool active)
{
    const UpscaleImages images
    {
        m_RoundTrip->Target(Color), m_RoundTrip->Target(Velocity),
        m_RoundTrip->Target(Depth), m_RoundTrip->Target(Output), renderSize, outputSize,
    };
    m_RoundTrip->Run([&](VkCommandBuffer cmd)
    {
        // Off, out of range or failed: the frame as rendered, no AA.
        if (!active || !m_Strategy->Evaluate(cmd, images, in))
            RecordBlit(cmd, images);
    });
}

}
