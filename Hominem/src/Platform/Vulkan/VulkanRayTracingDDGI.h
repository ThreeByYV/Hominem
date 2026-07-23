#pragma once

#include "Hominem/Renderer/RenderFrame.h"
#include "RaytracingContext.h"
#include "VulkanComputePipeline.h"
#include "VulkanStorageBuffer.h"
#include "VulkanRenderTarget.h"
#include "VulkanRenderGraph.h"

#include <array>
#include <cstdint>

namespace Hominem {

// Ray-traced diffuse global illumination (RTXGI-DDGI over VK_KHR_ray_query).
class VulkanRayTracingDDGI
{
public:
    static constexpr int k_IrradianceInterior = 8;
    static constexpr int k_DistanceInterior   = 16;

    struct AtlasView
    {
        VkImageView view    = VK_NULL_HANDLE;
        VkSampler   sampler = VK_NULL_HANDLE;
    };

    void Shutdown(DeletionQueue& mainQueue, VkDevice device, VmaAllocator allocator);

    // Trace probe rays, blend into the irradiance + distance atlases, fix up borders.
    // Requires the host to have a scene (ctx.tlas valid) and an active volume.
    void Execute(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi);

    AtlasView GetIrradianceAtlas() const { return { m_Irradiance.GetImageView(), m_Irradiance.GetSampler() }; }

    AtlasView GetDistanceAtlas()   const { return { m_Distance.GetImageView(),   m_Distance.GetSampler() }; }

    bool            HasAtlases()     const { return m_ResourcesCreated; }
    bool            HasSurfels()     const { return m_SurfelBuffersCreated; }
    uint32_t        GetSurfelCount() const { return m_SurfelCount; }

    VkDeviceAddress GetSurfelAddress(VkDevice device, uint32_t frameIdx) const
    {
        return m_SurfelBuffersCreated ? m_SurfelBuffers[frameIdx].GetDeviceAddress(device) : 0;
    }

private:
    void EnsureResources(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi);

    void RunTrace(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi);

    void RunBlendIrradiance(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi);

    void RunBlendDistance(const RaytracingContext& ctx, const VulkanDDGIParams& ddgi);

    void RunBorder(const RaytracingContext& ctx, VulkanComputePipeline& pipeline, bool& ready,
                   VkImageView atlasView, int interior,
                   const VulkanDDGIParams& ddgi, uint32_t atlasW, uint32_t atlasH);

    VulkanComputePipeline m_TracePipeline;           bool m_TraceReady          = false;
    VulkanComputePipeline m_BlendIrradiancePipeline; bool m_BlendReady          = false;
    VulkanComputePipeline m_BlendDistancePipeline;   bool m_BlendDistanceReady  = false;
    VulkanComputePipeline m_BorderPipeline;          bool m_BorderReady         = false;
    VulkanComputePipeline m_BorderDistancePipeline;  bool m_BorderDistanceReady = false;

    std::array<VulkanStorageBuffer, 2> m_SurfelBuffers;
    bool                               m_SurfelBuffersCreated = false;
    uint32_t                           m_SurfelCapacity = 0;
    uint32_t                           m_SurfelCount    = 0;

    VulkanRenderGraph  m_Graph;

    VulkanRenderTarget m_RayData;
    VulkanRenderTarget m_Irradiance;
    VulkanRenderTarget m_Distance;
    bool               m_ResourcesCreated = false;
    bool               m_FirstBlend       = true;
    uint32_t           m_RayDataW = 0, m_RayDataH = 0;
    uint32_t           m_AtlasW   = 0, m_AtlasH   = 0;
    uint32_t           m_DistW    = 0, m_DistH    = 0;
};

}
