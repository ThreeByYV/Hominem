#include "hmnpch.h"
#include "VulkanRaytracer.h"
#include "VulkanImage.h"

namespace Hominem {

void VulkanRaytracer::Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID, std::string preferredName)
{
    m_Renderer = std::make_unique<VulkanRenderer>();
    m_Renderer->Init(w, h, preferredLUID, std::move(preferredName));
}

void VulkanRaytracer::Shutdown()
{
    auto  device        = m_Renderer->GetDevice();
    auto  allocator     = m_Renderer->GetAllocator();
    auto& deletionQueue = m_Renderer->GetMainDeletionQueue();

    // Deferred, not called directly: VulkanRenderer::Shutdown() only flushes this
    // queue after its own vkDeviceWaitIdle, so these are guaranteed safe to destroy
    // by the time they actually run — never call Destroy() on these directly here.
    for (auto& [_, shader] : m_Shaders)
        deletionQueue.push_function([&shader]() { shader.Destroy(); });
    for (auto& slot : m_RenderTargets)
        if (slot.valid)
            deletionQueue.push_function([&slot, device, allocator]() { slot.rt.Destroy(device, allocator); });
    for (auto& slot : m_Buffers)
        if (slot.valid)
            deletionQueue.push_function([&slot, allocator]() { slot.buf.Destroy(allocator); });

    m_Renderer->Shutdown();
    m_Renderer.reset();
}

void VulkanRaytracer::RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h)
{
    auto device    = m_Renderer->GetDevice();
    auto allocator = m_Renderer->GetAllocator();

    if (handle >= m_RenderTargets.size())
        m_RenderTargets.resize(handle + 1);

    auto& slot = m_RenderTargets[handle];
    if (slot.valid)
        slot.rt.Destroy(device, allocator);

    slot.rt              = VulkanRenderTarget::Create(device, allocator, w, h, VK_FORMAT_R16G16B16A16_SFLOAT);
    slot.needsTransition = true;
    slot.valid           = true;
}

void VulkanRaytracer::RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity)
{
    auto allocator = m_Renderer->GetAllocator();

    if (handle >= m_Buffers.size())
        m_Buffers.resize(handle + 1);

    auto& slot = m_Buffers[handle];
    if (slot.valid)
        slot.buf.Destroy(allocator);

    slot.buf   = VulkanStorageBuffer::Create(allocator, static_cast<VkDeviceSize>(capacity));
    slot.valid = true;
}

void VulkanRaytracer::RunPasses(const std::vector<VulkanComputePass>& passes)
{
    VkCommandBuffer cmd = m_Renderer->BeginFrame();
    m_Renderer->PrepareComputeOnDrawImage();

    auto           device   = m_Renderer->GetDevice();
    const uint32_t frameIdx = m_Renderer->GetCurrentFrameIndex();
    auto [w, h]             = m_Renderer->GetDrawExtent();

    for (auto& pass : passes)
    {
        auto sit = m_Shaders.find(pass.debugName);
        if (sit == m_Shaders.end())
        {
            std::vector<ComputeBindingSpec> specs = {
                { .binding = 0, .type = ComputeBindingSpec::Type::StorageImage }
            };
            for (auto& buf : pass.storageBuffers)
                specs.push_back({ .binding = buf.binding, .type = ComputeBindingSpec::Type::StorageBuffer });
            for (const auto&[binding, handle] : pass.storageImages)
                specs.push_back({ .binding = binding, .type = ComputeBindingSpec::Type::StorageImage });

            sit = m_Shaders.emplace(
                pass.debugName,
                VulkanComputeShader::Create(device, pass.shaderSource, pass.debugName, specs)
            ).first;
        }
        auto& shader = sit->second;

        for (auto& bufData : pass.storageBuffers)
        {
            HMN_CORE_ASSERT(bufData.handle < m_Buffers.size() && m_Buffers[bufData.handle].valid,
                "VulkanHandle {} used in pass '{}' was never registered", bufData.handle, pass.debugName);

            auto& slot = m_Buffers[bufData.handle];
            slot.buf.Upload(bufData.data.data(), static_cast<VkDeviceSize>(bufData.data.size()));
            shader.WriteStorageBuffer(frameIdx, bufData.binding, slot.buf.GetBuffer(), slot.buf.GetCapacity());
        }

        for (auto& img : pass.storageImages)
        {
            HMN_CORE_ASSERT(img.handle < m_RenderTargets.size() && m_RenderTargets[img.handle].valid,
                "VulkanHandle {} used in pass '{}' was never registered", img.handle, pass.debugName);

            auto& slot = m_RenderTargets[img.handle];
            if (slot.needsTransition)
            {
                VulkanImage::TransitionUndefinedToGeneral(cmd, slot.rt.GetImage());
                slot.needsTransition = false;
            }
            shader.WriteStorageImage(frameIdx, img.binding, slot.rt.GetImageView());
        }

        shader.WriteStorageImage(frameIdx, 0, m_Renderer->GetDrawImageView());
        shader.Dispatch(cmd, frameIdx, (w + 15) / 16, (h + 15) / 16);
    }

    m_Renderer->EndFrame();
}

}
