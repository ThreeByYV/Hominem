#include "hmnpch.h"
#include "RenderDocCapture.h"

#include <atomic>

#ifdef HMN_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <renderdoc_app.h>
#endif

namespace Hominem {

namespace {

#ifdef HMN_PLATFORM_WINDOWS
RENDERDOC_API_1_6_0* s_API = nullptr;
#endif

std::atomic<uint32_t> s_PendingFrames { 0 };
bool                  s_Capturing = false; // render thread only
std::string           s_LastCapture;

}

void RenderDocCapture::Init()
{
#ifdef HMN_PLATFORM_WINDOWS
    // Already present when launched from the RenderDoc UI. Otherwise, load it ourselves,
    // which has to happen before any graphics context exists or RenderDoc misses the
    // device creation it needs to hook.
    HMODULE mod = GetModuleHandleA("renderdoc.dll");

    // Loading it ourselves is a debug convenience so captures work from the IDE. A release
    // build only picks up an injection that already happened, otherwise a player with
    // RenderDoc installed gets its overlay drawn over the game.
#ifdef HMN_DEBUG
    if (!mod)
    {
        char        overridePath[MAX_PATH] = {};
        const DWORD len = GetEnvironmentVariableA("HOMINEM_RENDERDOC_DLL", overridePath, MAX_PATH);
        mod = (len > 0 && len < MAX_PATH) ? LoadLibraryA(overridePath)
                                          : LoadLibraryA("C:\\Program Files\\RenderDoc\\renderdoc.dll");
    }
#endif

    if (!mod)
    {
        HMN_CORE_INFO("RenderDoc: not present, in-app capture disabled");
        return;
    }

    auto getAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    if (!getAPI || getAPI(eRENDERDOC_API_Version_1_6_0, (void**)&s_API) != 1)
    {
        s_API = nullptr;
        HMN_CORE_WARN("RenderDoc: module loaded but the 1.6 API is unavailable");
        return;
    }

    // '8' instead of the default F12/PrtScrn, which collide with laptop media keys.
    RENDERDOC_InputButton captureKey = eRENDERDOC_Key_8;
    s_API->SetCaptureKeys(&captureKey, 1);

    s_API->SetCaptureFilePathTemplate("captures/hominem");
    HMN_CORE_INFO("RenderDoc: capture ready, press 8, files land in captures/");
#endif
}

bool RenderDocCapture::IsAvailable()
{
#ifdef HMN_PLATFORM_WINDOWS
    return s_API != nullptr;
#else
    return false;
#endif
}

void RenderDocCapture::TriggerNextFrame(uint32_t frames)
{
    if (!IsAvailable() || frames == 0) return;
    s_PendingFrames.store(frames, std::memory_order_release);
}

void RenderDocCapture::BeginFrame()
{
#ifdef HMN_PLATFORM_WINDOWS
    if (!s_API || s_Capturing) return;
    if (s_PendingFrames.load(std::memory_order_acquire) == 0) return;

    // Null device and window wildcard onto whatever API owns the window. That resolves to
    // GL here; the Vulkan renderer is headless and lands in its own capture if asked for.
    s_API->StartFrameCapture(nullptr, nullptr);
    s_Capturing = true;
#endif
}

void RenderDocCapture::EndFrame()
{
#ifdef HMN_PLATFORM_WINDOWS
    if (!s_API || !s_Capturing) return;

    const uint32_t ok = s_API->EndFrameCapture(nullptr, nullptr);
    s_Capturing = false;
    s_PendingFrames.fetch_sub(1, std::memory_order_acq_rel);

    if (!ok)
    {
        HMN_CORE_WARN("RenderDoc: capture failed");
        return;
    }

    const uint32_t count = s_API->GetNumCaptures();
    if (count == 0) return;

    char     path[1024] = {};
    uint32_t pathLen    = sizeof(path);
    if (s_API->GetCapture(count - 1, path, &pathLen, nullptr))
    {
        s_LastCapture = path;
        HMN_CORE_INFO("RenderDoc: captured {0}", s_LastCapture);
    }
#endif
}

const std::string& RenderDocCapture::GetLastCapturePath()
{
    return s_LastCapture;
}

}
