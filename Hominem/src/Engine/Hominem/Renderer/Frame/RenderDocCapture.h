#pragma once

#include <cstdint>
#include <string>

namespace Hominem {

class RenderDocCapture
{
public:
    /// Call before the graphics context exists. Picks up an already-injected
    /// renderdoc.dll, or loads one from HOMINEM_RENDERDOC_DLL / the default install path.
    static void Init();

    static bool IsAvailable();

    /// Capture the next `frames` presented frames. Safe to call from any thread.
    static void TriggerNextFrame(uint32_t frames = 1);

    /// Bracket one frame. Called by RenderThread; not intended for use elsewhere.
    static void BeginFrame();
    static void EndFrame();

    /// Path of the most recent capture, empty if none has been taken.
    static const std::string& GetLastCapturePath();
};

}
