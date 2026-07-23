#include "hmnpch.h"
#include "VulkanSceneRenderer.h"
#include "VulkanImage.h"
#include "Hominem/Renderer/UvSphere.h"

#include <glm/glm.hpp>

namespace Hominem {

namespace {

struct GPUSceneData
{
    glm::mat4  viewProj;
    glm::vec4  cameraPos;
    glm::ivec4 lightCount;   // x = active lights this frame (<= kVulkanMaxSceneLights)
    glm::vec4  lightPos[kVulkanMaxSceneLights];
    glm::vec4  lightColor[kVulkanMaxSceneLights];
    glm::vec4  ambient;
    glm::vec4  ddgiOrigin;
    glm::vec4  ddgiSpacing;
    glm::ivec4 ddgiCounts;   // xyz = counts, w = numRays (0 = DDGI off)
    glm::vec4  ddgiRotation;
    glm::ivec4 ddgiTiles;    // x = irradiance interior, y = distance interior
};

constexpr const char* k_SceneShaderName       = "vk_mesh";
constexpr const char* k_DebugSphereShaderName = "vk_debug_sphere";

struct MeshPushConstants
{
    glm::mat4       model;
    glm::vec4       baseColor;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress sceneBuffer;
    float           unlit;
};

struct DebugSpherePushConstants
{
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress instanceBuffer;
    VkDeviceAddress sceneBuffer;
};

}

void VulkanSceneRenderer::Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID, std::string preferredName)
{
    m_Renderer = std::make_unique<VulkanRenderer>();
    m_Renderer->Init(w, h, preferredLUID, std::move(preferredName));
}

void VulkanSceneRenderer::Shutdown()
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
            deletionQueue.push_function([&slot, device, allocator]() { slot.renderTarget.Destroy(device, allocator); });
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
    if (m_DebugSphereMeshReady)
        deletionQueue.push_function([this, allocator]() { m_DebugSphereMesh.Destroy(allocator); });
    if (m_DebugSphereBuffersCreated)
        for (auto& buf : m_DebugSphereBuffers)
            deletionQueue.push_function([&buf, allocator]() { buf.Destroy(allocator); });
    if (m_MeshDescReady)
    {
        deletionQueue.push_function([this, device]() { vkDestroyDescriptorPool(device, m_MeshDescPool, nullptr); });
        deletionQueue.push_function([this, device]() { vkDestroyDescriptorSetLayout(device, m_MeshSetLayout, nullptr); });
    }
    if (m_DummyAtlasCreated)
        deletionQueue.push_function([this, device, allocator]() { m_DummyAtlas.Destroy(device, allocator); });

    m_DDGI.Shutdown(deletionQueue, device, allocator);
    m_Raytracer.Shutdown(deletionQueue, device, allocator);

    m_Renderer->Shutdown();
    m_Renderer.reset();
}

void VulkanSceneRenderer::RegisterRenderTarget(VulkanHandle handle, uint32_t w, uint32_t h)
{
    auto device    = m_Renderer->GetDevice();
    auto allocator = m_Renderer->GetAllocator();

    if (handle >= m_RenderTargets.size())
        m_RenderTargets.resize(handle + 1);

    auto& slot = m_RenderTargets[handle];
    if (slot.valid)
        slot.renderTarget.Destroy(device, allocator);

    slot.renderTarget    = VulkanRenderTarget::Create(device, allocator, w, h, VK_FORMAT_R16G16B16A16_SFLOAT);
    slot.needsTransition = true;
    slot.valid           = true;
}

void VulkanSceneRenderer::RegisterStorageBuffer(VulkanHandle handle, uint32_t capacity)
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

void VulkanSceneRenderer::RunFrame(const std::vector<VulkanMeshUpload>& uploads,
                                   const std::vector<VulkanComputePass>& computePasses,
                                   const std::vector<VulkanMeshDraw>& draws,
                                   const std::vector<VulkanSphereInstance>& debugSpheres,
                                   const VulkanDDGIParams& ddgi,
                                   const VulkanSceneView& view)
{
    VkCommandBuffer cmd = m_Renderer->BeginFrame();

    UploadMeshes(cmd, uploads);

    if (!computePasses.empty())
        RunComputePasses(cmd, computePasses);

    if (!draws.empty() || !debugSpheres.empty())
        RunScenePass(cmd, draws, debugSpheres, ddgi, view);

    m_Renderer->EndFrame();
}

void VulkanSceneRenderer::UploadMeshes(VkCommandBuffer cmd, const std::vector<VulkanMeshUpload>& uploads)
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
        auto& mesh = m_Meshes.emplace(upload.handle,
            VulkanMeshBuffer::Create(device, allocator, cmd, frameQueue,
                                     upload.vertexData, upload.indices)).first->second;

        m_Raytracer.OnMeshUploaded(*m_Renderer, cmd, upload.handle, mesh);
    }
}

void VulkanSceneRenderer::RunComputePasses(VkCommandBuffer cmd, const std::vector<VulkanComputePass>& passes)
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
                VulkanImage::TransitionUndefinedToGeneral(cmd, slot.renderTarget.GetImage());
                slot.needsTransition = false;
            }
            shader.WriteStorageImage(frameIdx, img.binding, slot.renderTarget.GetImageView());
        }

        shader.WriteStorageImage(frameIdx, 0, m_Renderer->GetDrawImageView());
        shader.Dispatch(cmd, frameIdx, (w + 15) / 16, (h + 15) / 16);
    }
}

VulkanGraphicsPipeline& VulkanSceneRenderer::GetOrCreateScenePipeline()
{
    auto it = m_GraphicsPipelines.find(k_SceneShaderName);
    if (it != m_GraphicsPipelines.end())
        return it->second;

    const VulkanShader& shader = m_ShaderLibrary.Load(k_SceneShaderName);

    const VkDescriptorSetLayout setLayouts[] = { m_MeshSetLayout };

    const GraphicsPipelineSpec spec
    {
        .vertexSpirv       = shader.vertexSpirv,
        .fragmentSpirv     = shader.fragmentSpirv,
        .colorFormats      = { m_Renderer->GetDrawImageFormat() },
        .depthFormat       = m_Renderer->GetDepthImageFormat(),
        .cullMode          = VK_CULL_MODE_NONE,
        .pushConstantBytes = sizeof(MeshPushConstants),
        .setLayouts        = setLayouts,
    };

    return m_GraphicsPipelines.emplace(k_SceneShaderName,
        VulkanGraphicsPipeline::Create(m_Renderer->GetDevice(), spec)).first->second;
}

VulkanGraphicsPipeline& VulkanSceneRenderer::GetOrCreateDebugSpherePipeline()
{
    auto it = m_GraphicsPipelines.find(k_DebugSphereShaderName);
    if (it != m_GraphicsPipelines.end())
        return it->second;

    const VulkanShader& shader = m_ShaderLibrary.Load(k_DebugSphereShaderName);

    const GraphicsPipelineSpec spec
    {
        .vertexSpirv       = shader.vertexSpirv,
        .fragmentSpirv     = shader.fragmentSpirv,
        .colorFormats      = { m_Renderer->GetDrawImageFormat() },
        .depthFormat       = m_Renderer->GetDepthImageFormat(),
        .cullMode          = VK_CULL_MODE_NONE,
        .pushConstantBytes = sizeof(DebugSpherePushConstants),
    };

    return m_GraphicsPipelines.emplace(k_DebugSphereShaderName,
        VulkanGraphicsPipeline::Create(m_Renderer->GetDevice(), spec)).first->second;
}

void VulkanSceneRenderer::EnsureDebugSphereMesh(VkCommandBuffer cmd)
{
    if (m_DebugSphereMeshReady) return;

    const UvSphereMesh mesh = GenerateUvSphere();
    const auto* bytes = reinterpret_cast<const uint8_t*>(mesh.vertices.data());

    m_DebugSphereMesh = VulkanMeshBuffer::Create(
        m_Renderer->GetDevice(), m_Renderer->GetAllocator(),
        cmd, m_Renderer->GetFrameDeletionQueue(),
        { bytes, bytes + mesh.vertices.size() * sizeof(StaticVertex) },
        mesh.indices);
    m_DebugSphereMeshReady = true;
}

void VulkanSceneRenderer::DrawDebugSpheres(VkCommandBuffer cmd,
                                           const std::vector<VulkanSphereInstance>& spheres,
                                           VkDeviceAddress sceneAddress)
{
    if (spheres.empty()) return;

    auto           device    = m_Renderer->GetDevice();
    auto           allocator = m_Renderer->GetAllocator();
    const uint32_t frameIdx  = m_Renderer->GetCurrentFrameIndex();

    const uint32_t needed = (uint32_t)(spheres.size() * sizeof(VulkanSphereInstance));
    if (!m_DebugSphereBuffersCreated || needed > m_DebugSphereCapacity)
    {
        if (m_DebugSphereBuffersCreated)
            for (auto& buf : m_DebugSphereBuffers)
                m_Renderer->GetFrameDeletionQueue().push_function(
                    [b = buf, allocator]() mutable { b.Destroy(allocator); });

        m_DebugSphereCapacity = std::max(needed, 1u << 16);
        for (auto& buf : m_DebugSphereBuffers)
            buf = VulkanStorageBuffer::Create(allocator, m_DebugSphereCapacity,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        m_DebugSphereBuffersCreated = true;
    }

    m_DebugSphereBuffers[frameIdx].Upload(spheres.data(), needed);

    DrawSphereInstances(cmd, m_DebugSphereBuffers[frameIdx].GetDeviceAddress(device),
                        (uint32_t)spheres.size(), sceneAddress);
}

void VulkanSceneRenderer::DrawSphereInstances(VkCommandBuffer cmd, VkDeviceAddress instanceAddress,
                                              uint32_t count, VkDeviceAddress sceneAddress)
{
    auto& pipeline = GetOrCreateDebugSpherePipeline();
    pipeline.Bind(cmd);

    const DebugSpherePushConstants pc
    {
        .vertexBuffer   = m_DebugSphereMesh.GetVertexBufferAddress(),
        .instanceBuffer = instanceAddress,
        .sceneBuffer    = sceneAddress,
    };
    pipeline.PushConstants(cmd, &pc, sizeof(pc));

    vkCmdBindIndexBuffer(cmd, m_DebugSphereMesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, m_DebugSphereMesh.GetIndexCount(), count, 0, 0, 0);
}

void VulkanSceneRenderer::EnsureMeshDescriptors(VkCommandBuffer cmd)
{
    if (m_MeshDescReady) return;

    auto device    = m_Renderer->GetDevice();
    auto allocator = m_Renderer->GetAllocator();

    const VkDescriptorSetLayoutBinding bindings[]
    {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    const VkDescriptorSetLayoutCreateInfo layoutInfo
    {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings,
    };
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_MeshSetLayout);

    const VkDescriptorPoolSize poolSize
    {
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 2 * (uint32_t)m_MeshDescSets.size(),
    };
    const VkDescriptorPoolCreateInfo poolInfo
    {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = (uint32_t)m_MeshDescSets.size(),
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_MeshDescPool);

    std::array<VkDescriptorSetLayout, 2> layouts { m_MeshSetLayout, m_MeshSetLayout };
    const VkDescriptorSetAllocateInfo allocInfo
    {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = m_MeshDescPool,
        .descriptorSetCount = (uint32_t)m_MeshDescSets.size(),
        .pSetLayouts        = layouts.data(),
    };
    vkAllocateDescriptorSets(device, &allocInfo, m_MeshDescSets.data());

    // 1x1 fallback so non-DDGI draws still have a valid sampler bound (never sampled).
    m_DummyAtlas = VulkanTexture::Create(device, allocator, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
                                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, true);
    VulkanImage::TransitionUndefinedToGeneral(cmd, m_DummyAtlas.GetImage());
    m_DummyAtlasCreated = true;

    m_MeshDescReady = true;
}

void VulkanSceneRenderer::RunScenePass(VkCommandBuffer cmd, const std::vector<VulkanMeshDraw>& draws,
                                       const std::vector<VulkanSphereInstance>& debugSpheres,
                                       const VulkanDDGIParams& ddgi,
                                       const VulkanSceneView& view)
{
    auto           device   = m_Renderer->GetDevice();
    auto           allocator = m_Renderer->GetAllocator();
    const uint32_t frameIdx = m_Renderer->GetCurrentFrameIndex();
    auto [w, h]             = m_Renderer->GetDrawExtent();

    const bool ddgiActive = ddgi.probeNumRays > 0;

    if (!debugSpheres.empty() || (ddgiActive && ddgi.showSurfels))
        EnsureDebugSphereMesh(cmd);

    EnsureMeshDescriptors(cmd);
    m_Raytracer.EnsureScene(*m_Renderer, cmd, draws, m_Meshes);

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
    scene.lightCount = glm::ivec4((int)view.lightCount, 0, 0, 0);
    for (uint32_t i = 0; i < view.lightCount; i++)
    {
        const auto& light = view.lights[i];
        scene.lightPos[i]   = glm::vec4(light.Position, light.Radius);
        scene.lightColor[i] = glm::vec4(light.Color, light.Intensity);
    }
    scene.ambient = glm::vec4(view.ambient, 1.f);
    if (ddgiActive)
    {
        scene.ddgiOrigin   = glm::vec4(ddgi.origin, 0.f);
        scene.ddgiSpacing  = glm::vec4(ddgi.probeSpacing, 0.f);
        scene.ddgiCounts   = glm::ivec4(ddgi.probeCounts, ddgi.probeNumRays);
        scene.ddgiRotation = glm::vec4(ddgi.probeRayRotation.x, ddgi.probeRayRotation.y,
                                       ddgi.probeRayRotation.z, ddgi.probeRayRotation.w);
        scene.ddgiTiles    = glm::ivec4(VulkanRayTracingDDGI::k_IrradianceInterior,
                                        VulkanRayTracingDDGI::k_DistanceInterior, 0, 0);
    }
    m_SceneBuffers[frameIdx].Upload(&scene, sizeof(scene));

    const VkDeviceAddress sceneAddress = m_SceneBuffers[frameIdx].GetDeviceAddress(device);

    // Ray-traced GI (trace + blend + border) must run before the draw image goes into rendering.
    if (ddgiActive && m_Raytracer.HasScene())
    {
        auto ctx = m_Raytracer.MakeContext(*m_Renderer, cmd, sceneAddress, m_ShaderLibrary);
        m_DDGI.Execute(ctx, ddgi);
    }

    const bool preserveContents = m_Renderer->DrawImageHasComputeOutput();
    m_Renderer->PrepareGraphicsOnDrawImage();

    auto& pipeline = GetOrCreateScenePipeline();

    const VkRenderingAttachmentInfo colorAttachment
    {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = m_Renderer->GetDrawImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = preserveContents ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = { { 0.f, 0.f, 0.f, 1.f } } },
    };
    const VkRenderingAttachmentInfo depthAttachment
    {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = m_Renderer->GetDepthImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue  = { .depthStencil = { 1.f, 0 } },
    };
    const VkRenderingInfo renderingInfo
    {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { .offset = { 0, 0 }, .extent = { w, h } },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
        .pDepthAttachment     = &depthAttachment,
    };
    vkCmdBeginRendering(cmd, &renderingInfo);

    pipeline.Bind(cmd);

    const bool sampleAtlas = ddgiActive && m_DDGI.HasAtlases();
    const VulkanRayTracingDDGI::AtlasView irradiance = sampleAtlas ? m_DDGI.GetIrradianceAtlas()
                                                           : VulkanRayTracingDDGI::AtlasView{ m_DummyAtlas.GetImageView(), m_DummyAtlas.GetSampler() };
    const VulkanRayTracingDDGI::AtlasView distance   = sampleAtlas ? m_DDGI.GetDistanceAtlas()
                                                           : VulkanRayTracingDDGI::AtlasView{ m_DummyAtlas.GetImageView(), m_DummyAtlas.GetSampler() };
    const VkDescriptorImageInfo irradianceInfo
    {
        .sampler     = irradiance.sampler,
        .imageView   = irradiance.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkDescriptorImageInfo distanceInfo
    {
        .sampler     = distance.sampler,
        .imageView   = distance.view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkWriteDescriptorSet atlasWrites[]
    {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = m_MeshDescSets[frameIdx],
          .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &irradianceInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = m_MeshDescSets[frameIdx],
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &distanceInfo },
    };
    vkUpdateDescriptorSets(device, 2, atlasWrites, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(),
                            0, 1, &m_MeshDescSets[frameIdx], 0, nullptr);

    const VkViewport viewport
    {
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

    if (m_DebugSphereMeshReady)
        DrawDebugSpheres(cmd, debugSpheres, sceneAddress);

    if (m_DebugSphereMeshReady && ddgiActive && ddgi.showSurfels && m_DDGI.HasSurfels())
        DrawSphereInstances(cmd, m_DDGI.GetSurfelAddress(device, frameIdx),
                            m_DDGI.GetSurfelCount(), sceneAddress);

    vkCmdEndRendering(cmd);
}

}
