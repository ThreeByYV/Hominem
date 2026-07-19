#include "hmnpch.h"
#include "Renderer.h"
#include "Platform/Vulkan/VulkanRaytracer.h"

namespace Hominem {

Renderer::Renderer()  = default;
Renderer::~Renderer() = default;

void Renderer::Init(uint32_t w, uint32_t h)
{
    auto glLUID = SharedResources::GetDeviceLUID();
    auto glName = SharedResources::GetDeviceName();

    m_VulkanRaytracer = std::make_unique<VulkanRaytracer>();
    m_VulkanRaytracer->Init(w, h, glLUID, glName);

    m_SceneRenderer.Init();
    m_SceneRenderer.GetRenderGraph().Resize(w, h);

    SetupInterop(w, h, glLUID);
}

void Renderer::SetupInterop(uint32_t w, uint32_t h, const std::array<uint8_t, 8>& glLUID)
{
    auto vkLUID = m_VulkanRaytracer->GetDeviceLUID();

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

    HANDLE memHandle = m_VulkanRaytracer->GetDrawImageWin32Handle();
    m_SharedResources->ImportSharedTexture(memHandle, m_VulkanRaytracer->GetDrawImageMemorySize(), w, h);

    // one semaphore per frame-in-flight, prevents N+1 overwriting before GPU signals N
    for (uint32_t i = 0; i < 2; ++i)
    {
        HANDLE semHandle = m_VulkanRaytracer->GetComputeDoneSemaphoreWin32Handle(i);
        m_SharedResources->ImportSemaphore(i, semHandle);
    }

    m_SharedResources->ImportGLDoneSemaphore(m_VulkanRaytracer->GetGLDoneSemaphoreWin32Handle());

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
    m_VulkanRaytracer->Shutdown();
    m_VulkanRaytracer.reset();
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
        m_VulkanRaytracer->RunFrame(frame.vulkanMeshUploads, frame.vulkanPasses,
                                    frame.vulkanMeshDraws, frame.vulkanDebugSpheres, frame.vulkanView);
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
    m_VulkanRaytracer->RegisterRenderTarget(handle, w, h);
}

void Renderer::RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity)
{
    m_VulkanRaytracer->RegisterStorageBuffer(handle, capacity);
}

}
