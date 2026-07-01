#pragma once

#include "VulkanCore.h"
#include "VulkanComputePipeline.h"
#include <string>
#include <vector>

namespace Hominem {

class VulkanComputeShader
{
public:
    static VulkanComputeShader Create(VkDevice device,
                                      const std::string& glslSource,
                                      const std::string& debugName,
                                      std::span<const ComputeBindingSpec> bindings);

    void Dispatch(VkCommandBuffer cmd, uint32_t frameIdx,
                  uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ = 1);

    void WriteStorageImage (uint32_t frameIdx, uint32_t binding, VkImageView view);
    void WriteStorageBuffer(uint32_t frameIdx, uint32_t binding,
                            VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    void WriteSampledImage (uint32_t frameIdx, uint32_t binding,
                            VkImageView view, VkSampler sampler,
                            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    void Destroy();

    const std::string& GetName() const { return m_Name; }

private:
    VulkanComputePipeline m_Pipeline;
    std::string           m_Name;
};

}
