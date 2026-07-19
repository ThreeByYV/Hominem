#include "hmnpch.h"
#include "VulkanGraphicsPipeline.h"

#include <array>

namespace Hominem {

static VkShaderModule CreateShaderModule(VkDevice device, std::span<const uint32_t> spirv)
{
    const VkShaderModuleCreateInfo moduleInfo {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv.size_bytes(),
        .pCode    = spirv.data(),
    };
    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule));
    return shaderModule;
}

VulkanGraphicsPipeline VulkanGraphicsPipeline::Create(VkDevice device, const GraphicsPipelineSpec& spec)
{
    VulkanGraphicsPipeline out;
    out.m_Device             = device;
    out.m_PushConstantStages = spec.pushConstantStages;

    const VkPushConstantRange pcRange {
        .stageFlags = spec.pushConstantStages,
        .offset     = 0,
        .size       = spec.pushConstantBytes,
    };
    const VkPipelineLayoutCreateInfo layoutInfo {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = (uint32_t)spec.setLayouts.size(),
        .pSetLayouts            = spec.setLayouts.data(),
        .pushConstantRangeCount = spec.pushConstantBytes > 0 ? 1u : 0u,
        .pPushConstantRanges    = spec.pushConstantBytes > 0 ? &pcRange : nullptr,
    };
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &out.m_Layout));

    VkShaderModule vertModule = CreateShaderModule(device, spec.vertexSpirv);
    VkShaderModule fragModule = CreateShaderModule(device, spec.fragmentSpirv);

    const std::array stages {
        VkPipelineShaderStageCreateInfo {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertModule,
            .pName  = "main",
        },
        VkPipelineShaderStageCreateInfo {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragModule,
            .pName  = "main",
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = spec.topology,
    };

    const VkPipelineViewportStateCreateInfo viewportState {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = spec.cullMode,
        .frontFace   = spec.frontFace,
        .lineWidth   = 1.f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencil {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = spec.depthTest  ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = spec.depthWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp   = spec.depthCompare,
        .maxDepthBounds   = 1.f,
    };

    VkPipelineColorBlendAttachmentState blendAttachment {
        .blendEnable    = spec.blend ? VK_TRUE : VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    if (spec.blend)
    {
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(spec.colorFormats.size(), blendAttachment);
    const VkPipelineColorBlendStateCreateInfo colorBlend {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = (uint32_t)blendAttachments.size(),
        .pAttachments    = blendAttachments.data(),
    };

    const std::array dynamicStates { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    const VkPipelineDynamicStateCreateInfo dynamicState {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (uint32_t)dynamicStates.size(),
        .pDynamicStates    = dynamicStates.data(),
    };

    const VkPipelineRenderingCreateInfo renderingInfo {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = (uint32_t)spec.colorFormats.size(),
        .pColorAttachmentFormats = spec.colorFormats.data(),
        .depthAttachmentFormat   = spec.depthFormat,
    };

    const VkGraphicsPipelineCreateInfo pipelineInfo {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderingInfo,
        .stageCount          = (uint32_t)stages.size(),
        .pStages             = stages.data(),
        .pVertexInputState   = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState    = &colorBlend,
        .pDynamicState       = &dynamicState,
        .layout              = out.m_Layout,
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out.m_Pipeline));

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    return out;
}

void VulkanGraphicsPipeline::Bind(VkCommandBuffer cmd) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

void VulkanGraphicsPipeline::PushConstants(VkCommandBuffer cmd, const void* data,
                                           uint32_t size, uint32_t offset) const
{
    vkCmdPushConstants(cmd, m_Layout, m_PushConstantStages, offset, size, data);
}

void VulkanGraphicsPipeline::Destroy()
{
    if (m_Device == VK_NULL_HANDLE) return;
    vkDestroyPipelineLayout(m_Device, m_Layout, nullptr);
    vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    m_Device = VK_NULL_HANDLE;
}

}
