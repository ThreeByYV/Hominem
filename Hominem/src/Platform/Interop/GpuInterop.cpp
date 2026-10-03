#include "hmnpch.h"
#include "GpuInterop.h"

#include "Platform/Vulkan/VulkanSceneRenderer.h"

namespace Hominem {

namespace {

constexpr uint32_t k_FramesInFlight = 2;

}

void GpuInterop::Init(VulkanSceneRenderer& vulkan, uint32_t w, uint32_t h)
{
    m_Vulkan = &vulkan;

    if (!SharedResources::IsInteropSupported())
    {
        HMN_CORE_WARN("GpuInterop: driver lacks GL/VK external-memory extensions, shared texture unavailable");
        return;
    }

    const auto glLUID = GLDeviceLUID();
    if (glLUID == std::array<uint8_t, 8>{})
    {
        HMN_CORE_WARN("GpuInterop: GL LUID unavailable (GL_EXT_memory_object not exposed), skipping LUID check");
    }
    else if (vulkan.GetDeviceLUID() != glLUID)
    {
        HMN_CORE_WARN("GpuInterop: VK and GL are on different GPUs, shared texture unavailable");
        return;
    }

    VulkanRenderer& vk = vulkan.GetRenderer();
    m_GL = SharedResources::Create();
    m_GL->ImportSharedTexture(vk.GetDrawImageWin32Handle(), vk.GetDrawImageMemorySize(), w, h);

    // One semaphore per frame in flight, so frame N+1 can't overwrite before N is signalled.
    for (uint32_t i = 0; i < k_FramesInFlight; ++i)
        m_GL->ImportSemaphore(i, vk.GetComputeDoneSemaphoreWin32Handle(i));
    m_GL->ImportGLDoneSemaphore(vk.GetGLDoneSemaphoreWin32Handle());
}

void GpuInterop::Shutdown()
{
    if (m_GL)
    {
        m_GL->Destroy();
        m_GL.reset();
    }
    m_ImportedImages.clear();
    m_SharedImageGeneration = 0;
    m_Vulkan = nullptr;
}

VulkanRenderer& GpuInterop::GetVulkan()
{
    return m_Vulkan->GetRenderer();
}

std::shared_ptr<const SharedImageTable> GpuInterop::AcquireFrame()
{
    if (!m_GL) return nullptr;

    // Import before the wait: the semaphore acquires every shared texture at once, and what
    // the GL passes are about to sample must be in that set.
    auto table = SyncSharedImages();
    m_GL->WaitSemaphore(m_FrameIdx);
    m_FrameIdx      = (m_FrameIdx + 1) % k_FramesInFlight;
    m_FrameAcquired = true;
    return table;
}

void GpuInterop::ReleaseFrame()
{
    if (!m_FrameAcquired) return;
    m_GL->SignalGLDone();
    m_FrameAcquired = false;
}

std::shared_ptr<const SharedImageTable> GpuInterop::SyncSharedImages()
{
    const uint32_t generation = m_Vulkan->GetSharedImageGeneration();
    if (generation == m_SharedImageGeneration) return nullptr;

    // Vulkan already queued the old images for deletion, so the GL textures over that
    // memory go regardless of whether replacements arrive.
    for (uint32_t tex : m_ImportedImages)
        m_GL->ReleaseSharedImage(tex);
    m_ImportedImages.clear();

    auto table = std::make_shared<SharedImageTable>();
    for (const auto& img : m_Vulkan->CollectSharedImages())
    {
        const uint32_t tex = m_GL->ImportSharedImage(img.desc);
        if (tex)
        {
            m_ImportedImages.push_back(tex);
            HMN_CORE_INFO("GpuInterop: imported shared image '{0}' -> GL tex {1} ({2}x{3})",
                          img.name, tex, img.desc.width, img.desc.height);
        }
        else HMN_CORE_WARN("GpuInterop: failed to import shared image '{0}'", img.name);

        table->Set(img.name, tex);
        if (img.desc.memHandle) CloseHandle(img.desc.memHandle);
    }
    table->Finalize();

    m_SharedImageGeneration = generation;
    return table;
}

Scope<InteropRoundTrip> GpuInterop::CreateRoundTrip()
{
    if (!m_GL) return nullptr;
    return CreateScope<InteropRoundTrip>(GetVulkan(), *m_GL);
}

}
