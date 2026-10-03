#pragma once

#include "Platform/Vulkan/VulkanCore.h"
#include "Platform/Vulkan/VulkanRenderTarget.h"

#include <functional>
#include <span>
#include <vector>
#include <glm/glm.hpp>

namespace Hominem {

class VulkanRenderer;
class SharedResources;

/// A mid-frame GL -> Vulkan -> GL handoff: images both APIs see, a semaphore each way, and
/// its own command buffer. Every image stays in VK_IMAGE_LAYOUT_GENERAL. Render thread only;
/// made by GpuInterop::CreateRoundTrip.
class InteropRoundTrip
{
public:
    struct ImageSpec
    {
        glm::uvec2 size{ 0 };
        VkFormat   format = VK_FORMAT_R16G16B16A16_SFLOAT; // RGBA16F or R32F

        bool operator==(const ImageSpec&) const = default;
    };

    InteropRoundTrip(VulkanRenderer& vk, SharedResources& gl);
    ~InteropRoundTrip();

    InteropRoundTrip(const InteropRoundTrip&)            = delete;
    InteropRoundTrip& operator=(const InteropRoundTrip&) = delete;

    /// Recreates every image when any spec changes, stalling the GPU. True when it did.
    bool SetImages(std::span<const ImageSpec> specs);

    /// GL texture -> image i, before Run. flipY turns GL's bottom-up rows into the top-down
    /// rows Vulkan-side code expects.
    void Upload(size_t i, uint32_t glTexture, bool flipY = false);
    /// Hands the images to Vulkan, records `work`, and hands them back to GL.
    void Run(const std::function<void(VkCommandBuffer)>& work);
    /// Image i -> GL texture, after Run.
    void Download(size_t i, uint32_t glTexture, bool flipY = false);

    const VulkanRenderTarget& Target(size_t i) const { return m_Targets[i]; }

private:
    void CreateImages();
    void DestroyImages();
    void Submit(const std::function<void(VkCommandBuffer)>& record, bool handoff);

    VulkanRenderer&  m_Vulkan;
    SharedResources& m_GL;

    std::vector<ImageSpec>          m_Specs;
    std::vector<VulkanRenderTarget> m_Targets;
    std::vector<uint32_t>           m_GLTextures;

    VkCommandPool   m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_Cmd         = VK_NULL_HANDLE;
    VkFence         m_Fence       = VK_NULL_HANDLE;

    VkSemaphore m_ToVulkan   = VK_NULL_HANDLE;  // GL signals, Vulkan waits
    VkSemaphore m_ToGL       = VK_NULL_HANDLE;  // Vulkan signals, GL waits
    uint32_t    m_GLToVulkan = 0;                // the same two semaphores on the GL side
    uint32_t    m_GLToGL     = 0;
};

}
