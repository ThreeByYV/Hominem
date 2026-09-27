#pragma once

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/PostProcess/Upscaling/Upscaler.h"

namespace Hominem {

class VulkanSceneRenderer;
class SharedResources;

/// What the Vulkan-based backends need; either being null rules them out.
struct UpscalerContext
{
    VulkanSceneRenderer* vulkan  = nullptr;
    SharedResources*     interop = nullptr;
};

/// HOMINEM_UPSCALER=taa|passthrough|dlss; TAA when unset or unknown.
UpscalerBackend UpscalerBackendFromEnvironment();

/// Falls back down the chain to TAA when the preferred backend is unavailable.
Scope<Upscaler> CreateUpscaler(UpscalerBackend preferred, const UpscalerContext& context);

}
