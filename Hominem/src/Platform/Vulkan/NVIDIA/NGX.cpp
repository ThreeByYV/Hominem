#include "hmnpch.h"
#include "NGX.h"

#ifdef HMN_ENABLE_DLSS

#include <nvsdk_ngx_vk.h>
#include <filesystem>

namespace Hominem {

namespace {

constexpr const char* k_ProjectId     = "347befbe-aaf2-44f4-a355-a57cd6d494f9";
constexpr const char* k_EngineVersion = "1.0";

// Every NGX feature the engine uses; their extensions are requested together.
constexpr NVSDK_NGX_Feature k_Features[] = { NVSDK_NGX_Feature_SuperSampling };

struct
{
    VkDevice             device       = VK_NULL_HANDLE;
    NVSDK_NGX_Parameter* capabilities = nullptr;
    bool                 initialized  = false;
} s_NGX;

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

NVSDK_NGX_FeatureCommonInfo* CommonInfo()
{
    static const std::wstring    dllDir  = std::filesystem::path(HMN_DLSS_DLL_DIR).make_preferred().wstring();
    static const wchar_t*        paths[] = { dllDir.c_str() };
    static NVSDK_NGX_FeatureCommonInfo info = []
    {
        NVSDK_NGX_FeatureCommonInfo i{};
        i.PathListInfo.Path   = paths;
        i.PathListInfo.Length = 1;
        // NGX prints ~80 lines to stdout when logging is on, so it's opt-in: HOMINEM_NGX_LOG=1
        // (also writes log files to DataPath()).
        char optIn[2] = {};
        const bool verbose = GetEnvironmentVariableA("HOMINEM_NGX_LOG", optIn, sizeof(optIn)) == 1 && optIn[0] == '1';
        i.LoggingInfo.MinimumLoggingLevel = verbose ? NVSDK_NGX_LOGGING_LEVEL_ON : NVSDK_NGX_LOGGING_LEVEL_OFF;
        return i;
    }();
    return &info;
}

NVSDK_NGX_FeatureDiscoveryInfo DiscoveryInfo(NVSDK_NGX_Feature feature)
{
    NVSDK_NGX_FeatureDiscoveryInfo info{};
    info.SDKVersion                             = NVSDK_NGX_Version_API;
    info.FeatureID                              = feature;
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

// Union of every feature's list; `query` fills one feature's.
template<typename Query>
std::vector<std::string> CollectExtensions(Query query)
{
    std::vector<std::string> names;
    if (RenderDocAttached()) return names;

    for (NVSDK_NGX_Feature feature : k_Features)
    {
        const auto info = DiscoveryInfo(feature);
        uint32_t count = 0;
        VkExtensionProperties* props = nullptr;
        if (NVSDK_NGX_FAILED(query(info, count, props))) continue;

        for (uint32_t i = 0; i < count; i++)
            if (std::ranges::find(names, props[i].extensionName) == names.end())
                names.emplace_back(props[i].extensionName);
    }
    return names;
}

}

std::vector<std::string> NGX::InstanceExtensions()
{
    return CollectExtensions([](const NVSDK_NGX_FeatureDiscoveryInfo& info, uint32_t& count, VkExtensionProperties*& props)
    {
        return NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &props);
    });
}

std::vector<std::string> NGX::DeviceExtensions(VkInstance instance, VkPhysicalDevice physical)
{
    return CollectExtensions([&](const NVSDK_NGX_FeatureDiscoveryInfo& info, uint32_t& count, VkExtensionProperties*& props)
    {
        return NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physical, &info, &count, &props);
    });
}

void NGX::Init(VkInstance instance, VkPhysicalDevice physical, VkDevice device)
{
    if (RenderDocAttached())
    {
        HMN_CORE_WARN("NGX: RenderDoc is attached, NVIDIA features unavailable (NVIDIA does not support them under RenderDoc)");
        return;
    }

    // volk loads Vulkan at runtime, so NGX gets the loader entry points rather than linking them.
    const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        k_ProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, k_EngineVersion, DataPath().c_str(),
        instance, physical, device, vkGetInstanceProcAddr, vkGetDeviceProcAddr, CommonInfo());
    if (NVSDK_NGX_FAILED(result))
    {
        HMN_CORE_WARN("NGX: init failed (0x{0:08X}), NVIDIA features unavailable", (uint32_t)result);
        return;
    }
    s_NGX.device      = device;
    s_NGX.initialized = true;

    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&s_NGX.capabilities)))
    {
        HMN_CORE_WARN("NGX: capability query failed, NVIDIA features unavailable");
        s_NGX.capabilities = nullptr;
    }
}

void NGX::Shutdown()
{
    if (!s_NGX.initialized) return;
    if (s_NGX.capabilities)
        NVSDK_NGX_VULKAN_DestroyParameters(s_NGX.capabilities);
    NVSDK_NGX_VULKAN_Shutdown1(s_NGX.device);
    s_NGX = {};
}

bool NGX::IsInitialized()
{
    return s_NGX.initialized;
}

NVSDK_NGX_Parameter* NGX::Capabilities()
{
    return s_NGX.capabilities;
}

bool NGX::IsFeatureAvailable(const char* availableParam, const char* needsDriverParam, const char* name)
{
    if (!s_NGX.capabilities) return false;

    int available = 0, needsDriver = 0;
    NVSDK_NGX_Parameter_GetI(s_NGX.capabilities, availableParam, &available);
    NVSDK_NGX_Parameter_GetI(s_NGX.capabilities, needsDriverParam, &needsDriver);
    if (needsDriver)
        HMN_CORE_WARN("NGX: {0} needs a newer NVIDIA driver", name);

    HMN_CORE_INFO("NGX: {0} {1}", name, available ? "available" : "unavailable");
    return available != 0;
}

}

#else

namespace Hominem {

std::vector<std::string> NGX::InstanceExtensions()                           { return {}; }
std::vector<std::string> NGX::DeviceExtensions(VkInstance, VkPhysicalDevice) { return {}; }
void                 NGX::Init(VkInstance, VkPhysicalDevice, VkDevice)       {}
void                 NGX::Shutdown()                                          {}
bool                 NGX::IsInitialized()                                     { return false; }
NVSDK_NGX_Parameter* NGX::Capabilities()                                      { return nullptr; }
bool                 NGX::IsFeatureAvailable(const char*, const char*, const char*) { return false; }

}

#endif
