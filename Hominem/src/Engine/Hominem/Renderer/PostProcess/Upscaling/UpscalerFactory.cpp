#include "hmnpch.h"
#include "UpscalerFactory.h"
#include "Hominem/Renderer/PostProcess/Upscaling/TAA/TAAUpscaler.h"

#include "Platform/Vulkan/VulkanSceneRenderer.h"
#include "Platform/Vulkan/VulkanPassthroughUpscaler.h"

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
    const bool bridge = context.vulkan && context.interop;

    switch (preferred)
    {
        case UpscalerBackend::Passthrough:
            if (bridge)
                return CreateScope<VulkanPassthroughUpscaler>(context.vulkan->GetRenderer(), *context.interop);
            HMN_CORE_WARN("Upscaler: GL/Vulkan interop unavailable, using TAA");
            break;
        case UpscalerBackend::DLSS:
        case UpscalerBackend::FSR:
            HMN_CORE_WARN("Upscaler backend {0} is not built in yet, using TAA", (int)preferred);
            break;
        case UpscalerBackend::TAA:
            break;
    }
    return CreateScope<TAAUpscaler>();
}

}
