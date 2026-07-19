#include "hmnpch.h"
#include "VulkanRaytracer.h"
#include "VulkanImage.h"

#include <glm/glm.hpp>

namespace Hominem {

namespace {

struct GPUSceneData
{
    glm::mat4 viewProj;
    glm::vec4 cameraPos;
    glm::vec4 lightPos[kVulkanSceneLightCount];
    glm::vec4 lightColor[kVulkanSceneLightCount];
    glm::vec4 ambient;
};

constexpr const char* k_SceneShaderName = "vk_mesh";

struct MeshPushConstants
{
    glm::mat4       model;
    glm::vec4       baseColor;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress sceneBuffer;
    float           unlit;
};

}

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
    for (auto& [_, pipeline] : m_ComputePipelines)
        deletionQueue.push_function([&pipeline]() { pipeline.Destroy(); });
    for (auto& slot : m_RenderTargets)
        if (slot.valid)
            deletionQueue.push_function([&slot, device, allocator]() { slot.rt.Destroy(device, allocator); });
    for (auto& slot : m_Buffers)
        if (slot.valid)
            deletionQueue.push_function([&slot, allocator]() { slot.buf.Destroy(allocator); });
    for (auto& [_, mesh] : m_Meshes)
        deletionQueue.push_function([&mesh, allocator]() { mesh.Destroy(allocator); });
    for (auto& [_, pipeline] : m_GraphicsPipelines)
        deletionQueue.push_function([&pipeline]() { pipeline.Destroy(); });
    if (m_SceneBuffersCreated)
        for (auto& buf : m_SceneBuffers)
            deletionQueue.push_function([&buf, allocator]() { buf.Destroy(allocator); });

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

void VulkanRaytracer::RunFrame(const std::vector<VulkanMeshUpload>& uploads,
                               const std::vector<VulkanComputePass>& computePasses,
                               const std::vector<VulkanMeshDraw>& draws,
                               const VulkanSceneView& view)
{
    VkCommandBuffer cmd = m_Renderer->BeginFrame();

    UploadMeshes(cmd, uploads);

    if (!computePasses.empty())
        RunComputePasses(cmd, computePasses);

    if (!draws.empty())
        RunScenePass(cmd, draws, view);

    m_Renderer->EndFrame();
}

void VulkanRaytracer::UploadMeshes(VkCommandBuffer cmd, const std::vector<VulkanMeshUpload>& uploads)
{
    auto  device     = m_Renderer->GetDevice();
    auto  allocator  = m_Renderer->GetAllocator();
    auto& frameQueue = m_Renderer->GetFrameDeletionQueue();

    for (const auto& upload : uploads)
    {
        if (auto it = m_Meshes.find(upload.handle); it != m_Meshes.end())
        {
            frameQueue.push_function([old = it->second, allocator]() mutable { old.Destroy(allocator); });
            m_Meshes.erase(it);
        }
        m_Meshes.emplace(upload.handle,
            VulkanMeshBuffer::Create(device, allocator, cmd, frameQueue,
                                     upload.vertexData, upload.indices));
    }
}

void VulkanRaytracer::RunComputePasses(VkCommandBuffer cmd, const std::vector<VulkanComputePass>& passes)
{
    m_Renderer->PrepareComputeOnDrawImage();

    auto           device   = m_Renderer->GetDevice();
    const uint32_t frameIdx = m_Renderer->GetCurrentFrameIndex();
    auto [w, h]             = m_Renderer->GetDrawExtent();

    for (auto& pass : passes)
    {
        auto sit = m_ComputePipelines.find(pass.debugName);
        if (sit == m_ComputePipelines.end())
        {
            std::vector<ComputeBindingSpec> specs = {
                { .binding = 0, .type = ComputeBindingSpec::Type::StorageImage }
            };
            for (auto& buf : pass.storageBuffers)
                specs.push_back({ .binding = buf.binding, .type = ComputeBindingSpec::Type::StorageBuffer });
            for (const auto&[binding, handle] : pass.storageImages)
                specs.push_back({ .binding = binding, .type = ComputeBindingSpec::Type::StorageImage });

            const VulkanShader& computeShader = m_ShaderLibrary.LoadCompute(pass.debugName, pass.shaderSource);
            sit = m_ComputePipelines.emplace(
                pass.debugName,
                VulkanComputePipeline::Create(device, computeShader.computeSpirv, specs)
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
}

VulkanGraphicsPipeline& VulkanRaytracer::GetOrCreateScenePipeline()
{
    auto it = m_GraphicsPipelines.find(k_SceneShaderName);
    if (it != m_GraphicsPipelines.end())
        return it->second;

    const VulkanShader& shader = m_ShaderLibrary.Load(k_SceneShaderName);

    const GraphicsPipelineSpec spec
    {
        .vertexSpirv       = shader.vertexSpirv,
        .fragmentSpirv     = shader.fragmentSpirv,
        .colorFormats      = { m_Renderer->GetDrawImageFormat() },
        .depthFormat       = m_Renderer->GetDepthImageFormat(),
        .cullMode          = VK_CULL_MODE_NONE,
        .pushConstantBytes = sizeof(MeshPushConstants),
    };

    return m_GraphicsPipelines.emplace(k_SceneShaderName,
        VulkanGraphicsPipeline::Create(m_Renderer->GetDevice(), spec)).first->second;
}

void VulkanRaytracer::RunScenePass(VkCommandBuffer cmd, const std::vector<VulkanMeshDraw>& draws,
                                   const VulkanSceneView& view)
{
    auto           device   = m_Renderer->GetDevice();
    auto           allocator = m_Renderer->GetAllocator();
    const uint32_t frameIdx = m_Renderer->GetCurrentFrameIndex();
    auto [w, h]             = m_Renderer->GetDrawExtent();

    const bool preserveContents = m_Renderer->DrawImageHasComputeOutput();
    m_Renderer->PrepareGraphicsOnDrawImage();

    if (!m_SceneBuffersCreated)
    {
        for (auto& buf : m_SceneBuffers)
            buf = VulkanStorageBuffer::Create(allocator, sizeof(GPUSceneData),
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_SceneBuffersCreated = true;
    }

    GPUSceneData scene {};
    scene.viewProj  = view.proj * view.view;
    scene.cameraPos = glm::vec4(view.cameraPos, 1.f);
    for (uint32_t i = 0; i < kVulkanSceneLightCount; i++)
    {
        const auto& light = view.lights[i];
        scene.lightPos[i]   = glm::vec4(light.Position, light.Radius);
        scene.lightColor[i] = glm::vec4(light.Color, light.Intensity);
    }
    scene.ambient = glm::vec4(view.ambient, 1.f);
    m_SceneBuffers[frameIdx].Upload(&scene, sizeof(scene));

    auto& pipeline = GetOrCreateScenePipeline();

    const VkRenderingAttachmentInfo colorAttachment {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = m_Renderer->GetDrawImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = preserveContents ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = { { 0.f, 0.f, 0.f, 1.f } } },
    };
    const VkRenderingAttachmentInfo depthAttachment {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = m_Renderer->GetDepthImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue  = { .depthStencil = { 1.f, 0 } },
    };
    const VkRenderingInfo renderingInfo {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { .offset = { 0, 0 }, .extent = { w, h } },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
        .pDepthAttachment     = &depthAttachment,
    };
    vkCmdBeginRendering(cmd, &renderingInfo);

    pipeline.Bind(cmd);

    const VkViewport viewport {
        .x        = 0.f,
        .y        = 0.f,
        .width    = (float)w,
        .height   = (float)h,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    const VkRect2D scissor { .offset = { 0, 0 }, .extent = { w, h } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const VkDeviceAddress sceneAddress = m_SceneBuffers[frameIdx].GetDeviceAddress(device);

    for (const auto& draw : draws)
    {
        auto it = m_Meshes.find(draw.mesh);
        if (it == m_Meshes.end()) continue;
        auto& mesh = it->second;

        const MeshPushConstants pc {
            .model        = draw.transform,
            .baseColor    = draw.baseColor,
            .vertexBuffer = mesh.GetVertexBufferAddress(),
            .sceneBuffer  = sceneAddress,
            .unlit        = draw.unlit ? 1.f : 0.f,
        };
        pipeline.PushConstants(cmd, &pc, sizeof(pc));

        vkCmdBindIndexBuffer(cmd, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.GetIndexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);
}

}
