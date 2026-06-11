#include "hmnpch.h"
#include "ShaderPermutation.h"
#include "Shader.h"

namespace Hominem
{
    Ref<ShaderVariantSet> ShaderVariantSet::Create(const std::string&              filepath,
                                                    const std::vector<MeshVariant>& variants)
    {
        auto set = CreateRef<ShaderVariantSet>();
        set->m_Filepath = filepath;

        for (const auto& v : variants)
        {
            auto shader = Shader::Create(filepath, v.defines);

            std::string defStr{};
            for (const auto& d : v.defines)
            {
                defStr += d + " ";
            }

            if (defStr.empty())
            {
                defStr = "(none)";
            }

            HMN_CORE_INFO("ShaderVariantSet: compiled '{}' [{}]", v.name, defStr);
            set->m_Variants[v.name] = std::move(shader);
        }

        return set;
    }

    Ref<Shader> ShaderVariantSet::Get(const std::string& name) const
    {
        auto it = m_Variants.find(name);
        HMN_CORE_ASSERT(it != m_Variants.end(),
            "ShaderVariantSet: variant '{}' not found — add it to the appropriate ShaderVariants_*.h", name);
        return it->second;
    }

    void ShaderVariantSet::ReloadAll()
    {
        for (auto& [name, shader] : m_Variants)
        {
            shader->Reload();
            HMN_CORE_INFO("ShaderVariantSet: reloaded '{}'", name);
        }
    }
}
