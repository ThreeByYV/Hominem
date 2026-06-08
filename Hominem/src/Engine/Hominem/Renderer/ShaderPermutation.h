#pragma once

#include "Shader.h"
#include "Hominem/Core/Core.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <array>

namespace Hominem {

// name = lookup key; defines are injected into mesh.glsl as #defines.
// {static|skinned}_{pbr|toon}[_nm][_mr][_env]  e.g. static_pbr_nm_mr, skinned_toon
struct MeshVariant
{
    std::string              name;
    std::vector<std::string> defines;
};

// compiles all variants from mesh.glsl at startup
class ShaderVariantSet final : public RefCounted
{
public:
    static Ref<ShaderVariantSet> Create(const std::string&              filepath,
                                         const std::vector<MeshVariant>& variants);
    Ref<Shader> Get(const std::string& name) const;
    void ReloadAll();
    const std::string& GetFilepath() const { return m_Filepath; }

private:
    std::string m_Filepath;
    std::unordered_map<std::string, Ref<Shader>> m_Variants;
};

}

// add new ShaderVariants_<mode>.h here for each new shading mode
#include "ShaderVariants_PBR.h"
#include "ShaderVariants_Toon.h"
#include "ShaderVariants_Skinned.h"

namespace Hominem {

// merges all mode files into one vector at startup
inline std::vector<MeshVariant> GetAllVariants()
{
    std::vector<MeshVariant> all;
    all.insert(all.end(), k_PBR.begin(),     k_PBR.end());
    all.insert(all.end(), k_Toon.begin(),    k_Toon.end());
    all.insert(all.end(), k_Skinned.begin(), k_Skinned.end());
    return all;
}

}
