#include "hmnpch.h"
#include "Hominem/Renderer/RenderSystem.h"
#include "Platform/Vulkan/VulkanSceneRenderer.h"
#include "Platform/Interop/GpuInterop.h"

namespace Hominem {

RenderSystem::RenderSystem()  = default;
RenderSystem::~RenderSystem() = default;

void RenderSystem::Init(uint32_t w, uint32_t h)
{
    m_VulkanRenderer = std::make_unique<VulkanSceneRenderer>();
    m_VulkanRenderer->Init(w, h, GpuInterop::GLDeviceLUID(), GpuInterop::GLDeviceName());

    // Interop first: SceneRenderer::Init creates the upscaler, which may run through it.
    m_Interop = std::make_unique<GpuInterop>();
    m_Interop->Init(*m_VulkanRenderer, w, h);
    m_SceneRenderer.SetSharedVulkanTexture(m_Interop->GetDrawTexture());

    m_SceneRenderer.Init({ m_Interop->IsActive() ? m_Interop.get() : nullptr });
    m_SceneRenderer.GetRenderGraph().Resize(w, h);
}

void RenderSystem::Shutdown()
{
    // First: the upscaler holds Vulkan images and GL imports of them.
    m_SceneRenderer.Shutdown();
    m_SceneRenderer.SetSharedVulkanTexture(0);
    m_Interop->Shutdown();
    m_Interop.reset();
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

    if (hasVulkanWork)
    {
        m_VulkanRenderer->RunFrame(frame.vulkanMeshUploads, frame.vulkanPasses,
                                    frame.vulkanMeshDraws, frame.vulkanDebugSpheres,
                                    frame.vulkanDDGI, frame.vulkanView);
        if (auto table = m_Interop->AcquireFrame())
            m_SceneRenderer.SetSharedImages(std::move(table));
    }

    for (auto& cmd : frame.passCmds)
        cmd.Submit();

    m_Interop->ReleaseFrame();
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
