#pragma once

#include "VulkanUpscalerBridge.h"

#include "Hominem/Core/Ref.h"
#include "Hominem/Renderer/RHI/Shader.h"
#include "Hominem/Renderer/PostProcess/Upscaling/Upscaler.h"

namespace Hominem {

/// An upscaler that runs in Vulkan while the rest of the frame is GL: copies the scene into
/// the bridge, runs RecordUpscale mid-frame, and copies the result into OutputTarget.
class VulkanBridgedUpscaler : public Upscaler
{
public:
    VulkanBridgedUpscaler(VulkanRenderer& vk, SharedResources& gl) : m_Vulkan(vk), m_GL(gl) {}
    ~VulkanBridgedUpscaler() override;

    void Init() override;
    void DeclareResources(RenderGraph& graph) override;
    void BeginFrame(RenderGraph&) override {}
    void Evaluate(RenderGraph& graph, const UpscalerInputs& in, CommandList& cmd) final;

protected:
    /// Records the Vulkan side. Every bridge image is in VK_IMAGE_LAYOUT_GENERAL.
    virtual void RecordUpscale(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge,
                               const UpscalerInputs& in) = 0;

    VulkanRenderer&  m_Vulkan;
    SharedResources& m_GL;

private:
    VulkanUpscalerBridge m_Bridge;
    Ref<Shader>          m_DepthCopyShader;
};

}
