#include "hmnpch.h"
#include "VulkanShaderLibrary.h"
#include "VulkanShaderCompiler.h"

#include "Hominem/Assets/AssetManager.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Renderer/ShaderSource.h"

namespace Hominem {

static std::string LoadShaderSource(const std::string& uri)
{
    auto result = AssetManager::Load<ShaderSource>(uri);
    if (!result)
    {
        HMN_CORE_ERROR("VulkanShaderLibrary: {}", result.error().message);
        HMN_CORE_ASSERT(false, "Shader source load failed");
        return {};
    }
    return result->Get()->source;
}

const VulkanShader& VulkanShaderLibrary::Load(const std::string& name)
{
    if (auto it = m_Shaders.find(name); it != m_Shaders.end())
        return it->second;

    const std::string vertSource = LoadShaderSource("engine://Shaders/" + name + ".vert");
    const std::string fragSource = LoadShaderSource("engine://Shaders/" + name + ".frag");

    VulkanShader shader;
    shader.vertexSpirv   = VulkanShaderCompiler::Compile(vertSource, VulkanShaderStage::Vertex,   name + ".vert");
    shader.fragmentSpirv = VulkanShaderCompiler::Compile(fragSource, VulkanShaderStage::Fragment, name + ".frag");

    return m_Shaders.emplace(name, std::move(shader)).first->second;
}

const VulkanShader& VulkanShaderLibrary::LoadCompute(const std::string& name, const std::string& source)
{
    if (auto it = m_Shaders.find(name); it != m_Shaders.end())
        return it->second;

    VulkanShader shader;
    shader.computeSpirv = VulkanShaderCompiler::Compile(source, VulkanShaderStage::Compute, name + ".comp");

    return m_Shaders.emplace(name, std::move(shader)).first->second;
}

const VulkanShader& VulkanShaderLibrary::LoadCompute(const std::string& name)
{
    if (auto it = m_Shaders.find(name); it != m_Shaders.end())
        return it->second;

    return LoadCompute(name, LoadShaderSource("engine://Shaders/" + name + ".comp"));
}

const VulkanShader& VulkanShaderLibrary::Get(const std::string& name)
{
    auto it = m_Shaders.find(name);
    HMN_CORE_ASSERT(it != m_Shaders.end(), "VulkanShaderLibrary: shader '{}' not loaded", name);
    return it->second;
}

bool VulkanShaderLibrary::Exists(const std::string& name) const
{
    return m_Shaders.contains(name);
}

}
