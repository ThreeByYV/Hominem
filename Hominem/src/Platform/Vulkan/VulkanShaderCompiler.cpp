#include "hmnpch.h"
#include "VulkanShaderCompiler.h"
#include "Hominem/Assets/AssetManager.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Renderer/ShaderSource.h"
#include "Hominem/Renderer/RenderFrame.h"

#include <shaderc/shaderc.hpp>

namespace Hominem {

class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
public:
    shaderc_include_result* GetInclude(const char* requested, shaderc_include_type,
                                       const char*, size_t) override
    {
        std::string uri = requested;
        if (uri.find("://") == std::string::npos)
            uri = "engine://Shaders/" + uri;

        auto* data = new IncludeData{};
        data->name = AssetManager::ResolvePath(uri);
        if (auto result = AssetManager::Load<ShaderSource>(uri))
            data->content = result->Get()->source;

        auto* result = new shaderc_include_result;
        result->source_name        = data->name.c_str();
        result->source_name_length = data->name.size();
        result->content            = data->content.c_str();
        result->content_length     = data->content.size();
        result->user_data          = data;
        return result;
    }

    void ReleaseInclude(shaderc_include_result* result) override
    {
        delete static_cast<IncludeData*>(result->user_data);
        delete result;
    }

private:
    struct IncludeData { std::string name; std::string content; };
};

static shaderc_shader_kind ToShaderKind(VulkanShaderStage stage)
{
    switch (stage)
    {
        case VulkanShaderStage::Vertex:   return shaderc_glsl_vertex_shader;
        case VulkanShaderStage::Fragment: return shaderc_glsl_fragment_shader;
        case VulkanShaderStage::Compute:  return shaderc_glsl_compute_shader;
        case VulkanShaderStage::Geometry: return shaderc_glsl_geometry_shader;
    }
    HMN_CORE_ASSERT(false, "Unknown VulkanShaderStage");
    return shaderc_glsl_vertex_shader;
}

std::vector<uint32_t> VulkanShaderCompiler::Compile(const std::string& source,
                                                      VulkanShaderStage stage,
                                                      const std::string& debugName)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetIncluder(std::make_unique<ShaderIncluder>());

    // Single source of truth: scene_common.glsl derives its SceneBuffer light-array capacity
    // from this so a shader can never fall out of sync with GPUSceneData. The active light
    // count is runtime (SceneBuffer.lightCount), so scenes add/remove lights without a rebuild.
    options.AddMacroDefinition("SCENE_MAX_LIGHTS", std::to_string(kVulkanMaxSceneLights));

    const auto result = compiler.CompileGlslToSpv(source, ToShaderKind(stage), debugName.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        HMN_CORE_ERROR("Shader compile error ({0}): {1}", debugName, result.GetErrorMessage());
        HMN_CORE_ASSERT(false, "Shader compilation failed");
        return {};
    }

    return { result.cbegin(), result.cend() };
}

}
