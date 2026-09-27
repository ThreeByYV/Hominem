#include "hmnpch.h"
#include "VulkanUpscalerBridge.h"
#include "VulkanRenderer.h"
#include "VulkanImage.h"

#include "Hominem/Renderer/RHI/SharedResources.h"

namespace Hominem {

namespace {

VkSemaphore CreateExportedSemaphore(VkDevice device)
{
    const VkExportSemaphoreCreateInfo exportInfo
    {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkSemaphoreCreateInfo info
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &exportInfo,
    };
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
}

HANDLE ExportSemaphore(VkDevice device, VkSemaphore semaphore)
{
    const VkSemaphoreGetWin32HandleInfoKHR info
    {
        .sType      = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore  = semaphore,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    HANDLE handle = nullptr;
    VK_CHECK(vkGetSemaphoreWin32HandleKHR(device, &info, &handle));
    return handle;
}

}

void VulkanUpscalerBridge::Init(VulkanRenderer& vk, SharedResources& gl)
{
    m_Vulkan = &vk;
    m_GL     = &gl;
    const VkDevice device = vk.GetDevice();

    const VkCommandPoolCreateInfo poolInfo
    {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk.GetGraphicsFamily(),
    };
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &m_CommandPool));

    const VkCommandBufferAllocateInfo cmdInfo
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = m_CommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdInfo, &m_Cmd));

    const VkFenceCreateInfo fenceInfo
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_Fence));

    // Importing an NT handle doesn't take ownership of it, so it's closed right after.
    m_ToVulkan = CreateExportedSemaphore(device);
    m_ToGL     = CreateExportedSemaphore(device);
    for (auto [vkSem, glSem] : { std::pair{ m_ToVulkan, &m_GLToVulkan }, std::pair{ m_ToGL, &m_GLToGL } })
    {
        HANDLE handle = ExportSemaphore(device, vkSem);
        *glSem = gl.ImportSemaphoreHandle(handle);
        CloseHandle(handle);
    }
}

void VulkanUpscalerBridge::Shutdown()
{
    if (!m_Vulkan) return;
    const VkDevice device = m_Vulkan->GetDevice();
    vkDeviceWaitIdle(device);

    DestroyImages();
    m_GL->DeleteSemaphore(m_GLToVulkan);
    m_GL->DeleteSemaphore(m_GLToGL);
    vkDestroySemaphore(device, m_ToVulkan, nullptr);
    vkDestroySemaphore(device, m_ToGL, nullptr);
    vkDestroyFence(device, m_Fence, nullptr);
    vkDestroyCommandPool(device, m_CommandPool, nullptr);
    m_Vulkan = nullptr;
}

void VulkanUpscalerBridge::Resize(glm::uvec2 renderSize, glm::uvec2 outputSize)
{
    if (renderSize == m_RenderSize && outputSize == m_OutputSize && m_GLTextures[Color]) return;

    vkDeviceWaitIdle(m_Vulkan->GetDevice());
    DestroyImages();
    m_RenderSize = renderSize;
    m_OutputSize = outputSize;
    CreateImages();

    HMN_CORE_INFO("Upscaler bridge: {0}x{1} -> {2}x{3}", renderSize.x, renderSize.y, outputSize.x, outputSize.y);
}

void VulkanUpscalerBridge::CreateImages()
{
    const VkDevice         device   = m_Vulkan->GetDevice();
    const VkPhysicalDevice physical = m_Vulkan->GetPhysical();

    struct Spec { glm::uvec2 size; VkFormat vkFormat; SharedImageFormat glFormat; };
    const Spec specs[Count] =
    {
        { m_RenderSize, VK_FORMAT_R16G16B16A16_SFLOAT, SharedImageFormat::RGBA16F }, // Color
        { m_RenderSize, VK_FORMAT_R16G16B16A16_SFLOAT, SharedImageFormat::RGBA16F }, // Velocity (RG used)
        { m_RenderSize, VK_FORMAT_R32_SFLOAT,          SharedImageFormat::R32F    }, // Depth
        { m_OutputSize, VK_FORMAT_R16G16B16A16_SFLOAT, SharedImageFormat::RGBA16F }, // Output
    };

    for (uint32_t i = 0; i < Count; i++)
        m_Targets[i] = VulkanRenderTarget::CreateShared(device, physical, specs[i].size.x, specs[i].size.y,
                                                        specs[i].vkFormat);

    // GL touches them first, and the semaphores describe them as GENERAL from then on.
    SubmitAndWait([this](VkCommandBuffer cmd)
    {
        for (const auto& target : m_Targets)
            VulkanImage::TransitionUndefinedToGeneral(cmd, target.GetImage());
    });

    for (uint32_t i = 0; i < Count; i++)
    {
        SharedImageDesc desc;
        desc.memHandle     = m_Targets[i].GetWin32Handle(device);
        desc.memSize       = m_Targets[i].GetMemorySize();
        desc.width         = specs[i].size.x;
        desc.height        = specs[i].size.y;
        desc.generalLayout = true;
        desc.format        = specs[i].glFormat;
        desc.frameSync     = false;
        m_GLTextures[i] = m_GL->ImportSharedImage(desc);
        CloseHandle(desc.memHandle);
        if (!m_GLTextures[i])
            HMN_CORE_ERROR("Upscaler bridge: GL import of image {0} failed", i);
    }
}

void VulkanUpscalerBridge::DestroyImages()
{
    for (uint32_t i = 0; i < Count; i++)
    {
        if (m_GLTextures[i]) m_GL->ReleaseSharedImage(m_GLTextures[i]);
        if (m_Targets[i].GetImage() != VK_NULL_HANDLE)
            m_Targets[i].Destroy(m_Vulkan->GetDevice(), m_Vulkan->GetAllocator());
        m_GLTextures[i] = 0;
        m_Targets[i]    = {};
    }
}

void VulkanUpscalerBridge::SubmitAndWait(const std::function<void(VkCommandBuffer)>& record)
{
    const VkDevice device = m_Vulkan->GetDevice();
    VK_CHECK(vkWaitForFences(device, 1, &m_Fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &m_Fence));
    VK_CHECK(vkResetCommandBuffer(m_Cmd, 0));

    const VkCommandBufferBeginInfo begin
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(m_Cmd, &begin));
    record(m_Cmd);
    VK_CHECK(vkEndCommandBuffer(m_Cmd));

    const VkSubmitInfo submit
    {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &m_Cmd,
    };
    VK_CHECK(vkQueueSubmit(m_Vulkan->GetGraphicsQueue(), 1, &submit, m_Fence));
    VK_CHECK(vkWaitForFences(device, 1, &m_Fence, VK_TRUE, UINT64_MAX));
}

void VulkanUpscalerBridge::RecordBlit(VkCommandBuffer cmd) const
{
    const VkImageBlit region
    {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets     = { { 0, 0, 0 }, { (int32_t)m_RenderSize.x, (int32_t)m_RenderSize.y, 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets     = { { 0, 0, 0 }, { (int32_t)m_OutputSize.x, (int32_t)m_OutputSize.y, 1 } },
    };
    vkCmdBlitImage(cmd, m_Targets[Color].GetImage(),  VK_IMAGE_LAYOUT_GENERAL,
                        m_Targets[Output].GetImage(), VK_IMAGE_LAYOUT_GENERAL,
                        1, &region, VK_FILTER_LINEAR);
}

void VulkanUpscalerBridge::Run(const std::function<void(VkCommandBuffer)>& work)
{
    const VkDevice device = m_Vulkan->GetDevice();

    // Every image goes both ways: the inputs because GL wrote them, the output because GL
    // reads it after and Vulkan writes it again next frame.
    m_GL->SignalOn(m_GLToVulkan, m_GLTextures);

    VK_CHECK(vkWaitForFences(device, 1, &m_Fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &m_Fence));
    VK_CHECK(vkResetCommandBuffer(m_Cmd, 0));

    const VkCommandBufferBeginInfo begin
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(m_Cmd, &begin));
    work(m_Cmd);
    VK_CHECK(vkEndCommandBuffer(m_Cmd));

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkSubmitInfo submit
    {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &m_ToVulkan,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &m_Cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &m_ToGL,
    };
    VK_CHECK(vkQueueSubmit(m_Vulkan->GetGraphicsQueue(), 1, &submit, m_Fence));

    m_GL->WaitOn(m_GLToGL, m_GLTextures);
}

}
