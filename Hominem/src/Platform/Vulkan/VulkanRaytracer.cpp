#include "hmnpch.h"
#include "VulkanRaytracer.h"

#include <unordered_map>
#include <vector>

#include "VulkanRenderer.h"
#include "VulkanComputeShader.h"
#include "VulkanStorageBuffer.h"
#include "VulkanRenderTarget.h"
#include "VulkanImage.h"

namespace Hominem {

struct VulkanRaytracer::Impl
{
    std::unique_ptr<VulkanRenderer>                      renderer;
    std::unordered_map<std::string, VulkanComputeShader> shaders;

    struct RTSlot { VulkanRenderTarget rt; bool needsTransition = true; bool valid = false; };
    struct BufSlot { VulkanStorageBuffer buf; bool valid = false; };

    std::vector<RTSlot>  renderTargets;
    std::vector<BufSlot> buffers;
};

VulkanRaytracer::VulkanRaytracer()
{
    m_Impl = std::make_unique<Impl>();
}

VulkanRaytracer::~VulkanRaytracer() = default;

void VulkanRaytracer::Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID, std::string preferredName)
{
    m_Impl->renderer = std::make_unique<VulkanRenderer>();
    m_Impl->renderer->Init(w, h, preferredLUID, std::move(preferredName));
}

void VulkanRaytracer::Shutdown()
{
    auto device    = m_Impl->renderer->GetDevice();
    auto allocator = m_Impl->renderer->GetAllocator();

    for (auto& [_, shader] : m_Impl->shaders)
        shader.Destroy();
    for (auto& slot : m_Impl->renderTargets)
        if (slot.valid) slot.rt.Destroy(device, allocator);
    for (auto& slot : m_Impl->buffers)
        if (slot.valid) slot.buf.Destroy(allocator);

    m_Impl->renderer->Shutdown();
    m_Impl->renderer.reset();
}

void VulkanRaytracer::RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h)
{
    auto device    = m_Impl->renderer->GetDevice();
    auto allocator = m_Impl->renderer->GetAllocator();

    if (handle >= m_Impl->renderTargets.size())
        m_Impl->renderTargets.resize(handle + 1);

    auto& slot = m_Impl->renderTargets[handle];
    if (slot.valid)
        slot.rt.Destroy(device, allocator);

    slot.rt              = VulkanRenderTarget::Create(device, allocator, w, h,
                                                      VK_FORMAT_R16G16B16A16_SFLOAT);
    slot.needsTransition = true;
    slot.valid           = true;
}

void VulkanRaytracer::RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity)
{
    auto allocator = m_Impl->renderer->GetAllocator();

    if (handle >= m_Impl->buffers.size())
        m_Impl->buffers.resize(handle + 1);

    auto& slot = m_Impl->buffers[handle];
    if (slot.valid)
        slot.buf.Destroy(allocator);

    slot.buf   = VulkanStorageBuffer::Create(allocator, static_cast<VkDeviceSize>(capacity));
    slot.valid = true;
}

void VulkanRaytracer::RunPasses(const std::vector<VulkanComputePass>& passes)
{
    VkCommandBuffer cmd = m_Impl->renderer->BeginFrame();

    m_Impl->renderer->PrepareComputeOnDrawImage();

    auto           device   = m_Impl->renderer->GetDevice();
    const uint32_t frameIdx = m_Impl->renderer->GetCurrentFrameIndex();
    auto [w, h]             = m_Impl->renderer->GetDrawExtent();

    for (auto& pass : passes)
    {
        auto sit = m_Impl->shaders.find(pass.debugName);
        if (sit == m_Impl->shaders.end())
        {
            std::vector<ComputeBindingSpec> specs = {
                { .binding = 0, .type = ComputeBindingSpec::Type::StorageImage }
            };
            for (auto& buf : pass.storageBuffers)
                specs.push_back({ .binding = buf.binding,
                                  .type    = ComputeBindingSpec::Type::StorageBuffer });
            for (auto& img : pass.storageImages)
                specs.push_back({ .binding = img.binding,
                                  .type    = ComputeBindingSpec::Type::StorageImage });

            sit = m_Impl->shaders.emplace(
                pass.debugName,
                VulkanComputeShader::Create(device, pass.shaderSource, pass.debugName, specs)
            ).first;
        }
        auto& shader = sit->second;

        for (auto& bufData : pass.storageBuffers)
        {
            HMN_CORE_ASSERT(bufData.handle < m_Impl->buffers.size() &&
                            m_Impl->buffers[bufData.handle].valid,
                "VulkanHandle {} used in pass '{}' was never registered",
                bufData.handle, pass.debugName);

            auto& slot = m_Impl->buffers[bufData.handle];
            slot.buf.Upload(bufData.data.data(),
                            static_cast<VkDeviceSize>(bufData.data.size()));
            shader.WriteStorageBuffer(frameIdx, bufData.binding,
                                      slot.buf.GetBuffer(), slot.buf.GetCapacity());
        }

        for (auto& img : pass.storageImages)
        {
            HMN_CORE_ASSERT(img.handle < m_Impl->renderTargets.size() &&
                            m_Impl->renderTargets[img.handle].valid,
                "VulkanHandle {} used in pass '{}' was never registered",
                img.handle, pass.debugName);

            auto& slot = m_Impl->renderTargets[img.handle];
            if (slot.needsTransition)
            {
                VulkanImage::TransitionUndefinedToGeneral(cmd, slot.rt.GetImage());
                slot.needsTransition = false;
            }
            shader.WriteStorageImage(frameIdx, img.binding, slot.rt.GetImageView());
        }

        shader.WriteStorageImage(frameIdx, 0, m_Impl->renderer->GetDrawImageView());
        shader.Dispatch(cmd, frameIdx, (w + 15) / 16, (h + 15) / 16);
    }

    m_Impl->renderer->EndFrame();
}

HANDLE VulkanRaytracer::GetDrawImageWin32Handle()
{
    return m_Impl->renderer->GetDrawImageWin32Handle();
}

HANDLE VulkanRaytracer::GetComputeDoneSemaphoreWin32Handle(uint32_t frameIdx)
{
    return m_Impl->renderer->GetComputeDoneSemaphoreWin32Handle(frameIdx);
}

VkDeviceSize VulkanRaytracer::GetDrawImageMemorySize() const
{
    return m_Impl->renderer->GetDrawImageMemorySize();
}

VkExtent2D VulkanRaytracer::GetDrawExtent() const
{
    return m_Impl->renderer->GetDrawExtent();
}

uint32_t VulkanRaytracer::GetCurrentFrameIndex() const
{
    return m_Impl->renderer->GetCurrentFrameIndex();
}

std::array<uint8_t, 8> VulkanRaytracer::GetDeviceLUID() const
{
    return m_Impl->renderer->GetDeviceLUID();
}

}
