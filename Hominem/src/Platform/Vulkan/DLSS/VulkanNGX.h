#pragma once

#include "Platform/Vulkan/VulkanCore.h"

#include <string>
#include <vector>

struct NVSDK_NGX_Parameter;

namespace Hominem {

/// NVIDIA NGX setup shared by the DLSS features. A no-op when built without HMN_ENABLE_DLSS.
class VulkanNGX
{
public:
    /// Extensions NGX needs; add them before creating the instance / device.
    static std::vector<std::string> InstanceExtensions();
    static std::vector<std::string> DeviceExtensions(VkInstance instance, VkPhysicalDevice physical);

    void Init(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    void Shutdown();

    bool                 IsSuperSamplingAvailable() const { return m_SuperSamplingAvailable; }
    NVSDK_NGX_Parameter* GetCapabilities() const          { return m_Capabilities; }

private:
    VkDevice             m_Device       = VK_NULL_HANDLE;
    NVSDK_NGX_Parameter* m_Capabilities = nullptr;
    bool                 m_Initialized  = false;
    bool                 m_SuperSamplingAvailable = false;
};

}
