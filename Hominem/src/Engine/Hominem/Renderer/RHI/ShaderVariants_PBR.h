#pragma once
#include "Hominem/Renderer/RHI/ShaderPermutation.h"

// Edit this file to add/remove PBR feature combos.
// Edit includes/pbr.glsl to improve PBR math.

namespace Hominem {

inline const std::vector<MeshVariant> k_PBR = {
    { "static_pbr",           { "CLUSTERED" } },
    { "static_pbr_nm",        { "CLUSTERED", "HAS_NORMAL_MAP" } },
    { "static_pbr_mr",        { "CLUSTERED", "HAS_METALROUGHNESS_TEX" } },
    { "static_pbr_nm_mr",     { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX" } },
    { "static_pbr_env",       { "CLUSTERED", "HAS_ENV_MAP" } },
    { "static_pbr_nm_env",    { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_ENV_MAP" } },
    { "static_pbr_mr_env",    { "CLUSTERED", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP" } },
    { "static_pbr_nm_mr_env", { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP" } },
};

}
