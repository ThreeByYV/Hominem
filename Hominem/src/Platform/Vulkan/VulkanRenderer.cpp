#include "hmnpch.h"
#include "VulkanRenderer.h"

#include <set>

namespace Hominem {

static const std::vector<const char*> k_ValidationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> k_DeviceExtensions =
{
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
};

#ifdef HMN_DEBUG
static constexpr bool k_EnableValidation = true;
#else
static constexpr bool k_EnableValidation = false;
#endif

void VulkanRenderer::Init(uint32_t w, uint32_t h, std::array<uint8_t, 8> preferredLUID, std::string preferredName)
{
    CreateInstance();
    if (k_EnableValidation)
        SetupDebugMessenger();
    PickPhysicalDevice(preferredLUID, preferredName);
    CreateLogicalDevice();
    InitVMA();
    CreateDrawImage(w, h);
    CreateCommandStructures();
    CreateSyncObjects();

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
    HMN_CORE_INFO("Vulkan headless renderer initialised: {0}", props.deviceName);
}

void VulkanRenderer::Shutdown()
{
    vkDeviceWaitIdle(m_Device);

    for (auto& f : m_Frames)
        f.deletionQueue.flush();

    vkDestroyImageView(m_Device, m_DrawImageView, nullptr);
    vkDestroyImage    (m_Device, m_DrawImageRaw,  nullptr);
    vkFreeMemory      (m_Device, m_DrawImageMemory, nullptr);

    m_MainDeletionQueue.flush();
}

VkCommandBuffer VulkanRenderer::BeginFrame()
{
    HMN_CORE_ASSERT(!m_FrameStarted, "BeginFrame called while a frame is already in progress");

    FrameData& frame = m_Frames[m_CurrentFrame];

    const VkSemaphoreWaitInfo waitInfo
    {
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores    = &m_TimelineSemaphore,
        .pValues        = &frame.timelineValue,
    };
    VK_CHECK(vkWaitSemaphores(m_Device, &waitInfo, UINT64_MAX));

    frame.deletionQueue.flush();
    vkResetCommandPool(m_Device, frame.cmdPool, 0);

    const VkCommandBufferBeginInfo beginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(frame.cmdBuffer, &beginInfo));

    m_FrameStarted = true;
    return frame.cmdBuffer;
}

void VulkanRenderer::PrepareComputeOnDrawImage()
{
    HMN_CORE_ASSERT(m_FrameStarted, "PrepareComputeOnDrawImage called outside a frame");
    VkCommandBuffer cmd = m_Frames[m_CurrentFrame].cmdBuffer;
    if (m_DrawImageInShaderRead)
        VulkanImage::TransitionShaderReadToGeneral(cmd, m_DrawImageRaw);
    else
        VulkanImage::TransitionUndefinedToGeneral(cmd, m_DrawImageRaw);
}

void VulkanRenderer::EndFrame()
{
    HMN_CORE_ASSERT(m_FrameStarted, "EndFrame called without a matching BeginFrame");

    FrameData& frame = m_Frames[m_CurrentFrame];
    VulkanImage::TransitionGeneralToShaderRead(frame.cmdBuffer, m_DrawImageRaw);
    m_DrawImageInShaderRead = true;

    VK_CHECK(vkEndCommandBuffer(frame.cmdBuffer));

    ++m_TimelineValue;
    frame.timelineValue = m_TimelineValue;

    const VkSemaphore signalSemaphores[] = { frame.computeDone, m_TimelineSemaphore };
    const uint64_t    signalValues[]     = { 0, m_TimelineValue }; // ignored for the binary computeDone semaphore

    const VkTimelineSemaphoreSubmitInfo timelineInfo
    {
        .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 2,
        .pSignalSemaphoreValues    = signalValues,
    };
    const VkSubmitInfo submitInfo
    {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &timelineInfo,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &frame.cmdBuffer,
        .signalSemaphoreCount = 2,
        .pSignalSemaphores    = signalSemaphores,
    };
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    m_FrameStarted = false;
    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

std::array<uint8_t, 8> VulkanRenderer::GetDeviceLUID() const
{
    return GetPhysicalDeviceLUID(m_PhysicalDevice);
}

HANDLE VulkanRenderer::GetDrawImageWin32Handle()
{
    const VkMemoryGetWin32HandleInfoKHR info
    {
        .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
        .memory     = m_DrawImageMemory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    HANDLE handle = nullptr;
    VK_CHECK(vkGetMemoryWin32HandleKHR(m_Device, &info, &handle));
    return handle;
}

HANDLE VulkanRenderer::GetComputeDoneSemaphoreWin32Handle(uint32_t frameIdx)
{
    const VkSemaphoreGetWin32HandleInfoKHR info
    {
        .sType      = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore  = m_Frames[frameIdx].computeDone,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    HANDLE handle = nullptr;
    VK_CHECK(vkGetSemaphoreWin32HandleKHR(m_Device, &info, &handle));
    return handle;
}

void VulkanRenderer::CreateInstance()
{
    VK_CHECK(volkInitialize());

    if (k_EnableValidation)
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> available(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, available.data());

        for (const char* name : k_ValidationLayers)
        {
            bool found = false;
            for (const auto& layer : available)
                if (strcmp(name, layer.layerName) == 0) { found = true; break; }
            HMN_CORE_ASSERT(found, "Validation layer not available: {0}", name);
        }
    }

    const VkApplicationInfo appInfo
    {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Hominem",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "Hominem Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_3,
    };

    std::vector<const char*> extensions;
    if (k_EnableValidation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const VkInstanceCreateInfo createInfo
    {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = k_EnableValidation ? (uint32_t)k_ValidationLayers.size() : 0u,
        .ppEnabledLayerNames     = k_EnableValidation ? k_ValidationLayers.data() : nullptr,
        .enabledExtensionCount   = (uint32_t)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));
    volkLoadInstance(m_Instance);

    m_MainDeletionQueue.push_function([this]() { vkDestroyInstance(m_Instance, nullptr); });
}

void VulkanRenderer::SetupDebugMessenger()
{
    const VkDebugUtilsMessengerCreateInfoEXT createInfo
    {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
    };
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger));
    m_MainDeletionQueue.push_function([this]()
    {
        vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
    });
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        HMN_CORE_ERROR("Vulkan: {0}", data->pMessage);
    else
        HMN_CORE_WARN("Vulkan: {0}", data->pMessage);
    return VK_FALSE;
}

QueueFamilyIndices VulkanRenderer::FindQueueFamilies(VkPhysicalDevice dev) const
{
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = (int32_t)i;
            break;
        }
    }
    return indices;
}

bool VulkanRenderer::IsDeviceSuitable(VkPhysicalDevice dev) const
{
    if (!FindQueueFamilies(dev).IsComplete()) return false;

    uint32_t extCount;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> available(extCount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, available.data());

    std::set<std::string> required(k_DeviceExtensions.begin(), k_DeviceExtensions.end());
    for (const auto& ext : available)
        required.erase(ext.extensionName);
    return required.empty();
}

std::array<uint8_t, 8> VulkanRenderer::GetPhysicalDeviceLUID(VkPhysicalDevice dev)
{
    VkPhysicalDeviceIDProperties idProps { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
    VkPhysicalDeviceProperties2  props2  { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                           .pNext = &idProps };
    vkGetPhysicalDeviceProperties2(dev, &props2);
    if (!idProps.deviceLUIDValid) return {};
    std::array<uint8_t, 8> luid;
    memcpy(luid.data(), idProps.deviceLUID, 8);
    return luid;
}

void VulkanRenderer::PickPhysicalDevice(std::array<uint8_t, 8> preferredLUID, const std::string& preferredName)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
    HMN_CORE_ASSERT(count > 0, "No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

    if (preferredLUID != std::array<uint8_t, 8>{})
    {
        for (auto dev : devices)
        {
            if (!IsDeviceSuitable(dev)) continue;
            if (GetPhysicalDeviceLUID(dev) == preferredLUID)
            {
                m_PhysicalDevice = dev;
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(dev, &props);
                HMN_CORE_INFO("Vulkan: matched GL adapter by LUID {0}", props.deviceName);
                return;
            }
        }
        HMN_CORE_WARN("Vulkan: no device matches GL adapter LUID, trying name match");
    }

    if (!preferredName.empty())
    {
        for (const auto dev : devices)
        {
            if (!IsDeviceSuitable(dev)) continue;
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (preferredName.find(props.deviceName) != std::string::npos ||
                std::string(props.deviceName).find(preferredName) != std::string::npos)
            {
                m_PhysicalDevice = dev;
                HMN_CORE_INFO("Vulkan: matched GL adapter by name {0}", props.deviceName);
                return;
            }
        }
        HMN_CORE_WARN("Vulkan: no device matches GL adapter name '{0}', falling back to best discrete", preferredName);
    }

    for (auto dev : devices)
    {
        if (!IsDeviceSuitable(dev)) continue;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            m_PhysicalDevice = dev;
            break;
        }
    }
    if (m_PhysicalDevice == VK_NULL_HANDLE)
    {
        for (auto dev : devices)
            if (IsDeviceSuitable(dev)) { m_PhysicalDevice = dev; break; }
    }
    HMN_CORE_ASSERT(m_PhysicalDevice != VK_NULL_HANDLE, "Failed to find a suitable GPU");
}

void VulkanRenderer::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
    m_GraphicsFamily = (uint32_t)indices.graphicsFamily;

    float priority = 1.0f;
    const VkDeviceQueueCreateInfo queueInfo
    {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_GraphicsFamily,
        .queueCount       = 1,
        .pQueuePriorities = &priority,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynRendering
    {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSem
    {
        .sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .pNext             = &dynRendering,
        .timelineSemaphore = VK_TRUE,
    };

    VkPhysicalDeviceSynchronization2Features sync2
    {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext            = &timelineSem,
        .synchronization2 = VK_TRUE,
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bda
    {
        .sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext               = &sync2,
        .bufferDeviceAddress = VK_TRUE,
    };

    const VkDeviceCreateInfo createInfo
    {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &bda,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &queueInfo,
        .enabledExtensionCount   = (uint32_t)k_DeviceExtensions.size(),
        .ppEnabledExtensionNames = k_DeviceExtensions.data(),
    };
    VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device));
    volkLoadDevice(m_Device);

    vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0, &m_GraphicsQueue);

    m_MainDeletionQueue.push_function([this]() { vkDestroyDevice(m_Device, nullptr); });
}

void VulkanRenderer::InitVMA()
{
    VmaVulkanFunctions vmaFuncs{};
    vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFuncs.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    const VmaAllocatorCreateInfo allocInfo
    {
        .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice   = m_PhysicalDevice,
        .device           = m_Device,
        .pVulkanFunctions = &vmaFuncs,
        .instance         = m_Instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    VK_CHECK(vmaCreateAllocator(&allocInfo, &m_Allocator));
    m_MainDeletionQueue.push_function([this]() { vmaDestroyAllocator(m_Allocator); });
}

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    HMN_CORE_ASSERT(false, "Failed to find suitable memory type");
    return ~0u;
}

void VulkanRenderer::CreateDrawImage(uint32_t w, uint32_t h)
{
    m_DrawExtent = { w, h };

    const VkExternalMemoryImageCreateInfo extMemInfo
    {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkImageCreateInfo imageInfo
    {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext         = &extMemInfo,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R16G16B16A16_SFLOAT,
        .extent        = { w, h, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_STORAGE_BIT          |
                         VK_IMAGE_USAGE_SAMPLED_BIT          |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT     |
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VK_CHECK(vkCreateImage(m_Device, &imageInfo, nullptr, &m_DrawImageRaw));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Device, m_DrawImageRaw, &memReqs);
    m_DrawImageMemorySize = memReqs.size;

    const VkExportMemoryAllocateInfo exportInfo
    {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkMemoryAllocateInfo allocInfo
    {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &exportInfo,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    VK_CHECK(vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_DrawImageMemory));
    VK_CHECK(vkBindImageMemory(m_Device, m_DrawImageRaw, m_DrawImageMemory, 0));

    const VkImageViewCreateInfo viewInfo
    {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image    = m_DrawImageRaw,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = VK_FORMAT_R16G16B16A16_SFLOAT,
        .subresourceRange =
        {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    VK_CHECK(vkCreateImageView(m_Device, &viewInfo, nullptr, &m_DrawImageView));
}

void VulkanRenderer::CreateCommandStructures()
{
    for (auto& f : m_Frames)
    {
        const VkCommandPoolCreateInfo poolInfo
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = m_GraphicsFamily,
        };
        VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &f.cmdPool));

        const VkCommandBufferAllocateInfo allocInfo
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = f.cmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocInfo, &f.cmdBuffer));

        m_MainDeletionQueue.push_function([this, &f]()
        {
            vkDestroyCommandPool(m_Device, f.cmdPool, nullptr);
        });
    }
}

void VulkanRenderer::CreateSyncObjects()
{
    const VkExportSemaphoreCreateInfo exportSemInfo
    {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkSemaphoreCreateInfo semInfo
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &exportSemInfo,
    };

    const VkSemaphoreTypeCreateInfo timelineTypeInfo
    {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = 0,
    };
    const VkSemaphoreCreateInfo timelineSemInfo
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineTypeInfo,
    };
    VK_CHECK(vkCreateSemaphore(m_Device, &timelineSemInfo, nullptr, &m_TimelineSemaphore));
    m_MainDeletionQueue.push_function([this]()
    {
        vkDestroySemaphore(m_Device, m_TimelineSemaphore, nullptr);
    });

    for (auto& f : m_Frames)
    {
        VK_CHECK(vkCreateSemaphore(m_Device, &semInfo, nullptr, &f.computeDone));

        m_MainDeletionQueue.push_function([this, &f]()
        {
            vkDestroySemaphore(m_Device, f.computeDone, nullptr);
        });
    }
}

}
