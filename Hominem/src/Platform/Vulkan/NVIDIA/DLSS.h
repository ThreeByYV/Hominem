#pragma once

#include "Platform/Vulkan/IUpscalerStrategy.h"

#include <atomic>
#include <string>
#include <vector>

struct NVSDK_NGX_Handle;

namespace Hominem {

/// NVIDIA DLSS Super Resolution through NGX. Quality mode comes from RenderSettings::DLSSQuality.
/// Never available when built without HMN_ENABLE_DLSS.
class DLSS final : public IUpscalerStrategy
{
public:
    /// NGX lives as long as the Vulkan device, so VulkanRenderer drives these.
    static std::vector<std::string> InstanceExtensions();
    static std::vector<std::string> DeviceExtensions(VkInstance instance, VkPhysicalDevice physical);
    static void InitRuntime(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    static void ShutdownRuntime();

    explicit DLSS(VkDevice device) : m_Device(device) {}
    ~DLSS() override;

    const char*  GetName() const override { return "DLSS"; }
    bool         IsAvailable() const override;
    bool         IsEnabled() const override;
    UpscalerCaps GetCaps() const override;
    float        GetRenderScale(glm::uvec2 outputSize) const override;

    void PollDebugKeys() override; // Ctrl+Alt+1 presses the dev DLL's Ctrl+Alt+F12 overlay cycle
    bool Evaluate(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge, const UpscalerInputs& in) override;

private:
    // Render sizes NGX accepts for one output size and quality mode.
    struct Optimal
    {
        glm::uvec2 output{ 0 };
        int        quality = -1;
        glm::uvec2 optimal{ 0 }, min{ 0 }, max{ 0 };
    };

    const Optimal& QueryOptimal(glm::uvec2 output, int quality);
    void           ReleaseFeature();

    VkDevice          m_Device  = VK_NULL_HANDLE;
    NVSDK_NGX_Handle* m_Feature = nullptr;
    glm::uvec2        m_FeatureRender{ 0 }, m_FeatureOutput{ 0 };
    int               m_FeatureQuality = -1;
    Optimal           m_Optimal;
    bool              m_WarnedRange = false;

    bool     m_DebugKeyDown = false;
    uint16_t m_HeldFKey     = 0;
    int      m_HeldFrames   = 0;

    // NGX's render size for one output size and quality, packed into one word so the main
    // thread never reads a mix of two render-thread queries. 0 = none yet.
    std::atomic<uint64_t> m_ScaleState{ 0 };
};

}
