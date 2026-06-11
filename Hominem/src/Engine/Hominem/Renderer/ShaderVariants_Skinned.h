#pragma once
#include "ShaderPermutation.h"

// Edit this file to add/remove skinned mesh feature combos.
// Skinned meshes use SSBO vertex data — no static mesh vertex layout.

namespace Hominem {

inline const std::vector<MeshVariant> k_Skinned = {
    { "skinned_pbr",         { "SKINNED" } },
    { "skinned_pbr_nm",      { "SKINNED", "HAS_NORMAL_MAP" } },
    { "skinned_pbr_mr",      { "SKINNED", "HAS_METALROUGHNESS_TEX" } },
    { "skinned_pbr_nm_mr",   { "SKINNED", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX" } },
    { "skinned_toon",        { "SKINNED", "HAS_TOON" } },
    { "skinned_toon_nm",     { "SKINNED", "HAS_NORMAL_MAP", "HAS_TOON" } },
    { "skinned_toon_mr",     { "SKINNED", "HAS_METALROUGHNESS_TEX", "HAS_TOON" } },
    { "skinned_toon_nm_mr",  { "SKINNED", "HAS_NORMAL_MAP", "HAS_METALROUGHNESS_TEX", "HAS_TOON" } },
};

}
