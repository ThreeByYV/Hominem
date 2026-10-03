#include "hmnpch.h"
#include "InteropRoundTrip.h"

#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanImage.h"
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

SharedImageFormat ToShared(VkFormat format)
{
    HMN_CORE_ASSERT(format == VK_FORMAT_R16G16B16A16_SFLOAT || format == VK_FORMAT_R32_SFLOAT,
                    "InteropRoundTrip: only RGBA16F and R32F are shared");
    return format == VK_FORMAT_R32_SFLOAT ? SharedImageFormat::R32F : SharedImageFormat::RGBA16F;
}

}

InteropRoundTrip::InteropRoundTrip(VulkanRenderer& vk, SharedResources& gl)
    : m_Vulkan(vk), m_GL(gl)
{
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

InteropRoundTrip::~InteropRoundTrip()
{
    const VkDevice device = m_Vulkan.GetDevice();
    vkDeviceWaitIdle(device);

    DestroyImages();
    m_GL.DeleteSemaphore(m_GLToVulkan);
    m_GL.DeleteSemaphore(m_GLToGL);
    vkDestroySemaphore(device, m_ToVulkan, nullptr);
    vkDestroySemaphore(device, m_ToGL, nullptr);
    vkDestroyFence(device, m_Fence, nullptr);
    vkDestroyCommandPool(device, m_CommandPool, nullptr);
}

bool InteropRoundTrip::SetImages(std::span<const ImageSpec> specs)
{
    if (std::ranges::equal(specs, m_Specs) && !m_Targets.empty()) return false;

    vkDeviceWaitIdle(m_Vulkan.GetDevice());
    DestroyImages();
    m_Specs.assign(specs.begin(), specs.end());
    CreateImages();
    return true;
}

void InteropRoundTrip::CreateImages()
{
    const VkDevice         device   = m_Vulkan.GetDevice();
    const VkPhysicalDevice physical = m_Vulkan.GetPhysical();

    m_Targets.resize(m_Specs.size());
    m_GLTextures.assign(m_Specs.size(), 0);
    for (size_t i = 0; i < m_Specs.size(); i++)
        m_Targets[i] = VulkanRenderTarget::CreateShared(device, physical, m_Specs[i].size.x, m_Specs[i].size.y,
                                                        m_Specs[i].format);

    // GL touches them first, and the semaphores describe them as GENERAL from then on.
    Submit([this](VkCommandBuffer cmd)
    {
        for (const auto& target : m_Targets)
            VulkanImage::TransitionUndefinedToGeneral(cmd, target.GetImage());
    }, false);
    VK_CHECK(vkWaitForFences(device, 1, &m_Fence, VK_TRUE, UINT64_MAX));

    for (size_t i = 0; i < m_Specs.size(); i++)
    {
        SharedImageDesc desc;
        desc.memHandle     = m_Targets[i].GetWin32Handle(device);
        desc.memSize       = m_Targets[i].GetMemorySize();
        desc.width         = m_Specs[i].size.x;
        desc.height        = m_Specs[i].size.y;
        desc.generalLayout = true;
        desc.format        = ToShared(m_Specs[i].format);
        desc.frameSync     = false;
        m_GLTextures[i] = m_GL.ImportSharedImage(desc);
        CloseHandle(desc.memHandle);
        if (!m_GLTextures[i])
            HMN_CORE_ERROR("InteropRoundTrip: GL import of image {0} failed", i);
    }
}

void InteropRoundTrip::DestroyImages()
{
    for (size_t i = 0; i < m_Targets.size(); i++)
    {
        if (m_GLTextures[i]) m_GL.ReleaseSharedImage(m_GLTextures[i]);
        if (m_Targets[i].GetImage() != VK_NULL_HANDLE)
            m_Targets[i].Destroy(m_Vulkan.GetDevice(), m_Vulkan.GetAllocator());
    }
    m_Targets.clear();
    m_GLTextures.clear();
}

void InteropRoundTrip::Upload(size_t i, uint32_t glTexture, bool flipY)
{
    m_GL.CopyTexture(glTexture, m_GLTextures[i], m_Specs[i].size.x, m_Specs[i].size.y, flipY);
}

void InteropRoundTrip::Download(size_t i, uint32_t glTexture, bool flipY)
{
    m_GL.CopyTexture(m_GLTextures[i], glTexture, m_Specs[i].size.x, m_Specs[i].size.y, flipY);
}

void InteropRoundTrip::Run(const std::function<void(VkCommandBuffer)>& work)
{
    // Every image goes both ways: inputs because GL wrote them, outputs because GL reads
    // them after and Vulkan writes them again next frame.
    m_GL.SignalOn(m_GLToVulkan, m_GLTextures);
    Submit(work, true);
    m_GL.WaitOn(m_GLToGL, m_GLTextures);
}

void InteropRoundTrip::Submit(const std::function<void(VkCommandBuffer)>& record, bool handoff)
{
    const VkDevice device = m_Vulkan.GetDevice();
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

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkSubmitInfo submit
    {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = handoff ? 1u : 0u,
        .pWaitSemaphores      = &m_ToVulkan,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &m_Cmd,
        .signalSemaphoreCount = handoff ? 1u : 0u,
        .pSignalSemaphores    = &m_ToGL,
    };
    VK_CHECK(vkQueueSubmit(m_Vulkan.GetGraphicsQueue(), 1, &submit, m_Fence));
}

}
