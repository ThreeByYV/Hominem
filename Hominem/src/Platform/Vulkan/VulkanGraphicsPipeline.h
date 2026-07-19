#pragma once

#include "VulkanCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Hominem {

struct GraphicsPipelineSpec
{
    std::span<const uint32_t> vertexSpirv;
    std::span<const uint32_t> fragmentSpirv;

    std::vector<VkFormat> colorFormats;
    VkFormat              depthFormat = VK_FORMAT_UNDEFINED;

    bool        depthTest    = true;
    bool        depthWrite   = true;
    VkCompareOp depthCompare = VK_COMPARE_OP_LESS;

    VkCullModeFlags     cullMode  = VK_CULL_MODE_BACK_BIT;
    VkFrontFace         frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPrimitiveTopology topology  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool                blend     = false;

    uint32_t           pushConstantBytes  = 0;
    VkShaderStageFlags pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::span<const VkDescriptorSetLayout> setLayouts;
};

class VulkanGraphicsPipeline
{
public:
    static VulkanGraphicsPipeline Create(VkDevice device, const GraphicsPipelineSpec& spec);

    void Bind(VkCommandBuffer cmd) const;
    void PushConstants(VkCommandBuffer cmd, const void* data, uint32_t size, uint32_t offset = 0) const;
    void Destroy();

    VkPipeline       GetPipeline()       const { return m_Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_Layout; }

private:
    VkDevice           m_Device   = VK_NULL_HANDLE;
    VkPipeline         m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout   m_Layout   = VK_NULL_HANDLE;
    VkShaderStageFlags m_PushConstantStages = 0;
};

}
