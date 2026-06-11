#pragma once
#include "ShaderPermutation.h"

// Edit this file to add/remove PBR feature combos.
// Edit includes/pbr.glsl to improve PBR math.

namespace Hominem {

inline const std::vector<MeshVariant> k_PBR = {
    { "static_pbr",           { "FORWARD_PLUS" } },
    { "static_pbr_nm",        { "FORWARD_PLUS", "HAS_NORMAL_MAP" } },
    { "static_pbr_mr",        { "FORWARD_PLUS", "HAS_METALROUGHNESS_TEX" } },
    { "static_pbr_nm_mr",     { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX" } },
    { "static_pbr_env",       { "FORWARD_PLUS", "HAS_ENV_MAP" } },
    { "static_pbr_nm_env",    { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_ENV_MAP" } },
    { "static_pbr_mr_env",    { "FORWARD_PLUS", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP" } },
    { "static_pbr_nm_mr_env", { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP" } },
};

}
