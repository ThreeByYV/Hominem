#include "hmnpch.h"
#include "UpscalerFactory.h"
#include "Hominem/Renderer/PostProcess/Upscaling/TAA/TAAUpscaler.h"

#include "Platform/Interop/GpuInterop.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanUpscaler.h"
#include "Platform/Vulkan/NVIDIA/DLSS.h"
#include "Platform/Vulkan/AMD/FSR.h"

#include "Hominem/Renderer/Frame/RenderSettings.h"

#include <iterator>

namespace Hominem {

static_assert(std::size(RenderSettings::UpscalerNames) - 1 == (size_t)UpscalerBackend::Passthrough + 1,
              "RenderSettings::UpscalerNames must list every UpscalerBackend, in order");

UpscalerBackend UpscalerBackendFromSettings()
{
    const int index = RenderSettings::Upscaler;
    return index >= 0 && index <= (int)UpscalerBackend::Passthrough ? (UpscalerBackend)index
                                                                    : UpscalerBackend::TAA;
}

Scope<Upscaler> CreateUpscaler(UpscalerBackend preferred, const UpscalerContext& context)
{
    if (preferred == UpscalerBackend::TAA) return CreateScope<TAAUpscaler>();
    if (!context.interop)
    {
        HMN_CORE_WARN("Upscaler: GL/Vulkan interop unavailable, using TAA");
        return CreateScope<TAAUpscaler>();
    }

    VulkanRenderer& vk = context.interop->GetVulkan();
    auto vulkan = [&](Scope<IUpscalerStrategy> strategy)
    {
        return CreateScope<VulkanUpscaler>(*context.interop, std::move(strategy));
    };

    if (preferred == UpscalerBackend::Passthrough)
        return vulkan(nullptr);

    // Strategies in fallback order: DLSS on NVIDIA, FSR elsewhere, then TAA.
    if (preferred == UpscalerBackend::DLSS)
    {
        if (auto dlss = vulkan(CreateScope<DLSS>(vk.GetDevice())); dlss->IsSupported())
            return dlss;
        HMN_CORE_WARN("Upscaler: DLSS unavailable, trying FSR");
    }
    if (auto fsr = vulkan(CreateScope<FSR>()); fsr->IsSupported())
        return fsr;
    HMN_CORE_WARN("Upscaler: FSR unavailable, using TAA");
    return CreateScope<TAAUpscaler>();
}

}
