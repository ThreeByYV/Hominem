#pragma once

#include "Hominem/Core/Core.h"

#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <functional>
#include <deque>

#define VK_CHECK(expr) \
    do { VkResult __vkr = (expr); \
         HMN_CORE_ASSERT(__vkr == VK_SUCCESS, "Vulkan error {0}", (int)__vkr); \
    } while(0)

namespace Hominem {

struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(std::move(function));
    }

    void flush()
    {
        for (auto it = deletors.rbegin(); it != deletors.rend(); ++it)
            (*it)();
        deletors.clear();
    }
};

}
