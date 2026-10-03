#pragma once

#include "Platform/Vulkan/VulkanCore.h"

#include <string>
#include <vector>

struct NVSDK_NGX_Parameter;

namespace Hominem {

/// NVIDIA's NGX runtime, shared by every NGX feature (DLSS Super Resolution today). Lives as
/// long as the Vulkan device, so VulkanRenderer drives Init / Shutdown. A no-op when built
/// without HMN_ENABLE_DLSS.
class NGX
{
public:
    /// Extensions every feature in k_Features needs; add them before creating the instance / device.
    static std::vector<std::string> InstanceExtensions();
    static std::vector<std::string> DeviceExtensions(VkInstance instance, VkPhysicalDevice physical);

    static void Init(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    static void Shutdown();

    static bool                 IsInitialized();
    static NVSDK_NGX_Parameter* Capabilities(); // null until Init succeeds

    /// Reads a feature's availability from the capabilities, e.g.
    /// NVSDK_NGX_Parameter_SuperSampling_Available / _NeedsUpdatedDriver. Logs the result.
    static bool IsFeatureAvailable(const char* availableParam, const char* needsDriverParam, const char* name);
};

}
