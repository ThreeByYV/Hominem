#include "hmnpch.h"
#include "VulkanComputeShader.h"
#include "VulkanShaderCompiler.h"

namespace Hominem {

VulkanComputeShader VulkanComputeShader::Create(VkDevice device,
                                                 const std::string& glslSource,
                                                 const std::string& debugName,
                                                 std::span<const ComputeBindingSpec> bindings)
{
    auto spirv = VulkanShaderCompiler::Compile(
        glslSource, VulkanShaderStage::Compute, debugName + ".comp");

    VulkanComputeShader shader;
    shader.m_Pipeline = VulkanComputePipeline::Create(device, spirv, bindings);
    shader.m_Name     = debugName;
    return shader;
}

void VulkanComputeShader::WriteStorageImage(uint32_t frameIdx, uint32_t binding, VkImageView view)
{
    m_Pipeline.WriteStorageImage(frameIdx, binding, view);
}

void VulkanComputeShader::WriteStorageBuffer(uint32_t frameIdx, uint32_t binding,
                                              VkBuffer buffer, VkDeviceSize size,
                                              VkDeviceSize offset)
{
    m_Pipeline.WriteStorageBuffer(frameIdx, binding, buffer, size, offset);
}

void VulkanComputeShader::WriteSampledImage(uint32_t frameIdx, uint32_t binding,
                                             VkImageView view, VkSampler sampler,
                                             VkImageLayout layout)
{
    m_Pipeline.WriteSampledImage(frameIdx, binding, view, sampler, layout);
}

void VulkanComputeShader::Dispatch(VkCommandBuffer cmd, uint32_t frameIdx,
                                    uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    m_Pipeline.Dispatch(cmd, frameIdx, groupsX, groupsY, groupsZ);
}

void VulkanComputeShader::Destroy()
{
    m_Pipeline.Destroy();
}

}
