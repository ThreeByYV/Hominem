#include "hmnpch.h"
#include "Hominem/Renderer/RenderSystem.h"
#include "Platform/Vulkan/VulkanSceneRenderer.h"

namespace Hominem {

RenderSystem::RenderSystem()  = default;
RenderSystem::~RenderSystem() = default;

void RenderSystem::Init(uint32_t w, uint32_t h)
{
    auto glLUID = SharedResources::GetDeviceLUID();
    auto glName = SharedResources::GetDeviceName();

    m_VulkanRenderer = std::make_unique<VulkanSceneRenderer>();
    m_VulkanRenderer->Init(w, h, glLUID, glName);

    // Interop first: SceneRenderer::Init creates the upscaler, which may run through it.
    SetupInterop(w, h, glLUID);

    m_SceneRenderer.Init({ m_VulkanRenderer.get(), m_SharedResources.get() });
    m_SceneRenderer.GetRenderGraph().Resize(w, h);
}

void RenderSystem::SetupInterop(uint32_t w, uint32_t h, const std::array<uint8_t, 8>& glLUID)
{
    if (!SharedResources::IsInteropSupported())
    {
        HMN_CORE_WARN("RenderSystem: driver lacks GL/VK external-memory extensions, shared texture unavailable");
        return;
    }

    auto vkLUID = m_VulkanRenderer->GetDeviceLUID();

    if (glLUID == std::array<uint8_t, 8>{})
    {
        HMN_CORE_WARN("RenderSystem: GL LUID unavailable (GL_EXT_memory_object not exposed), skipping LUID check");
    }
    else if (vkLUID != glLUID)
    {
        HMN_CORE_WARN("RenderSystem: VK and GL are on different GPUs, shared texture unavailable");
        return;
    }

    m_SharedResources = SharedResources::Create();

    HANDLE memHandle = m_VulkanRenderer->GetDrawImageWin32Handle();
    m_SharedResources->ImportSharedTexture(memHandle, m_VulkanRenderer->GetDrawImageMemorySize(), w, h);

    // one semaphore per frame-in-flight, prevents N+1 overwriting before GPU signals N
    for (uint32_t i = 0; i < 2; ++i)
    {
        HANDLE semHandle = m_VulkanRenderer->GetComputeDoneSemaphoreWin32Handle(i);
        m_SharedResources->ImportSemaphore(i, semHandle);
    }

    m_SharedResources->ImportGLDoneSemaphore(m_VulkanRenderer->GetGLDoneSemaphoreWin32Handle());

    m_SceneRenderer.SetSharedVulkanTexture(m_SharedResources->GetTextureID());
}

void RenderSystem::Shutdown()
{
    // First: the upscaler holds Vulkan images and GL imports of them.
    m_SceneRenderer.Shutdown();
    m_SceneRenderer.SetSharedVulkanTexture(0);
    if (m_SharedResources)
    {
        m_SharedResources->Destroy();
        m_SharedResources.reset();
    }
    m_VulkanRenderer->Shutdown();
    m_VulkanRenderer.reset();
}

void RenderSystem::ExecuteFrame(RecordedFrame& frame)
{
    auto& graph = m_SceneRenderer.GetRenderGraph();
    graph.SetRenderScale(frame.renderScale);
    graph.Resize(frame.viewportWidth, frame.viewportHeight);

    const bool hasVulkanWork = !frame.vulkanPasses.empty()
                            || !frame.vulkanMeshDraws.empty()
                            || !frame.vulkanMeshUploads.empty()
                            || !frame.vulkanDebugSpheres.empty();

    bool sharedTextureInUse = false;

    if (hasVulkanWork)
    {
        m_VulkanRenderer->RunFrame(frame.vulkanMeshUploads, frame.vulkanPasses,
                                    frame.vulkanMeshDraws, frame.vulkanDebugSpheres,
                                    frame.vulkanDDGI, frame.vulkanView);
        if (m_SharedResources)
        {
            // Import before the wait: the semaphore acquires every shared texture at once,
            // and what the GL passes are about to sample must be in that set.
            SyncSharedImages();
            sharedTextureInUse = true;
            m_SharedResources->WaitSemaphore(m_VkFrameIdx);
            m_VkFrameIdx = (m_VkFrameIdx + 1) % 2;
        }
    }

    for (auto& cmd : frame.passCmds)
        cmd.Submit();

    if (sharedTextureInUse)
        m_SharedResources->SignalGLDone();
}

void RenderSystem::SyncSharedImages()
{
    const uint32_t generation = m_VulkanRenderer->GetSharedImageGeneration();
    if (generation == m_SharedImageGeneration) return;

    // Vulkan already queued the old images for deletion, so the GL textures over that
    // memory go regardless of whether replacements arrive.
    for (uint32_t tex : m_ImportedImages)
        m_SharedResources->ReleaseSharedImage(tex);
    m_ImportedImages.clear();

    auto table = std::make_shared<SharedImageTable>();

    for (const auto& img : m_VulkanRenderer->CollectSharedImages())
    {
        const uint32_t tex = m_SharedResources->ImportSharedImage(img.desc);
        if (tex)
        {
            m_ImportedImages.push_back(tex);
            HMN_CORE_INFO("RenderSystem: imported shared image '{0}' -> GL tex {1} ({2}x{3})",
                          img.name, tex, img.desc.width, img.desc.height);
        }
        else HMN_CORE_WARN("RenderSystem: failed to import shared image '{0}'", img.name);

        table->Set(img.name, tex);
        if (img.desc.memHandle) CloseHandle(img.desc.memHandle);
    }
    table->Finalize();

    m_SharedImageGeneration = generation;
    m_SceneRenderer.SetSharedImages(std::move(table));
}

void RenderSystem::RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h)
{
    m_VulkanRenderer->RegisterRenderTarget(handle, w, h);
}

void RenderSystem::RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity)
{
    m_VulkanRenderer->RegisterStorageBuffer(handle, capacity);
}

}
