#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hominem {

struct VulkanShader
{
    std::vector<uint32_t> vertexSpirv;
    std::vector<uint32_t> fragmentSpirv;
    std::vector<uint32_t> computeSpirv;

    bool IsCompute() const { return !computeSpirv.empty(); }
};

class VulkanShaderLibrary
{
public:
    const VulkanShader& Load(const std::string& name);
    const VulkanShader& LoadCompute(const std::string& name, const std::string& source);
    const VulkanShader& Get(const std::string& name);

    bool Exists(const std::string& name) const;

private:
    std::unordered_map<std::string, VulkanShader> m_Shaders;
};

}
