#include "hmnpch.h"
#include "VulkanDLSSUpscaler.h"

#ifdef HMN_ENABLE_DLSS

#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanImage.h"
#include "Hominem/Renderer/Frame/RenderSettings.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"

#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

namespace Hominem {

namespace {

// Indexed by RenderSettings::DLSSQuality (dlaa, quality, balanced, performance, ultraperformance).
constexpr NVSDK_NGX_PerfQuality_Value k_Modes[] =
{
    NVSDK_NGX_PerfQuality_Value_DLAA,
    NVSDK_NGX_PerfQuality_Value_MaxQuality,
    NVSDK_NGX_PerfQuality_Value_Balanced,
    NVSDK_NGX_PerfQuality_Value_MaxPerf,
    NVSDK_NGX_PerfQuality_Value_UltraPerformance,
};

// NVIDIA's standard ratios, used until NGX has been asked for the exact size.
constexpr float k_DefaultScales[] = { 1.f, 2.f / 3.f, 0.58f, 0.5f, 1.f / 3.f };

int QualityIndex()
{
    return std::clamp(RenderSettings::DLSSQuality, 0, (int)std::size(k_Modes) - 1);
}

// [63:48] output width | [47:32] output height | [31:16] NGX render width | [3:0] quality + 1
uint64_t PackScale(glm::uvec2 output, int quality, uint32_t renderWidth)
{
    return (uint64_t)output.x << 48 | (uint64_t)output.y << 32 | (uint64_t)renderWidth << 16
         | (uint64_t)(quality + 1);
}

// The dev DLL only takes Ctrl+Alt+F-key hotkeys, map the overlay cycle (F12) to Ctrl+Alt+1
constexpr int k_DebugKeyHoldFrames = 4; // long enough for the DLL to see it held

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

VulkanDLSSUpscaler::~VulkanDLSSUpscaler()
{
    if (m_HeldFKey) SendKey(m_HeldFKey, true);
    vkDeviceWaitIdle(m_Vulkan.GetDevice());
    ReleaseFeature();
}

void VulkanDLSSUpscaler::Init()
{
    VulkanBridgedUpscaler::Init();
#ifndef HMN_DIST
    HMN_CORE_INFO("DLSS debug: Ctrl+Alt+1 cycles the debug overlay");
#endif
}

void VulkanDLSSUpscaler::BeginFrame(RenderGraph& graph)
{
    VulkanBridgedUpscaler::BeginFrame(graph);
#ifndef HMN_DIST
    PollDebugKeys();
#endif
}

void VulkanDLSSUpscaler::PollDebugKeys()
{
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
}

bool VulkanDLSSUpscaler::IsSupported() const
{
    return m_Vulkan.GetNGX().IsSuperSamplingAvailable();
}

UpscalerCaps VulkanDLSSUpscaler::GetCaps() const
{
    if (!RenderSettings::DLSS)
        return { .temporal = false, .minRenderScale = 1.f, .maxRenderScale = 1.f };
    return { .temporal = true, .minRenderScale = 0.33f, .maxRenderScale = 1.f, .mipBiasOffset = -1.f };
}

float VulkanDLSSUpscaler::GetRenderScale(float, glm::uvec2 outputSize) const
{
    if (!RenderSettings::DLSS) return 1.f;

    const int      quality = QualityIndex();
    const uint64_t state   = m_ScaleState.load(std::memory_order_acquire);
    const uint32_t renderW = (uint32_t)(state >> 16 & 0xFFFF);

    // Only NGX's answer for this exact output size and quality; otherwise the standard ratio.
    if (renderW && state == PackScale(outputSize, quality, renderW))
        return (float)renderW / (float)outputSize.x;
    return k_DefaultScales[quality];
}

const VulkanDLSSUpscaler::Optimal& VulkanDLSSUpscaler::QueryOptimal(glm::uvec2 output, int quality)
{
    if (m_Optimal.output == output && m_Optimal.quality == quality) return m_Optimal;

    Optimal o;
    o.output  = output;
    o.quality = quality;
    float sharpness = 0.f;

    NGX_DLSS_GET_OPTIMAL_SETTINGS(
            m_Vulkan.GetNGX().GetCapabilities(),
            output.x,
            output.y, k_Modes[quality],
            &o.optimal.x,
            &o.optimal.y,
            &o.max.x,
            &o.max.y,
            &o.min.x,
            &o.min.y, &sharpness
     );

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

void VulkanDLSSUpscaler::ReleaseFeature()
{
    if (!m_Feature) return;
    NVSDK_NGX_VULKAN_ReleaseFeature(m_Feature);
    m_Feature = nullptr;
}

void VulkanDLSSUpscaler::RecordUpscale(VkCommandBuffer cmd, const VulkanUpscalerBridge& bridge,
                                       const UpscalerInputs& in)
{
    using B = VulkanUpscalerBridge;
    NVSDK_NGX_Parameter* params  = m_Vulkan.GetNGX().GetCapabilities();
    const glm::uvec2     render  = bridge.RenderSize();
    const glm::uvec2     output  = bridge.OutputSize();
    const int            quality = QualityIndex();

    // Off: the frame as rendered, no AA. The feature stays alive for switching back.
    if (!RenderSettings::DLSS)
    {
        bridge.RecordBlit(cmd);
        return;
    }

    const Optimal& range = QueryOptimal(output, quality);
    if (!params || glm::any(glm::lessThan(render, range.min)) || glm::any(glm::greaterThan(render, range.max)))
    {
        // A frame rendered before the main thread picked up NGX's size, or a size NGX won't take.
        if (!m_WarnedRange)
            HMN_CORE_WARN("DLSS: render size {0}x{1} outside NGX's range, blitting instead", render.x, render.y);
        m_WarnedRange = true;
        bridge.RecordBlit(cmd);
        return;
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

        const NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT(cmd, 1, 1, &m_Feature, params, &create);
        if (NVSDK_NGX_FAILED(result))
        {
            HMN_CORE_ERROR("DLSS: feature creation failed (0x{0:08X}), blitting instead", (uint32_t)result);
            m_Feature = nullptr;
            bridge.RecordBlit(cmd);
            return;
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

    const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(cmd, m_Feature, params, &eval);

    for (B::Image i : { B::Color, B::Velocity, B::Depth })
        VulkanImage::TransitionShaderReadToGeneral(cmd, bridge.Target(i).GetImage());

    if (NVSDK_NGX_FAILED(result))
    {
        HMN_CORE_ERROR("DLSS: evaluate failed (0x{0:08X}), blitting instead", (uint32_t)result);
        bridge.RecordBlit(cmd);
    }
}

}

#endif
