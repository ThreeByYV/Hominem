#pragma once

#include "Hominem/Renderer/SharedResources.h"

#include <array>
#include <cstdint>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

class OpenGLSharedResources : public SharedResources
{
public:
    void ImportSharedTexture(HANDLE memHandle, uint64_t memSize, uint32_t w, uint32_t h) override;
    void ImportSemaphore(uint32_t frameIdx, HANDLE semHandle) override;
    void ImportGLDoneSemaphore(HANDLE semHandle) override;
    void WaitSemaphore(uint32_t frameIdx) override;
    void SignalGLDone() override;
    void Destroy() override;

    uint32_t GetTextureID() const override { return m_Texture; }

    static std::array<uint8_t, 8> GetDeviceLUID();
    static std::string             GetDeviceName();

private:
    uint32_t m_MemObject       = 0;
    uint32_t m_Texture         = 0;
    uint32_t m_Semaphores[2]   = {};
    uint32_t m_GLDoneSemaphore = 0;
};

}
