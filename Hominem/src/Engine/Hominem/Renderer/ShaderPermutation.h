#pragma once

#include "Shader.h"
#include "Hominem/Core/Core.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace Hominem {

enum ShaderPerm : uint32_t
{
    ShaderPerm_None              = 0,
    ShaderPerm_HasNormalMap      = 1 << 0,  // TBN + normal map sample
    ShaderPerm_HasEnvMap         = 1 << 1,  // cubemap reflection/refraction
    ShaderPerm_HasMetalRoughness = 1 << 2,  // per-pixel metalness/roughness texture
    ShaderPerm_Skinned           = 1 << 3,  // vertex data from SSBO (skinned mesh)
    ShaderPerm_ForwardPlus       = 1 << 4,  // tile-based point light grid
};

// All valid mesh permutations — static (ForwardPlus) and skinned variants.
// Add a new row here to support a new combination; nothing else needs changing.
static constexpr uint32_t k_MeshPerms[] = {
    ShaderPerm_ForwardPlus,
    ShaderPerm_ForwardPlus | ShaderPerm_HasMetalRoughness,
    ShaderPerm_ForwardPlus | ShaderPerm_HasNormalMap | ShaderPerm_HasMetalRoughness,
    ShaderPerm_ForwardPlus | ShaderPerm_HasNormalMap | ShaderPerm_HasMetalRoughness | ShaderPerm_HasEnvMap,
    ShaderPerm_ForwardPlus | ShaderPerm_HasMetalRoughness | ShaderPerm_HasEnvMap,
    ShaderPerm_ForwardPlus | ShaderPerm_HasEnvMap,
    ShaderPerm_Skinned,
    ShaderPerm_Skinned | ShaderPerm_HasMetalRoughness,
    ShaderPerm_Skinned | ShaderPerm_HasNormalMap | ShaderPerm_HasMetalRoughness,
};


inline std::vector<std::string> PermToDefines(uint32_t perm)
{
    std::vector<std::string> defs;
    if (perm & ShaderPerm_HasNormalMap)      defs.push_back("HAS_NORMAL_MAP");
    if (perm & ShaderPerm_HasEnvMap)         defs.push_back("HAS_ENV_MAP");
    if (perm & ShaderPerm_HasMetalRoughness) defs.push_back("HAS_METALROUGHNESS_TEX");
    if (perm & ShaderPerm_Skinned)           defs.push_back("SKINNED");
    if (perm & ShaderPerm_ForwardPlus)       defs.push_back("FORWARD_PLUS");
    return defs;
}

// Returns the nearest valid permutation for the requested flags.
inline uint32_t ResolvePermutation(uint32_t requested, const uint32_t* valid, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (valid[i] == requested) return requested;

    // Strip the highest set bit first until we match something valid
    uint32_t stripped = requested;
    for (int bit = 31; bit >= 0 && stripped != 0; bit--)
    {
        if (!(stripped & (1u << bit))) continue;
        stripped &= ~(1u << bit);
        for (size_t i = 0; i < count; i++)
            if (valid[i] == stripped) return stripped;
    }
    return 0; // ShaderPerm_None always valid
}

// Compiles N variants of one shader source file at startup, one per whitelisted perm.
class ShaderPermutationSet : public RefCounted
{
public:
    // Compile all variants listed in validPerms from filepath.
    static Ref<ShaderPermutationSet> Create(const std::string& filepath,
                                             const uint32_t*    validPerms,
                                             size_t             count);

    // Returns the variant for the exact permutation flags.
    // Call ResolvePermutation() first if the flags may not be in the whitelist.
    Ref<Shader> GetVariant(uint32_t permFlags) const;

    void ReloadAll();

    const std::string& GetFilepath() const { return m_Filepath; }

private:
    std::string m_Filepath;
    std::vector<uint32_t> m_ValidPerms;
    std::unordered_map<uint32_t, Ref<Shader>> m_Variants;
};

}
