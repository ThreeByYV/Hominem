#include "hmnpch.h"
#include "VulkanComputePipeline.h"

namespace Hominem {

static VkDescriptorType ToVkType(ComputeBindingSpec::Type t)
{
    switch (t)
    {
        case ComputeBindingSpec::Type::StorageImage:  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case ComputeBindingSpec::Type::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ComputeBindingSpec::Type::SampledImage:  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case ComputeBindingSpec::Type::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ComputeBindingSpec::Type::AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }
    HMN_CORE_ASSERT(false, "Unknown ComputeBindingSpec::Type");
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

VulkanComputePipeline VulkanComputePipeline::Create(VkDevice device,
                                                     std::span<const uint32_t> spirv,
                                                     std::span<const ComputeBindingSpec> bindings,
                                                     uint32_t pushConstantBytes,
                                                     uint32_t framesInFlight)
{
    VulkanComputePipeline out;
    out.m_Device = device;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    layoutBindings.reserve(bindings.size());
    for (const auto&[binding, type] : bindings)
    {
        layoutBindings.push_back(VkDescriptorSetLayoutBinding
        {
            .binding         = binding,
            .descriptorType  = ToVkType(type),
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
        });
    }

    const VkDescriptorSetLayoutCreateInfo setLayoutInfo
    {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (uint32_t)layoutBindings.size(),
        .pBindings    = layoutBindings.data(),
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &out.m_SetLayout));

    const VkPushConstantRange pcRange
    {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = pushConstantBytes,
    };

    const VkPipelineLayoutCreateInfo layoutInfo
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &out.m_SetLayout,
        .pushConstantRangeCount = pushConstantBytes > 0 ? 1u : 0u,
        .pPushConstantRanges    = pushConstantBytes > 0 ? &pcRange : nullptr,
    };
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &out.m_Layout));

    const VkShaderModuleCreateInfo moduleInfo
    {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv.size_bytes(),
        .pCode    = spirv.data(),
    };
    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule));

    const VkComputePipelineCreateInfo pipelineInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage =
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName  = "main",
        },
        .layout = out.m_Layout,
    };
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out.m_Pipeline));
    vkDestroyShaderModule(device, shaderModule, nullptr);

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(bindings.size());
    for (const auto& b : bindings)
        poolSizes.push_back({ ToVkType(b.type), framesInFlight });

    const VkDescriptorPoolCreateInfo poolInfo
    {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = framesInFlight,
        .poolSizeCount = (uint32_t)poolSizes.size(),
        .pPoolSizes    = poolSizes.data(),
    };
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &out.m_Pool));

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, out.m_SetLayout);
    const VkDescriptorSetAllocateInfo allocInfo
    {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = out.m_Pool,
        .descriptorSetCount = framesInFlight,
        .pSetLayouts        = layouts.data(),
    };
    out.m_Sets.resize(framesInFlight);
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, out.m_Sets.data()));

    return out;
}

void VulkanComputePipeline::WriteStorageImage(uint32_t frame, uint32_t binding, VkImageView view)
{
    const VkDescriptorImageInfo imgInfo
    {
        .imageView   = view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkWriteDescriptorSet write
    {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = m_Sets[frame],
        .dstBinding      = binding,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo      = &imgInfo,
    };
    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanComputePipeline::WriteStorageBuffer(uint32_t frame, uint32_t binding,
                                                VkBuffer buffer, VkDeviceSize size,
                                                VkDeviceSize offset)
{
    const VkDescriptorBufferInfo bufInfo
    {
        .buffer = buffer,
        .offset = offset,
        .range  = size,
    };
    const VkWriteDescriptorSet write
    {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = m_Sets[frame],
        .dstBinding      = binding,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo     = &bufInfo,
    };
    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanComputePipeline::WriteSampledImage(uint32_t frame, uint32_t binding,
                                               VkImageView view, VkSampler sampler,
                                               VkImageLayout layout)
{
    const VkDescriptorImageInfo imgInfo
    {
        .sampler     = sampler,
        .imageView   = view,
        .imageLayout = layout,
    };
    const VkWriteDescriptorSet write
    {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = m_Sets[frame],
        .dstBinding      = binding,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &imgInfo,
    };
    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanComputePipeline::WriteAccelerationStructure(uint32_t frame, uint32_t binding,
                                                        VkAccelerationStructureKHR accel)
{
    const VkWriteDescriptorSetAccelerationStructureKHR accelInfo
    {
        .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &accel,
    };
    const VkWriteDescriptorSet write
    {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext           = &accelInfo,
        .dstSet          = m_Sets[frame],
        .dstBinding      = binding,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };
    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanComputePipeline::PushConstants(VkCommandBuffer cmd,
                                           const void* data, uint32_t size, uint32_t offset)
{
    vkCmdPushConstants(cmd, m_Layout, VK_SHADER_STAGE_COMPUTE_BIT, offset, size, data);
}

void VulkanComputePipeline::Dispatch(VkCommandBuffer cmd, uint32_t frame,
                                      uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_Layout, 0, 1, &m_Sets[frame], 0, nullptr);
    vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
}

void VulkanComputePipeline::Destroy()
{
    if (m_Device == VK_NULL_HANDLE) return;
    vkDestroyDescriptorPool(m_Device, m_Pool, nullptr);
    vkDestroyDescriptorSetLayout(m_Device, m_SetLayout, nullptr);
    vkDestroyPipelineLayout(m_Device, m_Layout, nullptr);
    vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    m_Device = VK_NULL_HANDLE;
}

}
