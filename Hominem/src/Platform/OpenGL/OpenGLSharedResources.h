#pragma once

#include <array>
#include <cstdint>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <volk.h>

namespace Hominem {

class OpenGLSharedResources
{
public:
    void ImportSharedTexture(HANDLE memHandle, VkDeviceSize memSize, uint32_t w, uint32_t h);
    void ImportSemaphore(uint32_t frameIdx, HANDLE semHandle);
    void WaitSemaphore(uint32_t frameIdx);
    void Destroy();

    uint32_t GetTextureID() const { return m_Texture; }

    static std::array<uint8_t, 8> GetDeviceLUID();
    static std::string             GetDeviceName();

private:
    uint32_t m_MemObject     = 0;
    uint32_t m_Texture       = 0;
    uint32_t m_Semaphores[2] = {};
};

}
