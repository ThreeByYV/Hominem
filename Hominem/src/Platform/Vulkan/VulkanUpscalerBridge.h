#pragma once

#include "VulkanCore.h"
#include "VulkanRenderTarget.h"

#include <array>
#include <functional>
#include <glm/glm.hpp>

namespace Hominem {

class VulkanRenderer;
class SharedResources;

/// Hands the GL scene to a Vulkan upscaler mid-frame and the result back. Owns the shared
/// images (color, velocity, depth at render size; output at output size) and a semaphore
/// each way. Every image stays in VK_IMAGE_LAYOUT_GENERAL. Render thread only.
class VulkanUpscalerBridge
{
public:
    enum Image : uint32_t { Color, Velocity, Depth, Output, Count };

    void Init(VulkanRenderer& vk, SharedResources& gl);
    void Shutdown();

    /// Recreates the shared images when either size changes; stalls the GPU when it does.
    void Resize(glm::uvec2 renderSize, glm::uvec2 outputSize);

    /// Hands the images to Vulkan, records `work`, and hands them back to GL.
    void Run(const std::function<void(VkCommandBuffer)>& work);

    /// Bilinear blit of Color into Output.
    void RecordBlit(VkCommandBuffer cmd) const;

    const VulkanRenderTarget& Target(Image i) const    { return m_Targets[i]; }
    uint32_t                  GLTexture(Image i) const { return m_GLTextures[i]; }
    glm::uvec2                RenderSize() const       { return m_RenderSize; }
    glm::uvec2                OutputSize() const       { return m_OutputSize; }

private:
    void CreateImages();
    void DestroyImages();
    void SubmitAndWait(const std::function<void(VkCommandBuffer)>& record);

    VulkanRenderer*  m_Vulkan = nullptr;
    SharedResources* m_GL     = nullptr;

    std::array<VulkanRenderTarget, Count> m_Targets{};
    std::array<uint32_t, Count>           m_GLTextures{};
    glm::uvec2                            m_RenderSize{ 0 };
    glm::uvec2                            m_OutputSize{ 0 };

    VkCommandPool   m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_Cmd         = VK_NULL_HANDLE;
    VkFence         m_Fence       = VK_NULL_HANDLE;

    VkSemaphore m_ToVulkan   = VK_NULL_HANDLE;  // GL signals, Vulkan waits
    VkSemaphore m_ToGL       = VK_NULL_HANDLE;  // Vulkan signals, GL waits
    uint32_t    m_GLToVulkan = 0;                // the same two semaphores on the GL side
    uint32_t    m_GLToGL     = 0;
};

}
