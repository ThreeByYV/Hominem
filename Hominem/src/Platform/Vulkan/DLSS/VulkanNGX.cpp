#include "hmnpch.h"
#include "VulkanNGX.h"

#ifdef HMN_ENABLE_DLSS
    #include <nvsdk_ngx_vk.h>
    #include <filesystem>
#endif

namespace Hominem {

#ifdef HMN_ENABLE_DLSS

namespace {

constexpr const char* k_ProjectId     = "347befbe-aaf2-44f4-a355-a57cd6d494f9";
constexpr const char* k_EngineVersion = "1.0";

// NGX holds on to these pointers, so they live for the whole process.
const std::wstring& DataPath()
{
    static const std::wstring path = []
    {
        const auto dir = std::filesystem::current_path() / "NGX";
        std::filesystem::create_directories(dir);
        return dir.wstring();
    }();
    return path;
}

void NVSDK_CONV LogFromNGX(const char* message, NVSDK_NGX_Logging_Level, NVSDK_NGX_Feature)
{
    std::string_view line(message);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.remove_suffix(1);
    HMN_CORE_TRACE("NGX: {0}", line);
}

NVSDK_NGX_FeatureCommonInfo* CommonInfo()
{
    static const std::wstring    dllDir  = std::filesystem::path(HMN_DLSS_DLL_DIR).make_preferred().wstring();
    static const wchar_t*        paths[] = { dllDir.c_str() };
    static NVSDK_NGX_FeatureCommonInfo info = []
    {
        NVSDK_NGX_FeatureCommonInfo i{};
        i.PathListInfo.Path   = paths;
        i.PathListInfo.Length = 1;
        i.LoggingInfo.LoggingCallback     = &LogFromNGX;
        i.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
        return i;
    }();
    return &info;
}

NVSDK_NGX_FeatureDiscoveryInfo DiscoveryInfo()
{
    NVSDK_NGX_FeatureDiscoveryInfo info{};
    info.SDKVersion                             = NVSDK_NGX_Version_API;
    info.FeatureID                              = NVSDK_NGX_Feature_SuperSampling;
    info.Identifier.IdentifierType              = NVSDK_NGX_Application_Identifier_Type_Project_Id;
    info.Identifier.v.ProjectDesc.ProjectId     = k_ProjectId;
    info.Identifier.v.ProjectDesc.EngineType    = NVSDK_NGX_ENGINE_TYPE_CUSTOM;
    info.Identifier.v.ProjectDesc.EngineVersion = k_EngineVersion;
    info.ApplicationDataPath                    = DataPath().c_str();
    info.FeatureInfo                            = CommonInfo();
    return info;
}

// RenderDoc hooks Vulkan in a way NGX rejects (programming guide 8.6).
bool RenderDocAttached()
{
    return GetModuleHandleA("renderdoc.dll") != nullptr;
}

std::vector<std::string> ToNames(uint32_t count, const VkExtensionProperties* props)
{
    std::vector<std::string> names;
    for (uint32_t i = 0; i < count; i++)
        names.emplace_back(props[i].extensionName);
    return names;
}

}

std::vector<std::string> VulkanNGX::InstanceExtensions()
{
    if (RenderDocAttached()) return {};
    const auto info = DiscoveryInfo();
    uint32_t count = 0;
    VkExtensionProperties* props = nullptr;
    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &props)))
        return {};
    return ToNames(count, props);
}

std::vector<std::string> VulkanNGX::DeviceExtensions(VkInstance instance, VkPhysicalDevice physical)
{
    if (RenderDocAttached()) return {};
    const auto info = DiscoveryInfo();
    uint32_t count = 0;
    VkExtensionProperties* props = nullptr;
    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physical, &info, &count, &props)))
        return {};
    return ToNames(count, props);
}

void VulkanNGX::Init(VkInstance instance, VkPhysicalDevice physical, VkDevice device)
{
    if (RenderDocAttached())
    {
        HMN_CORE_WARN("NGX: RenderDoc is attached, DLSS unavailable (NVIDIA does not support DLSS under RenderDoc)");
        return;
    }

    // volk loads Vulkan at runtime, so NGX gets the loader entry points rather than linking them.
    const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        k_ProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, k_EngineVersion, DataPath().c_str(),
        instance, physical, device, vkGetInstanceProcAddr, vkGetDeviceProcAddr, CommonInfo());
    if (NVSDK_NGX_FAILED(result))
    {
        HMN_CORE_WARN("NGX: init failed (0x{0:08X}), DLSS unavailable", (uint32_t)result);
        return;
    }
    m_Device      = device;
    m_Initialized = true;

    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_Capabilities)))
    {
        HMN_CORE_WARN("NGX: capability query failed, DLSS unavailable");
        return;
    }

    int available = 0, needsDriver = 0;
    NVSDK_NGX_Parameter_GetI(m_Capabilities, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    NVSDK_NGX_Parameter_GetI(m_Capabilities, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
    if (needsDriver)
        HMN_CORE_WARN("NGX: DLSS needs a newer NVIDIA driver");

    m_SuperSamplingAvailable = available != 0;
    HMN_CORE_INFO("NGX: DLSS Super Resolution {0}", m_SuperSamplingAvailable ? "available" : "unavailable");
}

void VulkanNGX::Shutdown()
{
    if (!m_Initialized) return;
    if (m_Capabilities)
        NVSDK_NGX_VULKAN_DestroyParameters(m_Capabilities);
    NVSDK_NGX_VULKAN_Shutdown1(m_Device);

    m_Capabilities           = nullptr;
    m_Initialized            = false;
    m_SuperSamplingAvailable = false;
}

#else

std::vector<std::string> VulkanNGX::InstanceExtensions()                         { return {}; }
std::vector<std::string> VulkanNGX::DeviceExtensions(VkInstance, VkPhysicalDevice) { return {}; }
void VulkanNGX::Init(VkInstance, VkPhysicalDevice, VkDevice)                      {}
void VulkanNGX::Shutdown()                                                         {}

#endif

}
