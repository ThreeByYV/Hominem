#pragma once
#include "Hominem/Renderer/RHI/ShaderPermutation.h"

// Edit this file to add/remove toon feature combos.
// Edit mesh.glsl #ifdef HAS_TOON block for toon lighting changes.

namespace Hominem {

inline const std::vector<MeshVariant> k_Toon = {
    { "static_toon",             { "CLUSTERED", "HAS_TOON" } },
    { "static_toon_nm",          { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_TOON" } },
    { "static_toon_mr",          { "CLUSTERED", "HAS_METALROUGHNESS_TEX", "HAS_TOON" } },
    { "static_toon_nm_mr",       { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_TOON" } },
    { "static_toon_env",         { "CLUSTERED", "HAS_ENV_MAP", "HAS_TOON" } },
    { "static_toon_nm_env",      { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_ENV_MAP", "HAS_TOON" } },
    { "static_toon_mr_env",      { "CLUSTERED", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP", "HAS_TOON" } },
    { "static_toon_nm_mr_env",   { "CLUSTERED", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP", "HAS_TOON" } },
};

}
