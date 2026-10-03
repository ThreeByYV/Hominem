#include "hmnpch.h"
#include "DLSS.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanUpscalerBridge.h"
#include "Hominem/Renderer/Frame/RenderSettings.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"

#ifdef HMN_ENABLE_DLSS
    #include <nvsdk_ngx_vk.h>
    #include <nvsdk_ngx_helpers.h>
    #include <nvsdk_ngx_helpers_vk.h>
    #include <filesystem>
#endif

namespace Hominem {

namespace {

// NVIDIA's standard ratios, indexed by RenderSettings::DLSSQuality (dlaa, quality, balanced,
// performance, ultraperformance). Used until NGX has been asked for the exact size.
constexpr float k_DefaultScales[] = { 1.f, 2.f / 3.f, 0.58f, 0.5f, 1.f / 3.f };

int QualityIndex()
{
    return std::clamp(RenderSettings::DLSSQuality, 0, (int)std::size(k_DefaultScales) - 1);
}

// [63:48] output width | [47:32] output height | [31:16] NGX render width | [3:0] quality + 1
uint64_t PackScale(glm::uvec2 output, int quality, uint32_t renderWidth)
{
    return (uint64_t)output.x << 48 | (uint64_t)output.y << 32 | (uint64_t)renderWidth << 16
         | (uint64_t)(quality + 1);
}

}

bool DLSS::IsEnabled() const
{
    return RenderSettings::DLSS;
}

UpscalerCaps DLSS::GetCaps() const
{
    return { .temporal = true, .minRenderScale = 0.33f, .maxRenderScale = 1.f, .mipBiasOffset = -1.f };
}

float DLSS::GetRenderScale(glm::uvec2 outputSize) const
{
    const int      quality = QualityIndex();
    const uint64_t state   = m_ScaleState.load(std::memory_order_acquire);
    const uint32_t renderW = (uint32_t)(state >> 16 & 0xFFFF);

    // Only NGX's answer for this exact output size and quality; otherwise the standard ratio.
    if (renderW && state == PackScale(outputSize, quality, renderW))
        return (float)renderW / (float)outputSize.x;
    return k_DefaultScales[quality];
}

#ifdef HMN_ENABLE_DLSS

namespace {

constexpr const char* k_ProjectId     = "347befbe-aaf2-44f4-a355-a57cd6d494f9";
constexpr const char* k_EngineVersion = "1.0";

constexpr NVSDK_NGX_PerfQuality_Value k_Modes[] =
{
    NVSDK_NGX_PerfQuality_Value_DLAA,
    NVSDK_NGX_PerfQuality_Value_MaxQuality,
    NVSDK_NGX_PerfQuality_Value_Balanced,
    NVSDK_NGX_PerfQuality_Value_MaxPerf,
    NVSDK_NGX_PerfQuality_Value_UltraPerformance,
};
static_assert(std::size(k_Modes) == std::size(k_DefaultScales));

constexpr int k_DebugKeyHoldFrames = 4; // long enough for the DLL to see it held

// Process-wide NGX state, owned by VulkanRenderer through InitRuntime / ShutdownRuntime.
struct
{
    VkDevice             device       = VK_NULL_HANDLE;
    NVSDK_NGX_Parameter* capabilities = nullptr;
    bool                 initialized  = false;
    bool                 available    = false;
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

void SendKey(WORD vk, bool up)
{
    INPUT input{};
    input.type       = INPUT_KEYBOARD;
    input.ki.wVk     = vk;
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &input, sizeof(INPUT));
}

NVSDK_NGX_Resource_VK Resource(const VulkanRenderTarget& target, bool readWrite)
{
    const VkExtent2D extent = target.GetExtent();
    const VkImageSubresourceRange range { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return NVSDK_NGX_Create_ImageView_Resource_VK(target.GetImageView(), target.GetImage(), range,
                                                  target.GetFormat(), extent.width, extent.height, readWrite);
}

}

// --- NGX runtime ---------------------------------------------------------------------------

std::vector<std::string> DLSS::InstanceExtensions()
{
    if (RenderDocAttached()) return {};
    const auto info = DiscoveryInfo();
    uint32_t count = 0;
    VkExtensionProperties* props = nullptr;
    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &props)))
        return {};
    return ToNames(count, props);
}

std::vector<std::string> DLSS::DeviceExtensions(VkInstance instance, VkPhysicalDevice physical)
{
    if (RenderDocAttached()) return {};
    const auto info = DiscoveryInfo();
    uint32_t count = 0;
    VkExtensionProperties* props = nullptr;
    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physical, &info, &count, &props)))
        return {};
    return ToNames(count, props);
}

void DLSS::InitRuntime(VkInstance instance, VkPhysicalDevice physical, VkDevice device)
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
    s_NGX.device      = device;
    s_NGX.initialized = true;

    if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&s_NGX.capabilities)))
    {
        HMN_CORE_WARN("NGX: capability query failed, DLSS unavailable");
        return;
    }

    int available = 0, needsDriver = 0;
    NVSDK_NGX_Parameter_GetI(s_NGX.capabilities, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    NVSDK_NGX_Parameter_GetI(s_NGX.capabilities, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
    if (needsDriver)
        HMN_CORE_WARN("NGX: DLSS needs a newer NVIDIA driver");

    s_NGX.available = available != 0;
    HMN_CORE_INFO("NGX: DLSS Super Resolution {0}", s_NGX.available ? "available" : "unavailable");
}

void DLSS::ShutdownRuntime()
{
    if (!s_NGX.initialized) return;
    if (s_NGX.capabilities)
        NVSDK_NGX_VULKAN_DestroyParameters(s_NGX.capabilities);
    NVSDK_NGX_VULKAN_Shutdown1(s_NGX.device);
    s_NGX = {};
}

// --- Feature -------------------------------------------------------------------------------

DLSS::~DLSS()
{
    if (m_HeldFKey) SendKey(m_HeldFKey, true);
    if (m_Feature) vkDeviceWaitIdle(m_Device);
    ReleaseFeature();
}

bool DLSS::IsAvailable() const
{
    return s_NGX.available;
}

void DLSS::PollDebugKeys()
{
#ifndef HMN_DIST
    if (m_HeldFKey && --m_HeldFrames <= 0)
    {
        SendKey(m_HeldFKey, true);
        m_HeldFKey = 0;
    }

    const bool ctrl = Input::IsKeyPressed(HMN_KEY_LEFT_CONTROL) || Input::IsKeyPressed(HMN_KEY_RIGHT_CONTROL);
    const bool alt  = Input::IsKeyPressed(HMN_KEY_LEFT_ALT)     || Input::IsKeyPressed(HMN_KEY_RIGHT_ALT);

    const bool down = Input::IsKeyPressed(HMN_KEY_1);
    if (down && !m_DebugKeyDown && ctrl && alt && !m_HeldFKey)
    {
        SendKey(VK_F12, false);
        m_HeldFKey   = VK_F12;
        m_HeldFrames = k_DebugKeyHoldFrames;
    }
    m_DebugKeyDown = down;
#endif
}

const DLSS::Optimal& DLSS::QueryOptimal(glm::uvec2 output, int quality)
{
    if (m_Optimal.output == output && m_Optimal.quality == quality) return m_Optimal;

    Optimal o;
    o.output  = output;
    o.quality = quality;
    float sharpness = 0.f;
    NGX_DLSS_GET_OPTIMAL_SETTINGS(s_NGX.capabilities, output.x, output.y, k_Modes[quality],
                                  &o.optimal.x, &o.optimal.y, &o.max.x, &o.max.y,
                                  &o.min.x, &o.min.y, &sharpness);

    if (o.optimal.x == 0 || o.optimal.y == 0)
    {
        // Mode not offered for this output size; fall back to the standard ratio.
        const float s = k_DefaultScales[quality];
        o.optimal = o.min = o.max = { (uint32_t)std::lround(output.x * s), (uint32_t)std::lround(output.y * s) };
    }
    m_Optimal = o;

    m_ScaleState.store(PackScale(output, quality, o.optimal.x), std::memory_order_release);
    HMN_CORE_INFO("DLSS: {0} at {1}x{2} renders {3}x{4} (range {5}x{6} to {7}x{8})",
                  RenderSettings::DLSSQualityNames[quality], output.x, output.y,
                  o.optimal.x, o.optimal.y, o.min.x, o.min.y, o.max.x, o.max.y);
    return m_Optimal;
}

void DLSS::ReleaseFeature()
{
    if (!m_Feature) return;
    NVSDK_NGX_VULKAN_ReleaseFeature(m_Feature);
    m_Feature = nullptr;
}

bool DLSS::Evaluate(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge, const UpscalerInputs& in)
{
    using B = VulkanUpscalerBridge;
    if (!s_NGX.available) return false;

    const glm::uvec2 render  = bridge.RenderSize();
    const glm::uvec2 output  = bridge.OutputSize();
    const int        quality = QualityIndex();

    const Optimal& range = QueryOptimal(output, quality);
    if (glm::any(glm::lessThan(render, range.min)) || glm::any(glm::greaterThan(render, range.max)))
    {
        // A frame rendered before the main thread picked up NGX's size, or a size NGX won't take.
        if (!m_WarnedRange)
            HMN_CORE_WARN("DLSS: render size {0}x{1} outside NGX's range, blitting instead", render.x, render.y);
        m_WarnedRange = true;
        return false;
    }

    if (!m_Feature || render != m_FeatureRender || output != m_FeatureOutput || quality != m_FeatureQuality)
    {
        ReleaseFeature();

        NVSDK_NGX_DLSS_Create_Params create{};
        create.Feature.InWidth            = render.x;
        create.Feature.InHeight           = render.y;
        create.Feature.InTargetWidth      = output.x;
        create.Feature.InTargetHeight     = output.y;
        create.Feature.InPerfQualityValue = k_Modes[quality];
        // HDR: the scene is linear, before tonemapping. Velocity is at render size and built
        // from unjittered matrices. Exposure: DLSS computes its own until we feed ours.
        create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR
                                    | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes
                                    | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

        const NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT(cmd, 1, 1, &m_Feature, s_NGX.capabilities, &create);
        if (NVSDK_NGX_FAILED(result))
        {
            HMN_CORE_ERROR("DLSS: feature creation failed (0x{0:08X}), blitting instead", (uint32_t)result);
            m_Feature = nullptr;
            return false;
        }
        m_FeatureRender  = render;
        m_FeatureOutput  = output;
        m_FeatureQuality = quality;
        HMN_CORE_INFO("DLSS: feature created, {0}x{1} -> {2}x{3}", render.x, render.y, output.x, output.y);
    }

    // DLSS samples its inputs; the output stays GENERAL as a storage image.
    for (B::Image i : { B::Color, B::Velocity, B::Depth })
        VulkanImage::TransitionGeneralToShaderRead(cmd, bridge.Target(i).GetImage());

    NVSDK_NGX_Resource_VK color    = Resource(bridge.Target(B::Color),    false);
    NVSDK_NGX_Resource_VK velocity = Resource(bridge.Target(B::Velocity), false);
    NVSDK_NGX_Resource_VK depth    = Resource(bridge.Target(B::Depth),    false);
    NVSDK_NGX_Resource_VK out      = Resource(bridge.Target(B::Output),   true);

    NVSDK_NGX_VK_DLSS_Eval_Params eval{};
    eval.Feature.pInColor  = &color;
    eval.Feature.pInOutput = &out;
    eval.pInDepth          = &depth;
    eval.pInMotionVectors  = &velocity;

    // The bridge hands DLSS an upright frame (y down), while jitter and velocity are in GL's
    // y-up UV space. Velocity is curr - prev; DLSS wants current -> previous in pixels, so x
    // negates and y, already reversed by the flip, doesn't. Jitter y flips with the image.
    eval.InJitterOffsetX =  in.jitterUV.x * (float)render.x;
    eval.InJitterOffsetY = -in.jitterUV.y * (float)render.y;
    eval.InMVScaleX      = -(float)render.x;
    eval.InMVScaleY      =  (float)render.y;
    eval.InRenderSubrectDimensions = { render.x, render.y };
    eval.InReset         = in.reset ? 1 : 0;

    const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(cmd, m_Feature, s_NGX.capabilities, &eval);

    for (B::Image i : { B::Color, B::Velocity, B::Depth })
        VulkanImage::TransitionShaderReadToGeneral(cmd, bridge.Target(i).GetImage());

    if (NVSDK_NGX_FAILED(result))
    {
        HMN_CORE_ERROR("DLSS: evaluate failed (0x{0:08X}), blitting instead", (uint32_t)result);
        return false;
    }
    return true;
}

#else

std::vector<std::string> DLSS::InstanceExtensions()                           { return {}; }
std::vector<std::string> DLSS::DeviceExtensions(VkInstance, VkPhysicalDevice) { return {}; }
void DLSS::InitRuntime(VkInstance, VkPhysicalDevice, VkDevice)                {}
void DLSS::ShutdownRuntime()                                                   {}

DLSS::~DLSS() = default;
bool DLSS::IsAvailable() const                                                 { return false; }
void DLSS::PollDebugKeys()                                                     {}
void DLSS::ReleaseFeature()                                                    {}
bool DLSS::Evaluate(VkCommandBuffer, const VulkanUpscalerBridge&, const UpscalerInputs&) { return false; }

#endif

}
