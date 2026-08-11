#include "hmnpch.h"
#include "VulkanImage.h"

#include <algorithm>
#include <cmath>

namespace Hominem {

VulkanAllocatedImage VulkanImage::Create(VkDevice device, VmaAllocator allocator,
                                         VkExtent3D extent, VkFormat format,
                                         VkImageUsageFlags usage, bool mipmapped)
{
    VulkanAllocatedImage img;
    img.extent = extent;
    img.format = format;

    uint32_t mipLevels = 1;
    if (mipmapped)
        mipLevels = (uint32_t)std::floor(std::log2f((float)std::max(extent.width, extent.height))) + 1;

    const VkImageCreateInfo imageInfo
    {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = extent.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = extent,
        .mipLevels     = mipLevels,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VmaAllocationCreateInfo allocInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VK_CHECK(vmaCreateImage(allocator, &imageInfo, &allocInfo,
                            &img.image, &img.allocation, nullptr));

    const bool isDepth = (format == VK_FORMAT_D32_SFLOAT      ||
                          format == VK_FORMAT_D24_UNORM_S8_UINT ||
                          format == VK_FORMAT_D16_UNORM);

    const VkImageViewCreateInfo viewInfo
    {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = img.image,
        .viewType = extent.depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D,
        .format   = format,
        .subresourceRange =
        {
            .aspectMask     = static_cast<VkImageAspectFlags>(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            .baseMipLevel   = 0,
            .levelCount     = mipLevels,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &img.imageView));
    return img;
}

static uint32_t FindMemoryType(VkPhysicalDevice physical, uint32_t typeBits,
                               VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    HMN_CORE_ASSERT(false, "Failed to find suitable memory type");
    return ~0u;
}

VulkanAllocatedImage VulkanImage::CreateShared(VkDevice device, VkPhysicalDevice physical,
                                               VkExtent3D extent, VkFormat format,
                                               VkImageUsageFlags usage)
{
    VulkanAllocatedImage img;
    img.extent = extent;
    img.format = format;

    const VkExternalMemoryImageCreateInfo extMemInfo
    {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkImageCreateInfo imageInfo
    {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext         = &extMemInfo,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = extent,
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &img.image));

    // Drivers differ on whether an external-memory image needs its own allocation -
    // Intel demands it, NVIDIA doesn't - so ask rather than assume.
    const VkImageMemoryRequirementsInfo2 reqInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .image = img.image,
    };
    VkMemoryDedicatedRequirements dedicatedReqs
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
    };
    VkMemoryRequirements2 memReqs2
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = &dedicatedReqs,
    };
    vkGetImageMemoryRequirements2(device, &reqInfo, &memReqs2);

    const VkMemoryRequirements& memReqs = memReqs2.memoryRequirements;
    img.memorySize = memReqs.size;

    const VkMemoryDedicatedAllocateInfo dedicatedInfo
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = img.image,
    };
    const VkExportMemoryAllocateInfo exportInfo
    {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext       = (dedicatedReqs.requiresDedicatedAllocation ||
                        dedicatedReqs.prefersDedicatedAllocation) ? &dedicatedInfo : nullptr,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkMemoryAllocateInfo allocInfo
    {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &exportInfo,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = FindMemoryType(physical, memReqs.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &img.memory));
    VK_CHECK(vkBindImageMemory(device, img.image, img.memory, 0));

    const VkImageViewCreateInfo viewInfo
    {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = img.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = format,
        .subresourceRange =
        {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &img.imageView));
    return img;
}

HANDLE VulkanImage::GetWin32Handle(VkDevice device, const VulkanAllocatedImage& img)
{
    if (img.memory == VK_NULL_HANDLE) return nullptr;

    const VkMemoryGetWin32HandleInfoKHR info
    {
        .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
        .memory     = img.memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    HANDLE handle = nullptr;
    VK_CHECK(vkGetMemoryWin32HandleKHR(device, &info, &handle));
    return handle;
}

void VulkanImage::Destroy(VkDevice device, VmaAllocator allocator,
                          const VulkanAllocatedImage& img)
{
    vkDestroyImageView(device, img.imageView, nullptr);
    if (img.memory != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, img.image, nullptr);
        vkFreeMemory(device, img.memory, nullptr);
        return;
    }
    vmaDestroyImage(allocator, img.image, img.allocation);
}

static void SubmitBarrier(VkCommandBuffer cmd, const VkImageMemoryBarrier2& barrier)
{
    const VkDependencyInfo depInfo
    {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &depInfo);
}

static VkImageMemoryBarrier2 ColorBarrier(VkImage image,
                                           VkPipelineStageFlags2 srcStage,  VkAccessFlags2 srcAccess,
                                           VkPipelineStageFlags2 dstStage,  VkAccessFlags2 dstAccess,
                                           VkImageLayout         oldLayout, VkImageLayout  newLayout)
{
    return VkImageMemoryBarrier2
    {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask  = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout     = oldLayout,
        .newLayout     = newLayout,
        .image         = image,
        .subresourceRange =
        {
            .aspectMask     = (VkImageAspectFlags)VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        },
    };
}

void VulkanImage::TransitionUndefinedToGeneral(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,    VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,               VK_IMAGE_LAYOUT_GENERAL));
}

void VulkanImage::TransitionGeneralToShaderRead(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL,                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void VulkanImage::TransitionShaderReadToGeneral(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL));
}

void VulkanImage::TransitionColorAttachmentToPresent(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,          VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
}

void VulkanImage::TransitionColorAttachmentToTransferSrc(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,                VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
}

void VulkanImage::TransitionUndefinedToTransferDst(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,    VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
}

void VulkanImage::TransitionTransferDstToPresent(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,    VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
}

void VulkanImage::CopyImageToImage(VkCommandBuffer cmd,
                                    VkImage src, VkExtent2D srcSize,
                                    VkImage dst, VkExtent2D dstSize)
{
    const VkImageBlit2 region
    {
        .sType          = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .srcSubresource =
        {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
        .srcOffsets     = { {}, { (int32_t)srcSize.width, (int32_t)srcSize.height, 1 } },
        .dstSubresource =
        {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
        .dstOffsets     = { {}, { (int32_t)dstSize.width, (int32_t)dstSize.height, 1 } },
    };

    const VkBlitImageInfo2 blitInfo
    {
        .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .srcImage       = src,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage       = dst,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount    = 1,
        .pRegions       = &region,
        .filter         = VK_FILTER_LINEAR,
    };
    vkCmdBlitImage2(cmd, &blitInfo);
}

void VulkanImage::TransitionUndefinedToColorAttachment(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,             VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
}

void VulkanImage::TransitionGeneralToTransferSrc(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL,                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
}

void VulkanImage::TransitionShaderReadToColorAttachment(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,          VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
}

void VulkanImage::TransitionGeneralToColorAttachment(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,           VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                                                          VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL,                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
}

void VulkanImage::TransitionColorAttachmentToShaderRead(VkCommandBuffer cmd, VkImage image)
{
    SubmitBarrier(cmd, ColorBarrier(image,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,         VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void VulkanImage::TransitionUndefinedToDepthAttachment(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier = ColorBarrier(image,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,                       VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    SubmitBarrier(cmd, barrier);
}

}
