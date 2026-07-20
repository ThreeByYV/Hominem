#pragma once

#include "VulkanCore.h"
#include <vector>
#include <span>
#include <cstdint>

namespace Hominem {

struct ComputeBindingSpec
{
    enum class Type { StorageImage, StorageBuffer, SampledImage, UniformBuffer, AccelerationStructure };
    uint32_t binding = 0;
    Type     type    = Type::StorageImage;
};

class VulkanComputePipeline
{
public:
    static VulkanComputePipeline Create(VkDevice device,
                                        std::span<const uint32_t> spirv,
                                        std::span<const ComputeBindingSpec> bindings,
                                        uint32_t pushConstantBytes = 0,
                                        uint32_t framesInFlight = 2);

    void WriteStorageImage (uint32_t frame, uint32_t binding, VkImageView view);
    void WriteStorageBuffer(uint32_t frame, uint32_t binding,
                            VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    void WriteSampledImage (uint32_t frame, uint32_t binding,
                            VkImageView view, VkSampler sampler,
                            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void WriteAccelerationStructure(uint32_t frame, uint32_t binding, VkAccelerationStructureKHR accel);

    void PushConstants(VkCommandBuffer cmd, const void* data, uint32_t size, uint32_t offset = 0);
    void Dispatch(VkCommandBuffer cmd, uint32_t frame,
                  uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);
    void Destroy();

    VkPipeline       GetPipeline()       const { return m_Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_Layout; }

private:
    VkDevice              m_Device    = VK_NULL_HANDLE;
    VkPipeline            m_Pipeline  = VK_NULL_HANDLE;
    VkPipelineLayout      m_Layout    = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_Pool      = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_Sets;
};

}
