#pragma once

#include "VulkanCore.h"
#include "VulkanImage.h"

#include <array>
#include <cstdint>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hominem {

struct QueueFamilyIndices
{
    int32_t graphicsFamily = -1;
    bool IsComplete() const { return graphicsFamily >= 0; }
};

class VulkanRenderer
{
public:
    VulkanRenderer() = default;

    void Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID = {}, std::string preferredName = {});
    void Shutdown();

    VkCommandBuffer BeginFrame();
    void            PrepareComputeOnDrawImage();
    void            EndFrame();

    VkDevice         GetDevice()        const { return m_Device; }
    VkPhysicalDevice GetPhysical()      const { return m_PhysicalDevice; }
    VkInstance       GetInstance()      const { return m_Instance; }
    VmaAllocator     GetAllocator()     const { return m_Allocator; }
    VkQueue          GetGraphicsQueue() const { return m_GraphicsQueue; }

    VkImageView  GetDrawImageView()    const { return m_DrawImageView; }
    VkExtent2D   GetDrawExtent()       const { return m_DrawExtent; }

    uint32_t     GetCurrentFrameIndex() const { return m_CurrentFrame; }
    bool         IsFrameInProgress()    const { return m_FrameStarted; }

    HANDLE       GetDrawImageWin32Handle();
    HANDLE       GetComputeDoneSemaphoreWin32Handle(uint32_t frameIdx);
    VkDeviceSize GetDrawImageMemorySize()          const { return m_DrawImageMemorySize; }

    std::array<uint8_t, 8> GetDeviceLUID() const;

    DeletionQueue& GetFrameDeletionQueue() { return m_Frames[m_CurrentFrame].deletionQueue; }

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice(std::array<uint8_t, 8> preferredLUID, const std::string& preferredName);
    void CreateLogicalDevice();
    void InitVMA();
    void CreateDrawImage(uint32_t w, uint32_t h);
    void CreateCommandStructures();
    void CreateSyncObjects();

    uint32_t           FindMemoryType    (uint32_t typeBits, VkMemoryPropertyFlags props) const;
    QueueFamilyIndices FindQueueFamilies (VkPhysicalDevice dev)                           const;
    bool               IsDeviceSuitable  (VkPhysicalDevice dev)                           const;

    static std::array<uint8_t, 8> GetPhysicalDeviceLUID(VkPhysicalDevice dev);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT        type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* userdata);

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameData
    {
        VkCommandPool   cmdPool     = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer   = VK_NULL_HANDLE;
        VkSemaphore     computeDone = VK_NULL_HANDLE;
        VkFence         inFlight    = VK_NULL_HANDLE;
        DeletionQueue   deletionQueue;
    };

    VkInstance               m_Instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice                 m_Device         = VK_NULL_HANDLE;
    VkQueue                  m_GraphicsQueue  = VK_NULL_HANDLE;
    uint32_t                 m_GraphicsFamily = ~0u;

    VmaAllocator m_Allocator = VK_NULL_HANDLE;

    VkImage        m_DrawImageRaw      = VK_NULL_HANDLE;
    VkImageView    m_DrawImageView     = VK_NULL_HANDLE;
    VkDeviceMemory m_DrawImageMemory   = VK_NULL_HANDLE;
    VkDeviceSize   m_DrawImageMemorySize = 0;
    VkExtent2D     m_DrawExtent        = {};

    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames;

    uint32_t m_CurrentFrame          = 0;
    bool     m_FrameStarted          = false;
    bool     m_DrawImageInShaderRead = false;

    DeletionQueue m_MainDeletionQueue;
};

}
