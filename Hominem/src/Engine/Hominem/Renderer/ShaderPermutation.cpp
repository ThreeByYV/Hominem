#include "hmnpch.h"
#include "ShaderPermutation.h"
#include "Shader.h"

namespace Hominem
{
    Ref<ShaderPermutationSet> ShaderPermutationSet::Create(const std::string& filepath,
                                                             const uint32_t*    validPerms,
                                                             size_t             count)
    {
        auto set = CreateRef<ShaderPermutationSet>();
        set->m_Filepath = filepath;
        set->m_ValidPerms.assign(validPerms, validPerms + count);

        for (size_t i = 0; i < count; i++)
        {
            uint32_t perm    = validPerms[i];
            auto     defines = PermToDefines(perm);

            // TODO: make this async — push to ShaderCompileThread::Queue() instead of blocking here.
            auto shader = Shader::Create(filepath, defines);

            auto defStr = defines.empty() ? std::string("(none)") : [&]{
                std::string s; for (auto& d : defines) s += d + " "; return s; }();
            HMN_CORE_INFO("ShaderPermutationSet: compiled perm 0x{:X} [{}] '{}'", perm, defStr, filepath);
            set->m_Variants[perm] = std::move(shader);
        }

        return set;
    }

    Ref<Shader> ShaderPermutationSet::GetVariant(uint32_t permFlags) const
    {
        auto it = m_Variants.find(permFlags);
        if (it != m_Variants.end())
            return it->second;

        HMN_CORE_WARN("ShaderPermutationSet: perm 0x{:X} not found — falling back to 0", permFlags);
        auto fallback = m_Variants.find(0);
        return fallback != m_Variants.end() ? fallback->second : nullptr;
    }

    void ShaderPermutationSet::ReloadAll()
    {
        for (auto& [perm, shader] : m_Variants)
        {
            // TODO: when async, re-queue to ShaderCompileThread instead of blocking reload.
            shader->Reload();
            HMN_CORE_INFO("ShaderPermutationSet: reloaded perm 0x{:X}", perm);
        }
    }
}
