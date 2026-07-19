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
    static constexpr uint32_t kFramesInFlight = 2;

    VulkanRenderer() = default;

    void Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID = {}, std::string preferredName = {});
    void Shutdown();

    VkCommandBuffer BeginFrame();
    void            PrepareComputeOnDrawImage();
    void            PrepareGraphicsOnDrawImage();
    void            EndFrame();

    VkDevice         GetDevice()        const { return m_Device; }
    VkPhysicalDevice GetPhysical()      const { return m_PhysicalDevice; }
    VkInstance       GetInstance()      const { return m_Instance; }
    VmaAllocator     GetAllocator()     const { return m_Allocator; }
    VkQueue          GetGraphicsQueue() const { return m_GraphicsQueue; }

    VkImageView  GetDrawImageView()    const { return m_DrawImage.view; }
    VkExtent2D   GetDrawExtent()       const { return m_DrawExtent; }
    VkImageView  GetDepthImageView()   const { return m_DepthImage.imageView; }
    VkFormat     GetDrawImageFormat()  const { return VK_FORMAT_R16G16B16A16_SFLOAT; }
    VkFormat     GetDepthImageFormat() const { return VK_FORMAT_D32_SFLOAT; }
    bool         DrawImageHasComputeOutput() const { return m_DrawImage.layout == DrawImageLayout::General; }

    uint32_t     GetCurrentFrameIndex() const { return m_CurrentFrame; }
    bool         IsFrameInProgress()    const { return m_FrameStarted; }

    HANDLE       GetDrawImageWin32Handle();
    HANDLE       GetComputeDoneSemaphoreWin32Handle(uint32_t frameIdx);
    HANDLE       GetGLDoneSemaphoreWin32Handle();
    VkDeviceSize GetDrawImageMemorySize()          const { return m_DrawImageMemorySize; }

    std::array<uint8_t, 8> GetDeviceLUID() const;

    DeletionQueue& GetFrameDeletionQueue() { return m_Frames[m_CurrentFrame].deletionQueue; }
    DeletionQueue& GetMainDeletionQueue()  { return m_MainDeletionQueue; }

private:
    enum class DrawImageLayout { Undefined, General, ColorAttachment, ShaderRead };

    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice(std::array<uint8_t, 8> preferredLUID, const std::string& preferredName);
    void CreateLogicalDevice();
    void InitVMA();
    void CreateDrawImage(uint32_t w, uint32_t h);
    void CreateDepthImage(uint32_t w, uint32_t h);
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

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = kFramesInFlight;

    struct DrawImage
    {
        VkImage         image  = VK_NULL_HANDLE;
        VkImageView     view   = VK_NULL_HANDLE;
        VkDeviceMemory  memory = VK_NULL_HANDLE;
        DrawImageLayout layout = DrawImageLayout::Undefined;
    };

    struct FrameData
    {
        VkCommandPool   cmdPool       = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer     = VK_NULL_HANDLE;
        VkSemaphore     computeDone   = VK_NULL_HANDLE;
        uint64_t        timelineValue = 0;
        DeletionQueue   deletionQueue;
    };

    VkInstance               m_Instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice                 m_Device         = VK_NULL_HANDLE;
    VkQueue                  m_GraphicsQueue  = VK_NULL_HANDLE;
    uint32_t                 m_GraphicsFamily = ~0u;

    VmaAllocator m_Allocator = VK_NULL_HANDLE;

    DrawImage      m_DrawImage;
    VkSemaphore    m_GLDoneSemaphore   = VK_NULL_HANDLE;
    bool           m_GLDonePending     = false;
    VkDeviceSize   m_DrawImageMemorySize = 0;
    VkExtent2D     m_DrawExtent        = {};

    VulkanAllocatedImage m_DepthImage = {};

    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames;

    VkSemaphore m_TimelineSemaphore = VK_NULL_HANDLE;
    uint64_t    m_TimelineValue     = 0;

    uint32_t        m_CurrentFrame    = 0;
    bool            m_FrameStarted    = false;

    DeletionQueue m_MainDeletionQueue;
};

}
