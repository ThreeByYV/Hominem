#include "hmnpch.h"
#include "Renderer.h"
#include "Platform/Vulkan/VulkanSceneRenderer.h"

namespace Hominem {

Renderer::Renderer()  = default;
Renderer::~Renderer() = default;

void Renderer::Init(uint32_t w, uint32_t h)
{
    auto glLUID = SharedResources::GetDeviceLUID();
    auto glName = SharedResources::GetDeviceName();

    m_VulkanRenderer = std::make_unique<VulkanSceneRenderer>();
    m_VulkanRenderer->Init(w, h, glLUID, glName);

    m_SceneRenderer.Init();
    m_SceneRenderer.GetRenderGraph().Resize(w, h);

    SetupInterop(w, h, glLUID);
}

void Renderer::SetupInterop(uint32_t w, uint32_t h, const std::array<uint8_t, 8>& glLUID)
{
    if (!SharedResources::IsInteropSupported())
    {
        HMN_CORE_WARN("Renderer: driver lacks GL/VK external-memory extensions, shared texture unavailable");
        return;
    }

    auto vkLUID = m_VulkanRenderer->GetDeviceLUID();

    if (glLUID == std::array<uint8_t, 8>{})
    {
        HMN_CORE_WARN("Renderer: GL LUID unavailable (GL_EXT_memory_object not exposed), skipping LUID check");
    }
    else if (vkLUID != glLUID)
    {
        HMN_CORE_WARN("Renderer: VK and GL are on different GPUs, shared texture unavailable");
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

void Renderer::Shutdown()
{
    m_SceneRenderer.SetSharedVulkanTexture(0);
    if (m_SharedResources)
    {
        m_SharedResources->Destroy();
        m_SharedResources.reset();
    }
    m_VulkanRenderer->Shutdown();
    m_VulkanRenderer.reset();
    m_SceneRenderer.Shutdown();
}

void Renderer::ExecuteFrame(RecordedFrame& frame)
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

void Renderer::RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h)
{
    m_VulkanRenderer->RegisterRenderTarget(handle, w, h);
}

void Renderer::RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity)
{
    m_VulkanRenderer->RegisterStorageBuffer(handle, capacity);
}

}
