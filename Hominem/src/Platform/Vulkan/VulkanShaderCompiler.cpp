#include "hmnpch.h"
#include "VulkanShaderCompiler.h"

#include <shaderc/shaderc.hpp>

namespace Hominem {

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
