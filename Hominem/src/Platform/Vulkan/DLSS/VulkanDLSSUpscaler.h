#pragma once

#include "Platform/Vulkan/VulkanBridgedUpscaler.h"

#include <atomic>

struct NVSDK_NGX_Handle;

namespace Hominem {

/// NVIDIA DLSS Super Resolution through the GL<->Vulkan bridge. Quality mode comes from
/// RenderSettings::DLSSQuality. Every NGX call happens on the render thread.
class VulkanDLSSUpscaler final : public VulkanBridgedUpscaler
{
public:
    using VulkanBridgedUpscaler::VulkanBridgedUpscaler;
    ~VulkanDLSSUpscaler() override;

    const char*  GetName() const override { return "DLSS"; }
    bool         IsSupported() const override;
    UpscalerCaps GetCaps() const override;
    float        GetRenderScale(float requested, glm::uvec2 outputSize) const override;

    void Init() override;
    void BeginFrame(RenderGraph& graph) override;

protected:
    void RecordUpscale(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge, const UpscalerInputs& in) override;

private:
    // Render sizes NGX accepts for one output size and quality mode.
    struct Optimal
    {
        glm::uvec2 output{ 0 };
        int        quality = -1;
        glm::uvec2 optimal{ 0 }, min{ 0 }, max{ 0 };
    };

    const Optimal& QueryOptimal(glm::uvec2 output, int quality); // render thread
    void           ReleaseFeature();

    NVSDK_NGX_Handle* m_Feature = nullptr;
    glm::uvec2        m_FeatureRender{ 0 }, m_FeatureOutput{ 0 };
    int               m_FeatureQuality = -1;
    Optimal           m_Optimal;
    bool              m_WarnedRange = false;

    // Ctrl+Alt+1 presses the dev DLL's Ctrl+Alt+F12 overlay cycle (main thread).
    void     PollDebugKeys();
    bool     m_DebugKeyDown = false;
    uint16_t m_HeldFKey    = 0;
    int      m_HeldFrames  = 0;

    // NGX's render size for one output size and quality, packed into one word so the main
    // thread (GetRenderScale) never reads a mix of two render-thread queries. 0 = none yet.
    std::atomic<uint64_t> m_ScaleState{ 0 };
};

}
