#pragma once

#include "VulkanCore.h"
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

struct VulkanAllocatedImage
{
    VkImage       image      = VK_NULL_HANDLE;
    VkImageView   imageView  = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D    extent     = {};
    VkFormat      format     = VK_FORMAT_UNDEFINED;

    // CreateShared only — allocated outside VMA to carry VkExportMemoryAllocateInfo.
    VkDeviceMemory memory     = VK_NULL_HANDLE;
    VkDeviceSize   memorySize = 0;
};

namespace VulkanImage {

VulkanAllocatedImage Create(VkDevice device, VmaAllocator allocator,
                            VkExtent3D extent, VkFormat format,
                            VkImageUsageFlags usage,
                            bool mipmapped = false);

/// Image OpenGL can import over the same memory. Bypasses VMA, which can't attach
/// VkExportMemoryAllocateInfo to an allocation.
VulkanAllocatedImage CreateShared(VkDevice device, VkPhysicalDevice physical,
                                  VkExtent3D extent, VkFormat format,
                                  VkImageUsageFlags usage);

/// Caller owns the returned handle and must CloseHandle it after the importer takes a reference.
HANDLE GetWin32Handle(VkDevice device, const VulkanAllocatedImage& img);

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
