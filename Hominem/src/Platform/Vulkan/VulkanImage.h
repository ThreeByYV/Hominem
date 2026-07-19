#pragma once

#include "VulkanCore.h"
#include <cstdint>

namespace Hominem {

struct VulkanAllocatedImage
{
    VkImage       image      = VK_NULL_HANDLE;
    VkImageView   imageView  = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D    extent     = {};
    VkFormat      format     = VK_FORMAT_UNDEFINED;
};

namespace VulkanImage {

VulkanAllocatedImage Create(VkDevice device, VmaAllocator allocator,
                            VkExtent3D extent, VkFormat format,
                            VkImageUsageFlags usage,
                            bool mipmapped = false);

void Destroy(VkDevice device, VmaAllocator allocator, const VulkanAllocatedImage& img);

void TransitionUndefinedToGeneral        (VkCommandBuffer cmd, VkImage image);

void TransitionGeneralToShaderRead       (VkCommandBuffer cmd, VkImage image);

void TransitionShaderReadToGeneral       (VkCommandBuffer cmd, VkImage image);

void TransitionColorAttachmentToPresent    (VkCommandBuffer cmd, VkImage image);
void TransitionColorAttachmentToTransferSrc(VkCommandBuffer cmd, VkImage image);
void TransitionUndefinedToTransferDst      (VkCommandBuffer cmd, VkImage image);
void TransitionTransferDstToPresent        (VkCommandBuffer cmd, VkImage image);

void TransitionUndefinedToColorAttachment(VkCommandBuffer cmd, VkImage image);
void TransitionGeneralToTransferSrc      (VkCommandBuffer cmd, VkImage image);

void TransitionShaderReadToColorAttachment(VkCommandBuffer cmd, VkImage image);
void TransitionGeneralToColorAttachment   (VkCommandBuffer cmd, VkImage image);
void TransitionColorAttachmentToShaderRead(VkCommandBuffer cmd, VkImage image);
void TransitionUndefinedToDepthAttachment (VkCommandBuffer cmd, VkImage image);

void CopyImageToImage(VkCommandBuffer cmd,
                      VkImage src, VkExtent2D srcSize,
                      VkImage dst, VkExtent2D dstSize);

}
}
