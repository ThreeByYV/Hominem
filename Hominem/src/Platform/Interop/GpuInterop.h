#pragma once

#include "InteropRoundTrip.h"

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/RHI/SharedImages.h"
#include "Hominem/Renderer/RHI/SharedResources.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Hominem {

class VulkanSceneRenderer;
class VulkanRenderer;

/// The GL <-> Vulkan link: one owner for the GPU match check, the GL side of every shared
/// image and semaphore, and teardown order. Two ways to hand work across:
///   - per frame: Vulkan's frame work (draw image, DDGI results) -> the GL passes
///   - round trip: GL -> Vulkan -> GL mid-frame (the upscaler), via CreateRoundTrip
/// Render thread only.
class GpuInterop
{
public:
    /// The GL device, so Vulkan can pick the same GPU before the link exists.
    static std::array<uint8_t, 8> GLDeviceLUID() { return SharedResources::GetDeviceLUID(); }
    static std::string            GLDeviceName() { return SharedResources::GetDeviceName(); }

    /// Inactive (and every call a no-op) when the driver lacks the extensions or GL and
    /// Vulkan landed on different GPUs.
    void Init(VulkanSceneRenderer& vulkan, uint32_t w, uint32_t h);
    void Shutdown();

    bool IsActive() const { return m_GL != nullptr; }

    VulkanRenderer& GetVulkan();
    uint32_t        GetDrawTexture() const { return m_GL ? m_GL->GetTextureID() : 0; }

    /// After Vulkan's frame is submitted and before the GL passes: re-imports the published
    /// images when they changed and makes GL wait. Returns the new table when it changed.
    std::shared_ptr<const SharedImageTable> AcquireFrame();
    /// After the GL passes: hands the shared images back to Vulkan.
    void ReleaseFrame();

    Scope<InteropRoundTrip> CreateRoundTrip();

private:
    std::shared_ptr<const SharedImageTable> SyncSharedImages();

    VulkanSceneRenderer*             m_Vulkan = nullptr;
    std::unique_ptr<SharedResources> m_GL;
    uint32_t                         m_FrameIdx = 0;
    bool                             m_FrameAcquired = false;

    uint32_t              m_SharedImageGeneration = 0;
    std::vector<uint32_t> m_ImportedImages;
};

}
