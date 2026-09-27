#include "hmnpch.h"
#include "UpscalerFactory.h"
#include "Hominem/Renderer/PostProcess/Upscaling/TAA/TAAUpscaler.h"

#include "Platform/Vulkan/VulkanSceneRenderer.h"
#include "Platform/Vulkan/VulkanPassthroughUpscaler.h"

#include <string_view>

namespace Hominem {

UpscalerBackend UpscalerBackendFromEnvironment()
{
    char value[32] = {};
    const DWORD len = GetEnvironmentVariableA("HOMINEM_UPSCALER", value, sizeof(value));
    const std::string_view name(value, len < sizeof(value) ? len : 0);

    if (name == "passthrough") return UpscalerBackend::Passthrough;
    if (name == "dlss")        return UpscalerBackend::DLSS;
    return UpscalerBackend::TAA;
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
