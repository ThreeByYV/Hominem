#include "hmnpch.h"
#include "UpscalerFactory.h"
#include "Hominem/Renderer/PostProcess/Upscaling/TAA/TAAUpscaler.h"

#include "Platform/Vulkan/VulkanSceneRenderer.h"
#include "Platform/Vulkan/VulkanPassthroughUpscaler.h"
#ifdef HMN_ENABLE_DLSS
    #include "Platform/Vulkan/DLSS/VulkanDLSSUpscaler.h"
#endif

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
#ifdef HMN_ENABLE_DLSS
            if (!bridge)
            {
                HMN_CORE_WARN("Upscaler: GL/Vulkan interop unavailable, using TAA");
                break;
            }
            if (auto dlss = CreateScope<VulkanDLSSUpscaler>(context.vulkan->GetRenderer(), *context.interop);
                dlss->IsSupported())
                return dlss;
            HMN_CORE_WARN("Upscaler: DLSS unavailable on this system, using TAA");
#else
            HMN_CORE_WARN("Upscaler: built without DLSS (HMN_ENABLE_DLSS), using TAA");
#endif
            break;
        case UpscalerBackend::FSR:
            HMN_CORE_WARN("Upscaler: FSR is not built in yet, using TAA");
            break;
        case UpscalerBackend::TAA:
            break;
    }
    return CreateScope<TAAUpscaler>();
}

}
