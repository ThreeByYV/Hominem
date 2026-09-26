#include "hmnpch.h"
#include "UpscalerFactory.h"
#include "Hominem/Renderer/PostProcess/Upscaling/TAA/TAAUpscaler.h"

namespace Hominem {

Scope<Upscaler> CreateUpscaler(UpscalerBackend preferred)
{
    switch (preferred)
    {
        case UpscalerBackend::DLSS:
        case UpscalerBackend::FSR:
            HMN_CORE_WARN("Upscaler backend {0} is not built in yet, using TAA", (int)preferred);
            [[fallthrough]];
        case UpscalerBackend::TAA:
            break;
    }
    return CreateScope<TAAUpscaler>();
}

}
