#pragma once
#include "ShaderPermutation.h"

// Edit this file to add/remove toon feature combos.
// Edit mesh.glsl #ifdef HAS_TOON block for toon lighting changes.

namespace Hominem {

inline const std::vector<MeshVariant> k_Toon = {
    { "static_toon",             { "FORWARD_PLUS", "HAS_TOON" } },
    { "static_toon_nm",          { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_TOON" } },
    { "static_toon_mr",          { "FORWARD_PLUS", "HAS_METALROUGHNESS_TEX", "HAS_TOON" } },
    { "static_toon_nm_mr",       { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_TOON" } },
    { "static_toon_env",         { "FORWARD_PLUS", "HAS_ENV_MAP", "HAS_TOON" } },
    { "static_toon_nm_env",      { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_ENV_MAP", "HAS_TOON" } },
    { "static_toon_mr_env",      { "FORWARD_PLUS", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP", "HAS_TOON" } },
    { "static_toon_nm_mr_env",   { "FORWARD_PLUS", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_ENV_MAP", "HAS_TOON" } },
};

}
