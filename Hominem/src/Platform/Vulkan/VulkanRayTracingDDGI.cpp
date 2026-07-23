#include "hmnpch.h"
#include "VulkanRayTracingDDGI.h"
#include "VulkanRenderer.h"
#include "VulkanShaderLibrary.h"
#include "VulkanImage.h"
#include "VulkanBarrier.h"

#include <glm/glm.hpp>

namespace Hominem {

namespace {

struct DDGITracePush
{
    glm::vec4       origin;
    glm::vec4       spacing;
    glm::ivec4      counts;   // xyz = counts, w = numRays
    glm::vec4       rotation; // quaternion (x,y,z,w)
    glm::ivec4      tiles;    // x = irradiance interior, y = distance interior
    VkDeviceAddress surfels;
    VkDeviceAddress instances;
    VkDeviceAddress scene;
    float           surfelRadius;
    float           maxRayDistance;
};

struct DDGIBlendPush
{
    glm::vec4  origin;
    glm::vec4  spacing;
    glm::ivec4 counts;
    glm::vec4  rotation;
    glm::ivec4 tiles;      // x = irradiance interior, y = distance interior
    float      hysteresis;
    int        firstFrame;
    float      depthSharpness; // distance blend only
    float      maxDistance;    // distance blend only
};

struct DDGIBorderPush
{
    glm::ivec4 counts;
    glm::ivec4 tiles;
};

constexpr float k_Hysteresis     = 0.97f;
constexpr float k_DepthSharpness = 50.f;
constexpr float k_MaxRayDistance = 40.f;
constexpr float k_SurfelRadius   = 0.03f;

}

void VulkanRayTracingDDGI::Shutdown(DeletionQueue& mainQueue, VkDevice device, VmaAllocator allocator)
{
    // Deferred, not called directly: the caller flushes this queue only after its own
    // vkDeviceWaitIdle, so these are guaranteed safe to destroy by the time they run.
    if (m_TraceReady)          mainQueue.push_function([this]() { m_TracePipeline.Destroy(); });
    if (m_BlendReady)          mainQueue.push_function([this]() { m_BlendIrradiancePipeline.Destroy(); });
    if (m_BlendDistanceReady)  mainQueue.push_function([this]() { m_BlendDistancePipeline.Destroy(); });
    if (m_BorderReady)         mainQueue.push_function([this]() { m_BorderPipeline.Destroy(); });
    if (m_BorderDistanceReady) mainQueue.push_function([this]() { m_BorderDistancePipeline.Destroy(); });
    if (m_ResourcesCreated)
    {
        mainQueue.push_function([this, device, allocator]() { m_RayData.Destroy(device, allocator); });
        mainQueue.push_function([this, device, allocator]() { m_Irradiance.Destroy(device, allocator); });
        mainQueue.push_function([this, device, allocator]() { m_Distance.Destroy(device, allocator); });
    }
    if (m_SurfelBuffersCreated)
        for (auto& buf : m_SurfelBuffers)
            mainQueue.push_function([&buf, allocator]() { buf.Destroy(allocator); });
}

void VulkanRayTracingDDGI::Execute(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi)
{
    EnsureResources(ctx, ddgi);

    using U = GraphUsage;
    m_Graph.Begin();
    const auto rayData = m_Graph.AddResource("ddgi.raydata");
    const auto irr     = m_Graph.AddResource("ddgi.irradiance");
    const auto dist    = m_Graph.AddResource("ddgi.distance");
    const auto surfels = m_Graph.AddResource("ddgi.surfels");

    m_Graph.AddPass("ddgi.trace",
        { { rayData, U::ComputeWrite }, { surfels, U::ComputeWrite },
          { irr, U::ComputeSample }, { dist, U::ComputeSample } },
        [this, &ctx, &ddgi](VkCommandBuffer) { RunTrace(ctx, ddgi); });

    m_Graph.AddPass("ddgi.blend_irradiance",
        { { irr, U::ComputeReadWrite }, { rayData, U::ComputeRead } },
        [this, &ctx, &ddgi](VkCommandBuffer) { RunBlendIrradiance(ctx, ddgi); });

    m_Graph.AddPass("ddgi.blend_distance",
        { { dist, U::ComputeReadWrite }, { rayData, U::ComputeRead } },
        [this, &ctx, &ddgi](VkCommandBuffer) { RunBlendDistance(ctx, ddgi); });

    m_Graph.AddPass("ddgi.border_irradiance",
        { { irr, U::ComputeReadWrite } },
        [this, &ctx, &ddgi](VkCommandBuffer)
        {
            RunBorder(ctx, m_BorderPipeline, m_BorderReady,
                      m_Irradiance.GetImageView(), k_IrradianceInterior, ddgi, m_AtlasW, m_AtlasH);
        });

    m_Graph.AddPass("ddgi.border_distance",
        { { dist, U::ComputeReadWrite } },
        [this, &ctx, &ddgi](VkCommandBuffer)
        {
            RunBorder(ctx, m_BorderDistancePipeline, m_BorderDistanceReady,
                      m_Distance.GetImageView(), k_DistanceInterior, ddgi, m_DistW, m_DistH);
        });

    // Sync-only: hand the atlases to the mesh fragment shader and surfels to the debug vertex shader.
    m_Graph.AddPass("ddgi.handoff",
        { { irr, U::FragmentSample }, { dist, U::FragmentSample }, { surfels, U::VertexRead } },
        nullptr);

    m_Graph.Execute(ctx.cmd);
    m_FirstBlend = false;
}

void VulkanRayTracingDDGI::EnsureResources(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi)
{
    auto            device    = ctx.renderer.GetDevice();
    auto            allocator = ctx.renderer.GetAllocator();
    VkCommandBuffer cmd       = ctx.cmd;

    const uint32_t rayW     = (uint32_t)ddgi.probeNumRays;
    const uint32_t rayH     = (uint32_t)(ddgi.probeCounts.x * ddgi.probeCounts.y * ddgi.probeCounts.z);
    const int      slice    = ddgi.probeCounts.x * ddgi.probeCounts.z;
    const int      irrFull  = k_IrradianceInterior + 2;
    const int      distFull = k_DistanceInterior + 2;
    const uint32_t atlasW    = (uint32_t)slice * irrFull;
    const uint32_t atlasH    = (uint32_t)ddgi.probeCounts.y * irrFull;
    const uint32_t distW     = (uint32_t)slice * distFull;
    const uint32_t distH     = (uint32_t)ddgi.probeCounts.y * distFull;

    if (m_ResourcesCreated &&
        rayW == m_RayDataW && rayH == m_RayDataH &&
        atlasW == m_AtlasW && atlasH == m_AtlasH)
        return;

    if (m_ResourcesCreated)
    {
        ctx.renderer.GetFrameDeletionQueue().push_function(
            [rt = m_RayData, device, allocator]() mutable { rt.Destroy(device, allocator); });
        ctx.renderer.GetFrameDeletionQueue().push_function(
            [rt = m_Irradiance, device, allocator]() mutable { rt.Destroy(device, allocator); });
        ctx.renderer.GetFrameDeletionQueue().push_function(
            [rt = m_Distance, device, allocator]() mutable { rt.Destroy(device, allocator); });
    }

    // All three are RGBA16F so the shared border shader (rgba16f) matches every view;
    // the distance atlas only uses .rg (mean, mean^2), the extra channels are slack.
    m_RayData    = VulkanRenderTarget::Create(device, allocator, rayW, rayH, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_Irradiance = VulkanRenderTarget::Create(device, allocator, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_Distance   = VulkanRenderTarget::Create(device, allocator, distW, distH, VK_FORMAT_R16G16B16A16_SFLOAT);

    // All three go to GENERAL; the atlases are cleared so the first frame's trace samples
    // zeroes (not uninitialized float16) before the first blend overwrites them.
    const VkClearColorValue zero { .float32 = { 0.f, 0.f, 0.f, 0.f } };
    const VkImageSubresourceRange range { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (VkImage img : { m_RayData.GetImage(), m_Irradiance.GetImage(), m_Distance.GetImage() })
        VulkanImage::TransitionUndefinedToGeneral(cmd, img);

    vkCmdClearColorImage(cmd, m_Irradiance.GetImage(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    vkCmdClearColorImage(cmd, m_Distance.GetImage(),   VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);

    PipelineBarrier(cmd, MakeMemoryBarrier(
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT));

    m_RayDataW = rayW;   m_RayDataH = rayH;
    m_AtlasW   = atlasW; m_AtlasH   = atlasH;
    m_DistW    = distW;  m_DistH    = distH;
    m_ResourcesCreated = true;
    m_FirstBlend       = true;
}

void VulkanRayTracingDDGI::RunTrace(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi)
{
    auto            device    = ctx.renderer.GetDevice();
    auto            allocator = ctx.renderer.GetAllocator();
    const uint32_t  frameIdx  = ctx.renderer.GetCurrentFrameIndex();
    VkCommandBuffer cmd       = ctx.cmd;

    if (!m_TraceReady)
    {
        const std::vector<ComputeBindingSpec> specs =
        {
            { .binding = 0, .type = ComputeBindingSpec::Type::AccelerationStructure },
            { .binding = 1, .type = ComputeBindingSpec::Type::StorageImage },
            { .binding = 2, .type = ComputeBindingSpec::Type::SampledImage },
            { .binding = 3, .type = ComputeBindingSpec::Type::SampledImage },
        };
        const VulkanShader& shader = ctx.shaders.LoadCompute("vk_ddgi_trace");
        m_TracePipeline = VulkanComputePipeline::Create(device, shader.computeSpirv, specs, sizeof(DDGITracePush));
        m_TraceReady = true;
    }

    m_SurfelCount = (uint32_t)(ddgi.probeCounts.x * ddgi.probeCounts.y * ddgi.probeCounts.z) * (uint32_t)ddgi.probeNumRays;
    const uint32_t needed = m_SurfelCount * (uint32_t)sizeof(VulkanSphereInstance);
    if (!m_SurfelBuffersCreated || needed > m_SurfelCapacity)
    {
        if (m_SurfelBuffersCreated)
            for (auto& buf : m_SurfelBuffers)
                ctx.renderer.GetFrameDeletionQueue().push_function(
                    [b = buf, allocator]() mutable { b.Destroy(allocator); });

        m_SurfelCapacity = std::max(needed, 1u);
        for (auto& buf : m_SurfelBuffers)
            buf = VulkanStorageBuffer::Create(allocator, m_SurfelCapacity,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_SurfelBuffersCreated = true;
    }

    m_TracePipeline.WriteAccelerationStructure(frameIdx, 0, ctx.tlas);
    m_TracePipeline.WriteStorageImage(frameIdx, 1, m_RayData.GetImageView());
    m_TracePipeline.WriteSampledImage(frameIdx, 2, m_Irradiance.GetImageView(),
                                      m_Irradiance.GetSampler(), VK_IMAGE_LAYOUT_GENERAL);
    m_TracePipeline.WriteSampledImage(frameIdx, 3, m_Distance.GetImageView(),
                                      m_Distance.GetSampler(), VK_IMAGE_LAYOUT_GENERAL);

    const DDGITracePush push
    {
        .origin         = glm::vec4(ddgi.origin, 0.f),
        .spacing        = glm::vec4(ddgi.probeSpacing, 0.f),
        .counts         = glm::ivec4(ddgi.probeCounts, ddgi.probeNumRays),
        .rotation       = glm::vec4(ddgi.probeRayRotation.x, ddgi.probeRayRotation.y,
                                    ddgi.probeRayRotation.z, ddgi.probeRayRotation.w),
        .tiles          = glm::ivec4(k_IrradianceInterior, k_DistanceInterior, 0, 0),
        .surfels        = m_SurfelBuffers[frameIdx].GetDeviceAddress(device),
        .instances      = ctx.instanceGeom,
        .scene          = ctx.scene,
        .surfelRadius   = k_SurfelRadius,
        .maxRayDistance = k_MaxRayDistance,
    };
    m_TracePipeline.PushConstants(cmd, &push, sizeof(push));
    m_TracePipeline.Dispatch(cmd, frameIdx, (m_SurfelCount + 63) / 64);
}

void VulkanRayTracingDDGI::RunBlendIrradiance(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi)
{
    auto            device   = ctx.renderer.GetDevice();
    const uint32_t  frameIdx = ctx.renderer.GetCurrentFrameIndex();
    VkCommandBuffer cmd      = ctx.cmd;

    if (!m_BlendReady)
    {
        const std::vector<ComputeBindingSpec> specs =
        {
            { .binding = 0, .type = ComputeBindingSpec::Type::StorageImage },
            { .binding = 1, .type = ComputeBindingSpec::Type::StorageImage },
        };
        const VulkanShader& shader = ctx.shaders.LoadCompute("vk_ddgi_blend_irradiance");
        m_BlendIrradiancePipeline = VulkanComputePipeline::Create(device, shader.computeSpirv, specs, sizeof(DDGIBlendPush));
        m_BlendReady = true;
    }

    m_BlendIrradiancePipeline.WriteStorageImage(frameIdx, 0, m_Irradiance.GetImageView());
    m_BlendIrradiancePipeline.WriteStorageImage(frameIdx, 1, m_RayData.GetImageView());

    const DDGIBlendPush push
    {
        .origin     = glm::vec4(ddgi.origin, 0.f),
        .spacing    = glm::vec4(ddgi.probeSpacing, 0.f),
        .counts     = glm::ivec4(ddgi.probeCounts, ddgi.probeNumRays),
        .rotation   = glm::vec4(ddgi.probeRayRotation.x, ddgi.probeRayRotation.y,
                                ddgi.probeRayRotation.z, ddgi.probeRayRotation.w),
        .tiles      = glm::ivec4(k_IrradianceInterior, 0, 0, 0),
        .hysteresis = k_Hysteresis,
        .firstFrame = m_FirstBlend ? 1 : 0,
    };
    m_BlendIrradiancePipeline.PushConstants(cmd, &push, sizeof(push));
    m_BlendIrradiancePipeline.Dispatch(cmd, frameIdx, (m_AtlasW + 7) / 8, (m_AtlasH + 7) / 8);
}

void VulkanRayTracingDDGI::RunBlendDistance(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi)
{
    auto            device   = ctx.renderer.GetDevice();
    const uint32_t  frameIdx = ctx.renderer.GetCurrentFrameIndex();
    VkCommandBuffer cmd      = ctx.cmd;

    if (!m_BlendDistanceReady)
    {
        const std::vector<ComputeBindingSpec> specs =
        {
            { .binding = 0, .type = ComputeBindingSpec::Type::StorageImage },
            { .binding = 1, .type = ComputeBindingSpec::Type::StorageImage },
        };
        const VulkanShader& shader = ctx.shaders.LoadCompute("vk_ddgi_blend_distance");
        m_BlendDistancePipeline = VulkanComputePipeline::Create(device, shader.computeSpirv, specs, sizeof(DDGIBlendPush));
        m_BlendDistanceReady = true;
    }

    m_BlendDistancePipeline.WriteStorageImage(frameIdx, 0, m_Distance.GetImageView());
    m_BlendDistancePipeline.WriteStorageImage(frameIdx, 1, m_RayData.GetImageView());

    const DDGIBlendPush push
    {
        .origin         = glm::vec4(ddgi.origin, 0.f),
        .spacing        = glm::vec4(ddgi.probeSpacing, 0.f),
        .counts         = glm::ivec4(ddgi.probeCounts, ddgi.probeNumRays),
        .rotation       = glm::vec4(ddgi.probeRayRotation.x, ddgi.probeRayRotation.y,
                                    ddgi.probeRayRotation.z, ddgi.probeRayRotation.w),
        .tiles          = glm::ivec4(k_IrradianceInterior, k_DistanceInterior, 0, 0),
        .hysteresis     = k_Hysteresis,
        .firstFrame     = m_FirstBlend ? 1 : 0,
        .depthSharpness = k_DepthSharpness,
        .maxDistance    = k_MaxRayDistance,
    };
    m_BlendDistancePipeline.PushConstants(cmd, &push, sizeof(push));
    m_BlendDistancePipeline.Dispatch(cmd, frameIdx, (m_DistW + 7) / 8, (m_DistH + 7) / 8);
}

void VulkanRayTracingDDGI::RunBorder(const RaytracingContext& ctx, VulkanComputePipeline& pipeline, bool& ready,
                             VkImageView atlasView, int interior,
                             const VulkanDDGIParams& ddgi, uint32_t atlasW, uint32_t atlasH)
{
    auto            device   = ctx.renderer.GetDevice();
    const uint32_t  frameIdx = ctx.renderer.GetCurrentFrameIndex();
    VkCommandBuffer cmd      = ctx.cmd;

    if (!ready)
    {
        const std::vector<ComputeBindingSpec> specs =
        {
            { .binding = 0, .type = ComputeBindingSpec::Type::StorageImage },
        };
        const VulkanShader& shader = ctx.shaders.LoadCompute("vk_ddgi_border");
        pipeline = VulkanComputePipeline::Create(device, shader.computeSpirv, specs, sizeof(DDGIBorderPush));
        ready = true;
    }

    pipeline.WriteStorageImage(frameIdx, 0, atlasView);

    const DDGIBorderPush push
    {
        .counts = glm::ivec4(ddgi.probeCounts, ddgi.probeNumRays),
        .tiles  = glm::ivec4(interior, 0, 0, 0),
    };
    pipeline.PushConstants(cmd, &push, sizeof(push));
    pipeline.Dispatch(cmd, frameIdx, (atlasW + 7) / 8, (atlasH + 7) / 8);
}

}
