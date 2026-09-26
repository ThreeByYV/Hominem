#pragma once

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/PostProcess/Upscaling/Upscaler.h"

namespace Hominem {

/// Falls back down the chain to TAA when the preferred backend is unavailable.
Scope<Upscaler> CreateUpscaler(UpscalerBackend preferred);

}
