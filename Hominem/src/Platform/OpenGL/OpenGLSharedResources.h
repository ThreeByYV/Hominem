#pragma once

#include "Hominem/Renderer/RHI/SharedResources.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

class OpenGLSharedResources : public SharedResources
{
public:
    void ImportSharedTexture(HANDLE memHandle, uint64_t memSize, uint32_t w, uint32_t h) override;

    uint32_t ImportSharedImage(const SharedImageDesc& desc) override;
    void     ReleaseSharedImage(uint32_t texID) override;

    void ImportSemaphore(uint32_t frameIdx, HANDLE semHandle) override;
    void ImportGLDoneSemaphore(HANDLE semHandle) override;
    void WaitSemaphore(uint32_t frameIdx) override;
    void SignalGLDone() override;

    uint32_t ImportSemaphoreHandle(HANDLE semHandle) override;
    void     DeleteSemaphore(uint32_t semaphore) override;
    void     WaitOn(uint32_t semaphore, std::span<const uint32_t> textures) override;
    void     SignalOn(uint32_t semaphore, std::span<const uint32_t> textures) override;
    void     CopyTexture(uint32_t src, uint32_t dst, uint32_t width, uint32_t height) override;
    void Destroy() override;

    uint32_t GetTextureID() const override { return m_Texture; }

    static std::array<uint8_t, 8> GetDeviceLUID();
    static std::string             GetDeviceName();
    static bool                    IsInteropSupported();

private:
    // Every image imported from Vulkan, with the GL layout token matching the layout
    // Vulkan leaves it in. Wait/SignalSemaphore must name all of them: the semaphore
    // carries the ownership transfer for exactly the textures listed.
    struct ImportedImage
    {
        uint32_t memObject = 0;
        uint32_t texture   = 0;
        uint32_t layout    = 0;
        bool     frameSync = true;
    };

    void RebuildSyncLists();
    std::vector<uint32_t> LayoutsFor(std::span<const uint32_t> textures) const;

    std::vector<ImportedImage> m_Images;
    std::vector<uint32_t>      m_SyncTextures;  // rebuilt on import/release, used every frame
    std::vector<uint32_t>      m_SyncLayouts;

    uint32_t m_MemObject       = 0;
    uint32_t m_Texture         = 0;
    uint32_t m_Semaphores[2]   = {};
    uint32_t m_GLDoneSemaphore = 0;
};

}
