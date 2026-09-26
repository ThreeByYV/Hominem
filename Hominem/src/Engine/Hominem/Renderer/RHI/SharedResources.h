#pragma once

#include "Hominem/Renderer/RHI/SharedImages.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

class SharedResources
{
public:
    virtual ~SharedResources() = default;

    virtual void ImportSharedTexture(HANDLE memHandle, uint64_t memSize, uint32_t w, uint32_t h) = 0;

    /// Bind another Vulkan image as a GL texture. Returns the GL texture id, or 0 on failure.
    /// Every imported image joins the per-frame semaphore wait/signal set.
    virtual uint32_t ImportSharedImage(const SharedImageDesc& desc) = 0;
    virtual void     ReleaseSharedImage(uint32_t texID) = 0;

    virtual void ImportSemaphore(uint32_t frameIdx, HANDLE semHandle) = 0;
    virtual void ImportGLDoneSemaphore(HANDLE semHandle) = 0;
    virtual void WaitSemaphore(uint32_t frameIdx) = 0;
    virtual void SignalGLDone() = 0;
    virtual void Destroy() = 0;

    virtual uint32_t GetTextureID() const = 0;

    static std::unique_ptr<SharedResources> Create();

    static std::array<uint8_t, 8> GetDeviceLUID();
    static std::string             GetDeviceName();

    static bool IsInteropSupported();
};

}
